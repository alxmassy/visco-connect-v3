#include "NetworkInterfaceManager.h"
#include "Logger.h"
#include <QDebug>

NetworkInterfaceManager::NetworkInterfaceManager(QObject *parent)
    : QObject(parent)
    , m_monitorTimer(new QTimer(this))
    , m_lastWireGuardState(false)
    , m_monitoring(false)
{
    m_monitorTimer->setSingleShot(false);
    m_monitorTimer->setInterval(MONITOR_INTERVAL_MS);
    connect(m_monitorTimer, &QTimer::timeout, this, &NetworkInterfaceManager::checkInterfaces);
    
    // Initialize with current interfaces
    updateInterfaceList();
}

NetworkInterfaceManager::~NetworkInterfaceManager()
{
    stopMonitoring();
}

void NetworkInterfaceManager::startMonitoring()
{
    if (m_monitoring) return;
    
    m_monitoring = true;
    updateInterfaceList();
    m_monitorTimer->start();
    
    LOG_INFO("Started network interface monitoring", "NetworkInterfaceManager");
}

void NetworkInterfaceManager::stopMonitoring()
{
    if (!m_monitoring) return;
    
    m_monitoring = false;
    m_monitorTimer->stop();
    
    LOG_INFO("Stopped network interface monitoring", "NetworkInterfaceManager");
}

bool NetworkInterfaceManager::isMonitoring() const
{
    return m_monitoring;
}

QList<QNetworkInterface> NetworkInterfaceManager::getAllInterfaces() const
{
    return QNetworkInterface::allInterfaces();
}

QList<QNetworkInterface> NetworkInterfaceManager::getActiveInterfaces() const
{
    QList<QNetworkInterface> activeInterfaces;
    const auto interfaces = QNetworkInterface::allInterfaces();
    
    for (const QNetworkInterface& netInterface : interfaces) {
        if (netInterface.flags() & QNetworkInterface::IsUp &&
            netInterface.flags() & QNetworkInterface::IsRunning &&
            !(netInterface.flags() & QNetworkInterface::IsLoopBack)) {
            activeInterfaces.append(netInterface);
        }
    }
    
    return activeInterfaces;
}

QNetworkInterface NetworkInterfaceManager::getWireGuardInterface() const
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    
    for (const QNetworkInterface& netInterface : interfaces) {
        if (isWireGuardInterface(netInterface)) {
            return netInterface;
        }
    }
    
    return QNetworkInterface();
}

bool NetworkInterfaceManager::hasWireGuardInterface() const
{
    return getWireGuardInterface().isValid();
}

QList<QHostAddress> NetworkInterfaceManager::getAllAddresses() const
{
    QList<QHostAddress> addresses;
    const auto interfaces = getActiveInterfaces();
    
    for (const QNetworkInterface& netInterface : interfaces) {
        const auto entries = netInterface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                addresses.append(entry.ip());
            }
        }
    }
    
    return addresses;
}

QList<QHostAddress> NetworkInterfaceManager::getWireGuardAddresses() const
{
    QList<QHostAddress> addresses;
    const QNetworkInterface wgInterface = getWireGuardInterface();
    
    if (wgInterface.isValid()) {
        const auto entries = wgInterface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                addresses.append(entry.ip());
            }
        }
    }
    
    return addresses;
}

QHostAddress NetworkInterfaceManager::getWireGuardAddress() const
{
    const auto addresses = getWireGuardAddresses();
    return addresses.isEmpty() ? QHostAddress() : addresses.first();
}

QHostAddress NetworkInterfaceManager::getBestLocalAddress(const QHostAddress& destAddress) const
{
    // Try to find an interface directly on the same subnet
    const auto interfaces = getActiveInterfaces();
    QHostAddress wireGuardMatch;
    
    for (const QNetworkInterface& netInterface : interfaces) {
        const auto entries = netInterface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            const QHostAddress& ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback()) {
                
                // Check if destination is in this interface's subnet
                if (destAddress.isInSubnet(ip, entry.prefixLength())) {
                    
                    // Prioritize non-WireGuard interfaces for local traffic
                    if (!isWireGuardInterface(netInterface)) {
                        LOG_INFO(QString("Found direct local interface match for %1: %2 (%3)")
                                 .arg(destAddress.toString())
                                 .arg(ip.toString())
                                 .arg(netInterface.name()), "NetworkInterfaceManager");
                        return ip;
                    }
                    
                    // Keep WireGuard match as fallback
                    if (wireGuardMatch.isNull()) {
                        wireGuardMatch = ip;
                    }
                }
            }
        }
    }
    
    // Use WireGuard match if it's the only one found (e.g. accessing VPN resource)
    if (!wireGuardMatch.isNull()) {
        LOG_INFO(QString("Using WireGuard interface match for %1: %2")
                 .arg(destAddress.toString())
                 .arg(wireGuardMatch.toString()), "NetworkInterfaceManager");
        return wireGuardMatch;
    }
    
    // Default: Let OS decide
    return QHostAddress::Any;
}

bool NetworkInterfaceManager::isWireGuardActive() const
{
    const QNetworkInterface wgInterface = getWireGuardInterface();
    return wgInterface.isValid() && 
           (wgInterface.flags() & QNetworkInterface::IsUp) &&
           (wgInterface.flags() & QNetworkInterface::IsRunning);
}

QString NetworkInterfaceManager::getInterfaceStatus() const
{
    const auto activeInterfaces = getActiveInterfaces();
    const bool wgActive = isWireGuardActive();
    const QHostAddress wgAddress = getWireGuardAddress();
    
    QString status = QString("Active interfaces: %1").arg(activeInterfaces.size());
    
    if (wgActive) {
        status += QString(" | WireGuard: ACTIVE (%1)").arg(wgAddress.toString());
    } else {
        status += " | WireGuard: INACTIVE";
    }
    
    return status;
}

void NetworkInterfaceManager::checkInterfaces()
{
    updateInterfaceList();
}

bool NetworkInterfaceManager::isWireGuardInterface(const QNetworkInterface& netInterface) const
{
    const QString name = netInterface.name().toLower();
    
    // Check for common WireGuard interface patterns
    return name.startsWith("wg") ||              // wg0, wg1, etc.
           name.contains("wireguard") ||         // WireGuard
           name.startsWith("utun") ||            // macOS/iOS WireGuard
           name.contains("tun") ||               // Generic tunnel interface
           netInterface.humanReadableName().toLower().contains("wireguard");
}

void NetworkInterfaceManager::updateInterfaceList()
{
    const auto currentInterfaces = QNetworkInterface::allInterfaces();
    const bool currentWireGuardState = isWireGuardActive();
    
    // Check for interface changes
    if (m_lastInterfaces.size() != currentInterfaces.size()) {
        m_lastInterfaces = currentInterfaces;
        emit interfacesChanged();
        
        LOG_DEBUG(QString("Network interfaces changed: %1 interfaces now active")
                  .arg(getActiveInterfaces().size()), "NetworkInterfaceManager");
    }
    
    // Check for WireGuard state changes
    if (m_lastWireGuardState != currentWireGuardState) {
        m_lastWireGuardState = currentWireGuardState;
        emit wireGuardInterfaceStateChanged(currentWireGuardState);
        
        const QString state = currentWireGuardState ? "ACTIVE" : "INACTIVE";
        const QHostAddress wgAddress = getWireGuardAddress();
        
        LOG_INFO(QString("WireGuard interface state changed: %1 (%2)")
                 .arg(state, wgAddress.toString()), "NetworkInterfaceManager");
    }
}
