#include "WireGuardManager.h"
#include "Logger.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QDebug>
#include <QThread>
#include <QNetworkInterface>
#include <QNetworkInterface>
#include <winsvc.h> 


// Windows service definitions that might not be in older headers
#ifndef SERVICE_CONFIG_SERVICE_SID_INFO
#define SERVICE_CONFIG_SERVICE_SID_INFO 5
#endif

#ifndef SERVICE_SID_TYPE_UNRESTRICTED
#define SERVICE_SID_TYPE_UNRESTRICTED 3
#endif

// Constants
const QString WireGuardManager::CONFIG_DIR_NAME = "WireGuard";
const QString WireGuardManager::CONFIG_FILE_EXTENSION = ".conf";
const int WireGuardManager::STATS_UPDATE_INTERVAL = 1000; // 1 second
const int WireGuardManager::STATUS_CHECK_INTERVAL = 2000; // 2 seconds

WireGuardManager::WireGuardManager(QObject *parent)
    : QObject(parent)
    , m_tunnelDll(nullptr)
    , m_wireguardDll(nullptr)
    , m_generateKeypairFunc(nullptr)
    , m_tunnelServiceFunc(nullptr)
    , m_openAdapterFunc(nullptr)
    , m_closeAdapterFunc(nullptr)
    , m_getConfigurationFunc(nullptr)
    , m_connectionStatus(Disconnected)
    , m_statsTimer(new QTimer(this))
    , m_statusTimer(new QTimer(this))
{
    // Initialize configuration directory
    initializeConfigDirectory();
    
    // Load WireGuard DLLs
    if (!loadDlls()) {
        emit errorOccurred("Failed to load WireGuard DLLs. Please ensure tunnel.dll and wireguard.dll are in the application directory.");
        return;
    }
    
    // Setup timers
    m_statsTimer->setInterval(STATS_UPDATE_INTERVAL);
    m_statusTimer->setInterval(STATUS_CHECK_INTERVAL);
    
    connect(m_statsTimer, &QTimer::timeout, this, &WireGuardManager::updateTransferStats);
    connect(m_statusTimer, &QTimer::timeout, this, &WireGuardManager::checkConnectionStatus);
    
    m_statusTimer->start();
    
    emit logMessage("WireGuard Manager initialized successfully");
}

WireGuardManager::~WireGuardManager()
{
    // Disconnect any active tunnels
    if (m_connectionStatus == Connected || m_connectionStatus == Connecting) {
        disconnectTunnel();
    }
    
    unloadDlls();
}

WireGuardKeypair WireGuardManager::generateKeypair()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_generateKeypairFunc) {
        emit errorOccurred("Key generation function not available");
        return WireGuardKeypair();
    }
    
    BYTE publicKey[32];
    BYTE privateKey[32];
    
    // Initialize arrays to zero
    memset(publicKey, 0, 32);
    memset(privateKey, 0, 32);
    
    // Call the function like the C# implementation does - don't check return value
    // The C# code suggests this function always succeeds and the return value might not be meaningful
    bool result = m_generateKeypairFunc(publicKey, privateKey);
    emit logMessage(QString("WireGuardGenerateKeypair called, returned: %1").arg(result ? "true" : "false"));
    
    // Check if keys were actually generated (not all zeros)
    bool publicKeyValid = false;
    bool privateKeyValid = false;
    
    for (int i = 0; i < 32; ++i) {
        if (publicKey[i] != 0) publicKeyValid = true;
        if (privateKey[i] != 0) privateKeyValid = true;
    }
    
    // Always try to create the keypair like C# does, even if the function returned false
    QString pubKeyStr = base64Encode(QByteArray(reinterpret_cast<char*>(publicKey), 32));
    QString privKeyStr = base64Encode(QByteArray(reinterpret_cast<char*>(privateKey), 32));
    
    emit logMessage(QString("Generated keypair - Public: %1..., Private: %2..., PublicValid: %3, PrivateValid: %4")
                    .arg(pubKeyStr.left(8))
                    .arg(privKeyStr.left(8))
                    .arg(publicKeyValid)
                    .arg(privateKeyValid));
    
    // If both keys appear to be invalid (all zeros), that's a real problem
    if (!publicKeyValid && !privateKeyValid) {
        emit errorOccurred("Generated keys appear to be invalid (all zeros) - this may indicate a DLL issue");
        return WireGuardKeypair();
    }
    
    return WireGuardKeypair(pubKeyStr, privKeyStr);
}

