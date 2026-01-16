#include "PingResponder.h"
#include "Logger.h"
#include <QDateTime>
#include <QDebug>

#ifdef Q_OS_WIN
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif

// ICMP packet structures
struct IcmpHeader {
    quint8 type;
    quint8 code;
    quint16 checksum;
    quint16 identifier;
    quint16 sequence;
};

struct IpHeader {
    quint8 version_ihl;
    quint8 type_of_service;
    quint16 total_length;
    quint16 identification;
    quint16 flags_fragment_offset;
    quint8 time_to_live;
    quint8 protocol;
    quint16 header_checksum;
    quint32 source_address;
    quint32 destination_address;
};

PingResponder::PingResponder(QObject *parent)
    : QObject(parent)
    , m_rawSocket(INVALID_SOCKET)
    , m_winsockInitialized(false)
    , m_socketNotifier(nullptr)
    , m_statusTimer(nullptr)
    , m_running(false)
    , m_responseEnabled(true)
    , m_totalPingsReceived(0)
    , m_totalPingsReplied(0)
    , m_startTime(0)
{
    LOG_INFO("PingResponder created", "PingResponder");
    
    // Initialize status timer
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(5000); // Check every 5 seconds
    connect(m_statusTimer, &QTimer::timeout, this, &PingResponder::checkSocketStatus);
}

PingResponder::~PingResponder()
{
    stopResponder();
    cleanupWinsock();
    LOG_INFO("PingResponder destroyed", "PingResponder");
}

bool PingResponder::startResponder()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_running) {
        LOG_WARNING("PingResponder already running", "PingResponder");
        return true;
    }
    
    LOG_INFO("Starting ICMP ping responder...", "PingResponder");
    
    // Initialize Winsock
    if (!m_winsockInitialized) {
        initializeWinsock();
    }
    
    // Create raw socket
    if (!createRawSocket()) {
        LOG_ERROR("Failed to create raw socket for ping responder", "PingResponder");
        emit errorOccurred("Failed to create raw socket. Run as administrator.");
        return false;
    }
    
    // Set up socket notifier
    m_socketNotifier = new QSocketNotifier((qintptr)m_rawSocket, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, &PingResponder::processPingData);
    m_socketNotifier->setEnabled(true);
    
    m_running = true;
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_totalPingsReceived = 0;
    m_totalPingsReplied = 0;
    
    // Start status timer
    m_statusTimer->start();
    
    LOG_INFO("ICMP ping responder started successfully", "PingResponder");
    emit started();
    
    return true;
}

void PingResponder::stopResponder()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_running) {
        return;
    }
    
    LOG_INFO("Stopping ICMP ping responder...", "PingResponder");
    
    m_running = false;
    
    // Stop status timer
    if (m_statusTimer) {
        m_statusTimer->stop();
    }
    
    // Clean up socket notifier
    if (m_socketNotifier) {
        m_socketNotifier->setEnabled(false);
        m_socketNotifier->deleteLater();
        m_socketNotifier = nullptr;
    }
    
    // Close socket
    closeSocket();
    
    LOG_INFO(QString("ICMP ping responder stopped. Stats: %1 received, %2 replied")
             .arg(m_totalPingsReceived).arg(m_totalPingsReplied), "PingResponder");
    
    emit stopped();
}

bool PingResponder::isRunning() const
{
    QMutexLocker locker(&const_cast<QMutex&>(m_mutex));
    return m_running;
}

void PingResponder::setResponseEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_responseEnabled = enabled;
    LOG_INFO(QString("Ping response %1").arg(enabled ? "enabled" : "disabled"), "PingResponder");
}

bool PingResponder::isResponseEnabled() const
{
    QMutexLocker locker(&const_cast<QMutex&>(m_mutex));
    return m_responseEnabled;
}

quint64 PingResponder::totalPingsReceived() const
{
    QMutexLocker locker(&const_cast<QMutex&>(m_mutex));
    return m_totalPingsReceived;
}

quint64 PingResponder::totalPingsReplied() const
{
    QMutexLocker locker(&const_cast<QMutex&>(m_mutex));
    return m_totalPingsReplied;
}

quint32 PingResponder::getResponseTimeMs() const
{
    QMutexLocker locker(&const_cast<QMutex&>(m_mutex));
    if (m_startTime == 0) return 0;
    return static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() - m_startTime);
}

void PingResponder::processPingData()
{
    if (!m_running) return;
    
    char buffer[1024];
    sockaddr_in from;
    int fromlen = sizeof(from);
    
#ifdef Q_OS_WIN
    int bytesReceived = recvfrom(m_rawSocket, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromlen);
#else
    int bytesReceived = recvfrom(m_rawSocket, buffer, sizeof(buffer), 0, (sockaddr*)&from, (socklen_t*)&fromlen);
#endif
    
    if (bytesReceived < 0) {
        LOG_ERROR("Error receiving ICMP data", "PingResponder");
        return;
    }
    
    QString sourceAddress = QHostAddress(ntohl(from.sin_addr.s_addr)).toString();
    
    // Process the ICMP packet
    if (processIcmpPacket(buffer, bytesReceived, sourceAddress)) {
        QMutexLocker locker(&m_mutex);
        m_totalPingsReceived++;
    }
}

void PingResponder::checkSocketStatus()
{
    if (!m_running) return;
    
    // Periodic status check - could be used for health monitoring
    LOG_DEBUG(QString("Ping responder status: %1 received, %2 replied")
              .arg(m_totalPingsReceived).arg(m_totalPingsReplied), "PingResponder");
}