QString WireGuardManager::generatePublicKey(const QString& privateKey)
{
    // Don't generate a new keypair - this would create a different public key!
    // Instead, we should derive the public key from the provided private key.
    // However, WireGuard's tunnel.dll doesn't expose a function to derive public from private.
    // 
    // For now, if we have both keys in a configuration, we should use the existing public key.
    // The proper solution would be to implement Curve25519 public key derivation or 
    // use WireGuard's key derivation functions directly.
    
    emit logMessage("generatePublicKey called - this should not be used for existing configurations");
    
    // Return empty string to indicate we cannot derive the public key
    // The calling code should handle this by using the stored public key instead
    return QString();
}

bool WireGuardManager::saveConfig(const WireGuardConfig& config)
{
    if (!isValidConfig(config)) {
        emit errorOccurred("Invalid WireGuard configuration");
        return false;
    }
    
    QString configPath = QDir(m_configDirectory).filePath(config.interfaceConfig.name + CONFIG_FILE_EXTENSION);
    
    if (writeConfigFile(configPath, config)) {
        emit logMessage(QString("Saved WireGuard configuration: %1").arg(config.interfaceConfig.name));
        emit configurationChanged();
        return true;
    }
    
    emit errorOccurred(QString("Failed to save configuration: %1").arg(config.interfaceConfig.name));
    return false;
}

WireGuardConfig WireGuardManager::loadConfig(const QString& configName)
{
    QString configPath = QDir(m_configDirectory).filePath(configName + CONFIG_FILE_EXTENSION);
    return readConfigFile(configPath);
}

QStringList WireGuardManager::getAvailableConfigs()
{
    QDir configDir(m_configDirectory);
    QStringList configs;
    
    QStringList filters;
    filters << "*" + CONFIG_FILE_EXTENSION;
    
    QFileInfoList files = configDir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo& file : files) {
        configs << file.baseName();
    }
    
    return configs;
}

bool WireGuardManager::deleteConfig(const QString& configName)
{
    QString configPath = QDir(m_configDirectory).filePath(configName + CONFIG_FILE_EXTENSION);
    
    if (QFile::remove(configPath)) {
        emit logMessage(QString("Deleted WireGuard configuration: %1").arg(configName));
        emit configurationChanged();
        return true;
    }
    
    emit errorOccurred(QString("Failed to delete configuration: %1").arg(configName));
    return false;
}

bool WireGuardManager::connectTunnel(const QString& configName)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_connectionStatus == Connected || m_connectionStatus == Connecting) {
        emit errorOccurred("A tunnel is already connected or connecting");
        return false;
    }

    // Support loading external config paths directly
    QString pathToUse;
    QString configKey;
    QFileInfo info(configName);
    if (info.isAbsolute() && info.exists()) {
        pathToUse = configName;
        configKey = info.baseName();
    } else {
        configKey = configName;
        pathToUse = QDir(m_configDirectory).filePath(configKey + CONFIG_FILE_EXTENSION);
    }

    if (!QFile::exists(pathToUse)) {
        emit errorOccurred(QString("Configuration file not found: %1").arg(pathToUse));
        return false;
    }
    
    // Check if DLLs are available
    if (!isDllsAvailable()) {
        emit errorOccurred("WireGuard DLLs are not available. Please ensure tunnel.dll and wireguard.dll are in the application directory.");
        return false;
    }
    
    m_connectionStatus = Connecting;
    emit connectionStatusChanged(m_connectionStatus);
    emit logMessage(QString("Connecting to WireGuard tunnel: %1").arg(configKey));
    
    QString serviceName = generateServiceName(configKey);
      try {
        // Attempt service creation with retry logic for race condition handling
        bool serviceCreated = false;
        int createAttempts = 0;
        const int maxCreateAttempts = 2;
        
        while (createAttempts < maxCreateAttempts && !serviceCreated) {
            createAttempts++;
            emit logMessage(QString("Attempting to create tunnel service (attempt %1/%2)").arg(createAttempts).arg(maxCreateAttempts));
            
            if (createTunnelService(pathToUse, serviceName)) {
                serviceCreated = true;
                emit logMessage(QString("Successfully created tunnel service on attempt %1").arg(createAttempts));
            } else {
                emit logMessage(QString("Failed to create tunnel service on attempt %1").arg(createAttempts));
                if (createAttempts < maxCreateAttempts) {
                    emit logMessage("Waiting 1 second before retry...");
                    QThread::msleep(1000); // Wait 1 second before retry
                }
            }
        }
        
        if (serviceCreated) {
            if (startTunnelService(serviceName)) {
                m_currentConfigName = configKey;
                m_currentServiceName = serviceName;
                m_connectionStatus = Connected;
                m_statsTimer->start();
                
                emit connectionStatusChanged(m_connectionStatus);
                emit logMessage(QString("Successfully connected to WireGuard tunnel: %1").arg(configKey));
                return true;
            } else {
                removeTunnelService(serviceName);
                emit logMessage(QString("Failed to start tunnel service for: %1").arg(configKey));
            }
        } else {
            emit logMessage(QString("Failed to create tunnel service after %1 attempts for: %2").arg(maxCreateAttempts).arg(configKey));
        }
    } catch (...) {
        emit errorOccurred(QString("Exception occurred while connecting to tunnel: %1").arg(configKey));
        removeTunnelService(serviceName);
    }
    
    m_connectionStatus = Error;
    emit connectionStatusChanged(m_connectionStatus);
    emit errorOccurred(QString("Failed to connect to WireGuard tunnel: %1").arg(configKey));
    return false;
}

bool WireGuardManager::disconnectTunnel(const QString& configName)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_connectionStatus == Disconnected) {
        return true;
    }
    
    m_connectionStatus = Disconnecting;
    emit connectionStatusChanged(m_connectionStatus);
    
    QString targetConfigName = configName.isEmpty() ? m_currentConfigName : configName;
    emit logMessage(QString("Disconnecting WireGuard tunnel: %1").arg(targetConfigName));
    
    m_statsTimer->stop();
    
    if (!m_currentServiceName.isEmpty()) {
        stopTunnelService(m_currentServiceName);
        removeTunnelService(m_currentServiceName);
    }
    
    m_currentConfigName.clear();
    m_currentServiceName.clear();
    m_connectionStatus = Disconnected;
    
    emit connectionStatusChanged(m_connectionStatus);
    emit transferStatsUpdated(0, 0);
    emit logMessage(QString("Disconnected WireGuard tunnel: %1").arg(targetConfigName));
    
    return true;
}

WireGuardManager::ConnectionStatus WireGuardManager::getConnectionStatus() const
{
    return m_connectionStatus;
}

QString WireGuardManager::getCurrentConfigName() const
{
    return m_currentConfigName;
}

QString WireGuardManager::getCurrentTunnelIp() const
{
    // 1. Try internal state if we own the connection
    if (!m_currentConfigName.isEmpty()) {
        WireGuardConfig config = const_cast<WireGuardManager*>(this)->loadConfig(m_currentConfigName);
        if (!config.interfaceConfig.addresses.isEmpty()) {
            QString addr = config.interfaceConfig.addresses.first();
            // Remove CIDR suffix if present (e.g., 10.0.0.2/32 -> 10.0.0.2)
            int slashIdx = addr.indexOf('/');
            if (slashIdx != -1) {
                addr = addr.left(slashIdx);
            }
            return addr;
        }
    }
    
    // 2. Fallback: Search all system interfaces for any WireGuard-like interface
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    // Log available interfaces for debugging
    QStringList interfaceNames;
    for (const QNetworkInterface& iface : interfaces) {
        interfaceNames << QString("%1 (%2)").arg(iface.humanReadableName(), iface.name());
    }
    // const_cast to emit logMessage from const method
    const_cast<WireGuardManager*>(this)->emit logMessage(QString("Detected Network Interfaces: %1").arg(interfaceNames.join(", ")));
    
    for (const QNetworkInterface& iface : interfaces) {
        QString name = iface.name().toLower();
        QString humanName = iface.humanReadableName().toLower();
        
        // Robust check for WireGuard interfaces (similar to NetworkInterfaceManager)
        bool isWireGuard = name.startsWith("wg") || 
                           name.contains("wireguard") || 
                           name.contains("tun") ||
                           humanName.contains("wireguard");

        if (isWireGuard) {
            QList<QNetworkAddressEntry> entries = iface.addressEntries();
            for (const QNetworkAddressEntry& entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().isLoopback()) {
                    QString ip = entry.ip().toString();
                    const_cast<WireGuardManager*>(this)->emit logMessage(QString("Found active WireGuard interface '%1' with IP: %2").arg(iface.humanReadableName(), ip));
                    return ip;
                }
            }
        }
    }
    
    return QString();
}


WireGuardConfig WireGuardManager::parseConfigFile(const QString& filePath)
{
    return readConfigFile(filePath);
}