void PingResponder::initializeWinsock()
{
#ifdef Q_OS_WIN
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG_ERROR(QString("WSAStartup failed: %1").arg(result), "PingResponder");
        return;
    }
    m_winsockInitialized = true;
    LOG_DEBUG("Winsock initialized successfully", "PingResponder");
#endif
}

void PingResponder::cleanupWinsock()
{
#ifdef Q_OS_WIN
    if (m_winsockInitialized) {
        WSACleanup();
        m_winsockInitialized = false;
        LOG_DEBUG("Winsock cleaned up", "PingResponder");
    }
#endif
}

bool PingResponder::createRawSocket()
{
#ifdef Q_OS_WIN
    m_rawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (m_rawSocket == INVALID_SOCKET) {
        int error = WSAGetLastError();
        LOG_ERROR(QString("Failed to create raw socket: %1").arg(error), "PingResponder");
        return false;
    }
    
    // Bind to any address
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;
    
    if (bind(m_rawSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        LOG_ERROR(QString("Failed to bind raw socket: %1").arg(error), "PingResponder");
        closesocket(m_rawSocket);
        m_rawSocket = INVALID_SOCKET;
        return false;
    }
    
    LOG_DEBUG("Raw ICMP socket created successfully", "PingResponder");
    return true;
#else
    m_rawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (m_rawSocket < 0) {
        LOG_ERROR("Failed to create raw socket", "PingResponder");
        return false;
    }
    return true;
#endif
}

void PingResponder::closeSocket()
{
#ifdef Q_OS_WIN
    if (m_rawSocket != INVALID_SOCKET) {
        closesocket(m_rawSocket);
        m_rawSocket = INVALID_SOCKET;
    }
#else
    if (m_rawSocket >= 0) {
        close(m_rawSocket);
        m_rawSocket = -1;
    }
#endif
}

bool PingResponder::processIcmpPacket(const char* buffer, int length, const QString& sourceAddress)
{
    // Skip IP header (typically 20 bytes)
    if (length < IP_HEADER_SIZE + ICMP_HEADER_SIZE) {
        return false;
    }
    
    // Parse IP header to get the correct IP header length
    const IpHeader* ipHeader = reinterpret_cast<const IpHeader*>(buffer);
    int ipHeaderLength = (ipHeader->version_ihl & 0x0F) * 4;
    
    if (length < ipHeaderLength + ICMP_HEADER_SIZE) {
        return false;
    }
    
    // Get ICMP header
    const IcmpHeader* icmpHeader = reinterpret_cast<const IcmpHeader*>(buffer + ipHeaderLength);
    
    // Check if it's an ICMP Echo Request (ping)
    if (icmpHeader->type == ICMP_ECHO) {
        quint16 identifier = ntohs(icmpHeader->identifier);
        quint16 sequence = ntohs(icmpHeader->sequence);
        
        LOG_DEBUG(QString("Received ping from %1 (ID: %2, Seq: %3)")
                  .arg(sourceAddress).arg(identifier).arg(sequence), "PingResponder");
        
        emit pingReceived(sourceAddress, identifier, sequence);
        
        // Send reply if enabled
        if (m_responseEnabled) {
            // Extract the data portion of the ping
            int dataLength = length - ipHeaderLength - ICMP_HEADER_SIZE;
            QByteArray originalData;
            if (dataLength > 0) {
                originalData = QByteArray(buffer + ipHeaderLength + ICMP_HEADER_SIZE, dataLength);
            }
            
            if (sendPingReply(sourceAddress, identifier, sequence, originalData)) {
                QMutexLocker locker(&m_mutex);
                m_totalPingsReplied++;
                emit pingReplied(sourceAddress, identifier, sequence, 1); // 1ms response time estimate
            }
        }
        
        return true;
    }
    
    return false;
}

bool PingResponder::sendPingReply(const QString& targetAddress, quint16 identifier, quint16 sequence, const QByteArray& originalData)
{
#ifdef Q_OS_WIN
    // Create ICMP Echo Reply packet
    QByteArray packet;
    packet.resize(ICMP_HEADER_SIZE + originalData.size());
    
    IcmpHeader* icmpHeader = reinterpret_cast<IcmpHeader*>(packet.data());
    icmpHeader->type = ICMP_ECHOREPLY;
    icmpHeader->code = 0;
    icmpHeader->checksum = 0;
    icmpHeader->identifier = htons(identifier);
    icmpHeader->sequence = htons(sequence);
    
    // Copy original data
    if (!originalData.isEmpty()) {
        memcpy(packet.data() + ICMP_HEADER_SIZE, originalData.data(), originalData.size());
    }
    
    // Calculate checksum
    quint32 checksum = 0;
    quint16* ptr = reinterpret_cast<quint16*>(packet.data());
    int len = packet.size();
    
    while (len > 1) {
        checksum += *ptr++;
        len -= 2;
    }
    
    if (len == 1) {
        checksum += *(reinterpret_cast<quint8*>(ptr));
    }
    
    checksum = (checksum >> 16) + (checksum & 0xFFFF);
    checksum += (checksum >> 16);
    icmpHeader->checksum = htons(~checksum);
    
    // Send the reply
    sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = htonl(QHostAddress(targetAddress).toIPv4Address());
    
    int result = sendto(m_rawSocket, packet.data(), packet.size(), 0, 
                       (sockaddr*)&target, sizeof(target));
    
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        LOG_ERROR(QString("Failed to send ping reply: %1").arg(error), "PingResponder");
        return false;
    }
    
    LOG_DEBUG(QString("Sent ping reply to %1 (ID: %2, Seq: %3)")
              .arg(targetAddress).arg(identifier).arg(sequence), "PingResponder");
    
    return true;
#else
    // Linux implementation would go here
    return false;
#endif
}