QString WireGuardManager::configToString(const WireGuardConfig& config)
{
    QString configStr;
    QTextStream stream(&configStr);    // Interface section
    stream << "[Interface]" << Qt::endl;
    if (!config.interfaceConfig.privateKey.isEmpty()) {
        stream << "PrivateKey = " << config.interfaceConfig.privateKey << Qt::endl;
    }
    // Note: PublicKey is NOT written to [Interface] section - it's derived from PrivateKey
    if (!config.interfaceConfig.addresses.isEmpty()) {
        stream << "Address = " << config.interfaceConfig.addresses.join(", ") << Qt::endl;
    }
    if (!config.interfaceConfig.dns.isEmpty()) {
        stream << "DNS = " << config.interfaceConfig.dns.join(", ") << Qt::endl;
    }
    if (config.interfaceConfig.listenPort > 0) {
        stream << "ListenPort = " << config.interfaceConfig.listenPort << Qt::endl;
    }
    if (config.interfaceConfig.mtu > 0) {
        stream << "MTU = " << config.interfaceConfig.mtu << Qt::endl;
    }
    
    // Peer sections
    for (const WireGuardPeer& peer : config.interfaceConfig.peers) {
        stream << Qt::endl << "[Peer]" << Qt::endl;
        if (!peer.publicKey.isEmpty()) {
            stream << "PublicKey = " << peer.publicKey << Qt::endl;
        }
        if (!peer.presharedKey.isEmpty()) {
            stream << "PresharedKey = " << peer.presharedKey << Qt::endl;
        }
        if (!peer.endpoint.isEmpty()) {
            stream << "Endpoint = " << peer.endpoint << Qt::endl;
        }
        if (!peer.allowedIPs.isEmpty()) {
            stream << "AllowedIPs = " << peer.allowedIPs.join(", ") << Qt::endl;
        }
        if (peer.persistentKeepalive > 0) {
            stream << "PersistentKeepalive = " << peer.persistentKeepalive << Qt::endl;
        }
    }
    
    return configStr;
}

bool WireGuardManager::isValidConfig(const WireGuardConfig& config)
{
    // Basic validation
    if (config.interfaceConfig.name.isEmpty() || config.interfaceConfig.privateKey.isEmpty()) {
        return false;
    }
    
    if (config.interfaceConfig.peers.isEmpty()) {
        return false;
    }
    
    for (const WireGuardPeer& peer : config.interfaceConfig.peers) {
        if (peer.publicKey.isEmpty() || peer.allowedIPs.isEmpty()) {
            return false;
        }
    }
    
    return true;
}

WireGuardInterface WireGuardManager::getAdapterInfo(const QString& adapterName)
{
    WireGuardInterface info;
    info.name = adapterName;
    
    if (!m_openAdapterFunc || !m_getConfigurationFunc || !m_closeAdapterFunc) {
        return info;
    }
    
    HANDLE adapter = m_openAdapterFunc(reinterpret_cast<LPCWSTR>(adapterName.utf16()));
    if (adapter == INVALID_HANDLE_VALUE) {
        return info;
    }
    
    DWORD configSize = 1024;
    QByteArray configData(configSize, 0);
    
    if (m_getConfigurationFunc(adapter, reinterpret_cast<BYTE*>(configData.data()), &configSize)) {
        // Parse the configuration data here
        // This would require parsing the binary structure returned by WireGuard
        // For now, we'll just return basic info
    }
    
    m_closeAdapterFunc(adapter);
    return info;
}

QPair<uint64_t, uint64_t> WireGuardManager::getTransferStats()
{
    if (m_connectionStatus != Connected || m_currentConfigName.isEmpty()) {
        return qMakePair(0ULL, 0ULL);
    }
    
    WireGuardInterface info = getAdapterInfo(m_currentConfigName);
    uint64_t rx = 0, tx = 0;
    
    for (const WireGuardPeer& peer : info.peers) {
        rx += peer.rxBytes;
        tx += peer.txBytes;
    }
    
    return qMakePair(rx, tx);
}

QString WireGuardManager::formatBytes(uint64_t bytes)
{
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
}

bool WireGuardManager::isDllsAvailable()
{
    return (m_tunnelDll != nullptr && m_wireguardDll != nullptr &&
            m_generateKeypairFunc != nullptr && m_tunnelServiceFunc != nullptr &&
            m_openAdapterFunc != nullptr && m_closeAdapterFunc != nullptr &&
            m_getConfigurationFunc != nullptr);
}

QString WireGuardManager::getConfigDirectory() const
{
    return m_configDirectory;
}

void WireGuardManager::updateTransferStats()
{
    if (m_connectionStatus == Connected) {
        QPair<uint64_t, uint64_t> stats = getTransferStats();
        emit transferStatsUpdated(stats.first, stats.second);
    }
}

void WireGuardManager::checkConnectionStatus()
{
    // Check if the service is still running
    if (m_connectionStatus == Connected && !m_currentServiceName.isEmpty()) {
        SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (scm) {
            SC_HANDLE service = OpenService(scm, reinterpret_cast<LPCWSTR>(m_currentServiceName.utf16()), SERVICE_QUERY_STATUS);
            if (service) {
                SERVICE_STATUS status;
                if (QueryServiceStatus(service, &status)) {
                    if (status.dwCurrentState != SERVICE_RUNNING) {
                        m_connectionStatus = Disconnected;
                        m_statsTimer->stop();
                        emit connectionStatusChanged(m_connectionStatus);
                        emit logMessage("WireGuard tunnel connection lost");
                    }
                }
                CloseServiceHandle(service);
            }
            CloseServiceHandle(scm);
        }
    }
}

bool WireGuardManager::createTunnelService(const QString& configPath, const QString& serviceName)
{
    emit logMessage(QString("Creating tunnel service: %1 for config: %2").arg(serviceName, configPath));
    
    // According to WireGuard documentation, we need to create a Windows service 
    // that will call WireGuardTunnelService, not call it directly
    
    // First, verify the configuration file exists and is readable with robust validation
    QFile configFile(configPath);
    if (!configFile.exists()) {
        emit errorOccurred(QString("Configuration file does not exist: %1").arg(configPath));
        return false;
    }
    
    // Try to open and read the file multiple times to ensure it's fully written
    QString configContent;
    int attempts = 0;
    const int maxAttempts = 5;
    bool fileValidated = false;
    
    while (attempts < maxAttempts && !fileValidated) {
        if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&configFile);
            configContent = stream.readAll();
            configFile.close();
            
            // Validate content is not empty and looks like a WireGuard config
            if (!configContent.isEmpty() && 
                configContent.contains("[Interface]") && 
                configContent.contains("PrivateKey")) {
                fileValidated = true;
                emit logMessage(QString("Config file validated successfully on attempt %1: %2 characters")
                               .arg(attempts + 1).arg(configContent.length()));
            } else {
                emit logMessage(QString("Config file validation failed on attempt %1: empty or invalid content")
                               .arg(attempts + 1));
            }
        } else {
            emit logMessage(QString("Could not open config file on attempt %1: %2")
                           .arg(attempts + 1).arg(configFile.errorString()));
        }
        
        if (!fileValidated) {
            attempts++;
            if (attempts < maxAttempts) {
                emit logMessage(QString("Waiting 200ms before retry attempt %1").arg(attempts + 1));
                QThread::msleep(200); // Wait 200ms before retry
            }
        }
    }
    
    if (!fileValidated) {
        emit errorOccurred(QString("Cannot validate configuration file after %1 attempts: %2").arg(maxAttempts).arg(configPath));
        return false;
    }
    
    // Get current executable path for service creation
    QString exePath = QCoreApplication::applicationFilePath();
    QString serviceCmd = QString("\"%1\" /service \"%2\"").arg(QDir::toNativeSeparators(exePath), QDir::toNativeSeparators(configPath));
    
    emit logMessage(QString("Creating Windows service with command: %1").arg(serviceCmd));
    
    // Create Windows service using Windows API
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scManager) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            emit errorOccurred("Failed to create tunnel service: Access Denied. Please run this application as Administrator.");
        } else {
            emit errorOccurred(QString("Failed to open Service Control Manager. Error: %1").arg(error));
        }
        return false;
    }
    
    // Convert strings to wide strings for Windows API
    std::wstring wideServiceName = serviceName.toStdWString();
    std::wstring wideDisplayName = QString("WireGuard Tunnel: %1").arg(serviceName).toStdWString();
    std::wstring wideServiceCmd = serviceCmd.toStdWString();
    
    SC_HANDLE service = CreateService(
        scManager,
        wideServiceName.c_str(),
        wideDisplayName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        wideServiceCmd.c_str(),
        nullptr,
        nullptr,
        L"Nsi\0TcpIp\0",  // Dependencies as required by WireGuard
        nullptr,
        nullptr
    );
    
    if (!service) {
        DWORD error = GetLastError();
        CloseServiceHandle(scManager);
        
        if (error == ERROR_SERVICE_EXISTS) {
            emit logMessage(QString("Service already exists: %1").arg(serviceName));
            return true;  // Service exists, that's okay
        } else {
            emit errorOccurred(QString("Failed to create service: %1. Error: %2").arg(serviceName).arg(error));
            return false;
        }
    }
    
    // Set service description
    SERVICE_DESCRIPTION serviceDesc;
    std::wstring wideDesc = QString("WireGuard VPN tunnel service").toStdWString();
    serviceDesc.lpDescription = const_cast<LPWSTR>(wideDesc.c_str());
    ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &serviceDesc);
    
    // Set service SID type to unrestricted (required by WireGuard)
    SERVICE_SID_INFO sidInfo;
    sidInfo.dwServiceSidType = SERVICE_SID_TYPE_UNRESTRICTED;
    ChangeServiceConfig2(service, SERVICE_CONFIG_SERVICE_SID_INFO, &sidInfo);
    
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    
    emit logMessage(QString("Successfully created tunnel service: %1").arg(serviceName));
    return true;
}

bool WireGuardManager::startTunnelService(const QString& serviceName)
{
    emit logMessage(QString("Starting tunnel service: %1").arg(serviceName));
    
    // Open Service Control Manager
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            emit errorOccurred("Failed to start tunnel service: Access Denied. Please run this application as Administrator.");
        } else {
            emit errorOccurred(QString("Failed to open Service Control Manager. Error: %1").arg(error));
        }
        return false;
    }
    
    // Convert service name to wide string
    std::wstring wideServiceName = serviceName.toStdWString();
    
    // Open the service
    SC_HANDLE service = OpenService(scManager, wideServiceName.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
    if (!service) {
        DWORD error = GetLastError();
        CloseServiceHandle(scManager);
        emit errorOccurred(QString("Failed to open service: %1. Error: %2").arg(serviceName).arg(error));
        return false;
    }
    
    // Check if service is already running
    SERVICE_STATUS status;
    if (QueryServiceStatus(service, &status)) {
        if (status.dwCurrentState == SERVICE_RUNNING) {
            emit logMessage(QString("Service is already running: %1").arg(serviceName));
            CloseServiceHandle(service);
            CloseServiceHandle(scManager);
            return true;
        }
    }
    
    // Start the service
    if (!StartService(service, 0, nullptr)) {
        DWORD error = GetLastError();
        CloseServiceHandle(service);
        CloseServiceHandle(scManager);
        
        if (error == ERROR_SERVICE_ALREADY_RUNNING) {
            emit logMessage(QString("Service is already running: %1").arg(serviceName));
            return true;
        } else {
            emit errorOccurred(QString("Failed to start service: %1. Error: %2").arg(serviceName).arg(error));
            return false;
        }
    }
    
    // Wait for service to start (with timeout)
    for (int i = 0; i < 30; i++) {  // Wait up to 30 seconds
        if (QueryServiceStatus(service, &status)) {
            if (status.dwCurrentState == SERVICE_RUNNING) {
                emit logMessage(QString("Service started successfully: %1").arg(serviceName));
                CloseServiceHandle(service);
                CloseServiceHandle(scManager);
                return true;
            } else if (status.dwCurrentState == SERVICE_STOPPED) {
                emit errorOccurred(QString("Service failed to start: %1").arg(serviceName));
                break;
            }
        }
        QThread::msleep(1000);  // Wait 1 second
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    emit errorOccurred(QString("Service start timeout: %1").arg(serviceName));
    return false;
}

bool WireGuardManager::stopTunnelService(const QString& serviceName)
{
    emit logMessage(QString("Stopping tunnel service: %1").arg(serviceName));
    
    // Open Service Control Manager
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            emit errorOccurred("Failed to stop tunnel service: Access Denied. Please run this application as Administrator.");
        } else {
            emit errorOccurred(QString("Failed to open Service Control Manager. Error: %1").arg(error));
        }
        return false;
    }
    
    // Convert service name to wide string
    std::wstring wideServiceName = serviceName.toStdWString();
    
    // Open the service
    SC_HANDLE service = OpenService(scManager, wideServiceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!service) {
        DWORD error = GetLastError();
        CloseServiceHandle(scManager);
        if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
            emit logMessage(QString("Service does not exist: %1").arg(serviceName));
            return true;  // Service doesn't exist, consider it "stopped"
        }
        emit errorOccurred(QString("Failed to open service: %1. Error: %2").arg(serviceName).arg(error));
        return false;
    }
    
    // Check if service is already stopped
    SERVICE_STATUS status;
    if (QueryServiceStatus(service, &status)) {
        if (status.dwCurrentState == SERVICE_STOPPED) {
            emit logMessage(QString("Service is already stopped: %1").arg(serviceName));
            CloseServiceHandle(service);
            CloseServiceHandle(scManager);
            return true;
        }
    }
    
    // Stop the service
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        DWORD error = GetLastError();
        CloseServiceHandle(service);
        CloseServiceHandle(scManager);
        
        if (error == ERROR_SERVICE_NOT_ACTIVE) {
            emit logMessage(QString("Service is not active: %1").arg(serviceName));
            return true;
        } else {
            emit errorOccurred(QString("Failed to stop service: %1. Error: %2").arg(serviceName).arg(error));
            return false;
        }
    }
    
    // Wait for service to stop (with timeout)
    for (int i = 0; i < 30; i++) {  // Wait up to 30 seconds
        if (QueryServiceStatus(service, &status)) {
            if (status.dwCurrentState == SERVICE_STOPPED) {
                emit logMessage(QString("Service stopped successfully: %1").arg(serviceName));
                CloseServiceHandle(service);
                CloseServiceHandle(scManager);
                return true;
            }
        }
        QThread::msleep(1000);  // Wait 1 second
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    emit logMessage(QString("Service stop completed (may have timed out): %1").arg(serviceName));
    return true;  // Don't fail if we can't verify it stopped
}

bool WireGuardManager::removeTunnelService(const QString& serviceName)
{
    emit logMessage(QString("Removing tunnel service: %1").arg(serviceName));
    
    // First stop the service if it's running
    stopTunnelService(serviceName);
    
    // Open Service Control Manager
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            emit errorOccurred("Failed to remove tunnel service: Access Denied. Please run this application as Administrator.");
        } else {
            emit errorOccurred(QString("Failed to open Service Control Manager. Error: %1").arg(error));
        }
        return false;
    }
    
    // Convert service name to wide string
    std::wstring wideServiceName = serviceName.toStdWString();
    
    // Open the service
    SC_HANDLE service = OpenService(scManager, wideServiceName.c_str(), DELETE);
    if (!service) {
        DWORD error = GetLastError();
        CloseServiceHandle(scManager);
        if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
            emit logMessage(QString("Service does not exist: %1").arg(serviceName));
            return true;  // Service doesn't exist, consider it "removed"
        }
        emit errorOccurred(QString("Failed to open service for deletion: %1. Error: %2").arg(serviceName).arg(error));
        return false;
    }
    
    // Delete the service
    if (!DeleteService(service)) {
        DWORD error = GetLastError();
        CloseServiceHandle(service);
        CloseServiceHandle(scManager);
        if (error == ERROR_SERVICE_MARKED_FOR_DELETE) {
            emit logMessage(QString("Service is already marked for deletion: %1").arg(serviceName));
            return true;
        } else {
            emit errorOccurred(QString("Failed to delete service: %1. Error: %2").arg(serviceName).arg(error));
            return false;
        }
    }
    
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    
    emit logMessage(QString("Service removed successfully: %1").arg(serviceName));
    return true;
}

QString WireGuardManager::generateServiceName(const QString& configName)
{
    return QString("WireGuardTunnel$%1").arg(configName);
}

bool WireGuardManager::loadDlls()
{
    emit logMessage("Loading WireGuard DLLs...");
    
    // Try to load tunnel.dll
    m_tunnelDll = LoadLibrary(L"tunnel.dll");
    if (!m_tunnelDll) {
        DWORD error = GetLastError();
        emit errorOccurred(QString("Could not load tunnel.dll (Error: %1)").arg(error));
        return false;
    }
    emit logMessage("Successfully loaded tunnel.dll");
    
    // Try to load wireguard.dll
    m_wireguardDll = LoadLibrary(L"wireguard.dll");
    if (!m_wireguardDll) {
        DWORD error = GetLastError();
        emit errorOccurred(QString("Could not load wireguard.dll (Error: %1)").arg(error));
        FreeLibrary(m_tunnelDll);
        m_tunnelDll = nullptr;
        return false;
    }
    emit logMessage("Successfully loaded wireguard.dll");
    
    // Load function pointers
    m_generateKeypairFunc = reinterpret_cast<WireGuardGenerateKeypairFunc>(
        GetProcAddress(m_tunnelDll, "WireGuardGenerateKeypair"));
    
    m_tunnelServiceFunc = reinterpret_cast<WireGuardTunnelServiceFunc>(
        GetProcAddress(m_tunnelDll, "WireGuardTunnelService"));
    
    m_openAdapterFunc = reinterpret_cast<WireGuardOpenAdapterFunc>(
        GetProcAddress(m_wireguardDll, "WireGuardOpenAdapter"));
    
    m_closeAdapterFunc = reinterpret_cast<WireGuardCloseAdapterFunc>(
        GetProcAddress(m_wireguardDll, "WireGuardCloseAdapter"));
    
    m_getConfigurationFunc = reinterpret_cast<WireGuardGetConfigurationFunc>(
        GetProcAddress(m_wireguardDll, "WireGuardGetConfiguration"));
    
    // Check if all required functions were loaded
    if (!m_generateKeypairFunc) {
        emit errorOccurred("Failed to load WireGuardGenerateKeypair function from tunnel.dll");
        unloadDlls();
        return false;
    }
    
    if (!m_tunnelServiceFunc) {
        emit errorOccurred("Failed to load WireGuardTunnelService function from tunnel.dll");
        unloadDlls();
        return false;
    }
    
    if (!m_openAdapterFunc) {
        emit errorOccurred("Failed to load WireGuardOpenAdapter function from wireguard.dll");
        unloadDlls();
        return false;
    }
    
    if (!m_closeAdapterFunc) {
        emit errorOccurred("Failed to load WireGuardCloseAdapter function from wireguard.dll");
        unloadDlls();
        return false;
    }
    
    if (!m_getConfigurationFunc) {
        emit errorOccurred("Failed to load WireGuardGetConfiguration function from wireguard.dll");
        unloadDlls();
        return false;
    }
    
    emit logMessage("All WireGuard DLL functions loaded successfully");
    return true;
}

void WireGuardManager::unloadDlls()
{
    if (m_tunnelDll) {
        FreeLibrary(m_tunnelDll);
        m_tunnelDll = nullptr;
    }
    
    if (m_wireguardDll) {
        FreeLibrary(m_wireguardDll);
        m_wireguardDll = nullptr;
    }
    
    m_generateKeypairFunc = nullptr;
    m_tunnelServiceFunc = nullptr;
    m_openAdapterFunc = nullptr;
    m_closeAdapterFunc = nullptr;
    m_getConfigurationFunc = nullptr;
}

bool WireGuardManager::initializeConfigDirectory()
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_configDirectory = QDir(appDataPath).filePath(CONFIG_DIR_NAME);
    
    QDir dir;
    if (!dir.exists(m_configDirectory)) {
        if (!dir.mkpath(m_configDirectory)) {
            emit errorOccurred(QString("Failed to create configuration directory: %1").arg(m_configDirectory));
            return false;
        }
    }
    
    return true;
}

QString WireGuardManager::base64Encode(const QByteArray& data)
{
    return data.toBase64();
}

QByteArray WireGuardManager::base64Decode(const QString& data)
{
    return QByteArray::fromBase64(data.toUtf8());
}

bool WireGuardManager::writeConfigFile(const QString& filePath, const WireGuardConfig& config)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << configToString(config);
    
    return true;
}

WireGuardConfig WireGuardManager::readConfigFile(const QString& filePath)
{
    WireGuardConfig config;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return config;
    }
    
    QTextStream in(&file);
    QString currentSection;
    WireGuardPeer currentPeer;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        
        if (line.startsWith('[') && line.endsWith(']')) {
            if (currentSection == "Peer" && !currentPeer.publicKey.isEmpty()) {
                config.interfaceConfig.peers.append(currentPeer);
                currentPeer = WireGuardPeer();
            }
            currentSection = line.mid(1, line.length() - 2);
            continue;
        }
        
        int equalPos = line.indexOf('=');
        if (equalPos == -1) continue;
        
        QString key = line.left(equalPos).trimmed();
        QString value = line.mid(equalPos + 1).trimmed();        if (currentSection == "Interface") {
            if (key == "PrivateKey") {
                config.interfaceConfig.privateKey = value;
            // Note: PublicKey is NOT read from [Interface] section - it's derived from PrivateKey
            } else if (key == "Address") {
                config.interfaceConfig.addresses = value.split(',');
                for (QString& addr : config.interfaceConfig.addresses) {
                    addr = addr.trimmed();
                }
            } else if (key == "DNS") {
                config.interfaceConfig.dns = value.split(',');
                for (QString& dns : config.interfaceConfig.dns) {
                    dns = dns.trimmed();
                }
            } else if (key == "ListenPort") {
                config.interfaceConfig.listenPort = value.toUInt();
            } else if (key == "MTU") {
                config.interfaceConfig.mtu = value.toUInt();
                if (config.interfaceConfig.mtu == 0) {
                    config.interfaceConfig.mtu = 1280;  // Default to 1280 if invalid
                }
            }
        } else if (currentSection == "Peer") {
            if (key == "PublicKey") {
                currentPeer.publicKey = value;
            } else if (key == "PresharedKey") {
                currentPeer.presharedKey = value;
            } else if (key == "Endpoint") {
                currentPeer.endpoint = value;
            } else if (key == "AllowedIPs") {
                currentPeer.allowedIPs = value.split(',');
                for (QString& ip : currentPeer.allowedIPs) {
                    ip = ip.trimmed();
                }
            } else if (key == "PersistentKeepalive") {
                currentPeer.persistentKeepalive = value.toUInt();
            }
        }
    }
    
    if (currentSection == "Peer" && !currentPeer.publicKey.isEmpty()) {
        config.interfaceConfig.peers.append(currentPeer);
    }
    
    // Set the config name from the file path
    QFileInfo fileInfo(filePath);
    config.interfaceConfig.name = fileInfo.baseName();
    config.configFilePath = filePath;
    
    return config;
}
