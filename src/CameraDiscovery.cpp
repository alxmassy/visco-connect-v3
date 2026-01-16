#include "CameraDiscovery.h"
#include "Logger.h"
#include <QNetworkInterface>
#include <QHostInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QEventLoop>
#include <QApplication>
#include <QThread>
#include <QThreadPool>
#include <QRunnable>
#include <QAtomicInteger>
#include <QMutex>
#include <QMutexLocker>

// NetworkScanner Implementation
NetworkScanner::NetworkScanner(const QString& networkRange, QObject *parent)
    : QThread(parent)
    , m_networkRange(networkRange)
    , m_shouldStop(false)
{
    // Default camera ports - prioritized order (most common first)
    m_ports = {80, 554, 8080, 8081, 443, 8000, 8443, 88, 8088};
}

void NetworkScanner::setPortRange(const QList<int>& ports)
{
    QMutexLocker locker(&m_mutex);
    m_ports = ports;
}

void NetworkScanner::stop()
{
    QMutexLocker locker(&m_mutex);
    m_shouldStop = true;
}

void NetworkScanner::run()
{
    QRegularExpression ipRegex(R"((\d+)\.(\d+)\.(\d+)\.(\d+)(?:/(\d+))?)");
    QRegularExpressionMatch match = ipRegex.match(m_networkRange);
    
    if (!match.hasMatch()) {
        emit scanFinished();
        return;
    }

    QString baseIp = QString("%1.%2.%3").arg(match.captured(1), match.captured(2), match.captured(3));
    int subnetMask = match.captured(5).isEmpty() ? 24 : match.captured(5).toInt();
    
    int hostCount = (1 << (32 - subnetMask)) - 2; // Exclude network and broadcast
    int startHost = 1;
    int endHost = qMin(254, hostCount);
    
    int totalOperations = (endHost - startHost + 1) * m_ports.size();
    int currentOperation = 0;
    
    // Use QThreadPool for parallel scanning
    QThreadPool *pool = QThreadPool::globalInstance();
    pool->setMaxThreadCount(qMin(50, QThread::idealThreadCount() * 4)); // Increase thread count
    
    QAtomicInteger<int> completedOperations(0);
    QMutex progressMutex;
    
    // Collect results to emit later
    QMutex resultsMutex;
    QList<QPair<QString, int>> foundDevices;
    
    for (int host = startHost; host <= endHost && !m_shouldStop; ++host) {
        QString ipAddress = QString("%1.%2").arg(baseIp).arg(host);
        
        // Create a single task per IP that scans all ports with early termination
        QRunnable* task = QRunnable::create([this, ipAddress, &completedOperations, &progressMutex, &resultsMutex, &foundDevices, totalOperations]() {
            bool deviceFound = false;
            
            // Scan priority ports first (80, 554) with shorter timeout
            QList<int> priorityPorts = {80, 554};
            QList<int> remainingPorts = m_ports;
            
            // Remove priority ports from remaining list
            for (int priority : priorityPorts) {
                remainingPorts.removeAll(priority);
            }
            
            // Scan priority ports first
            for (int port : priorityPorts) {
                if (m_shouldStop || deviceFound) break;
                
                QTcpSocket socket;
                socket.connectToHost(ipAddress, port);
                
                if (socket.waitForConnected(200)) { // Reduced timeout to 200ms
                    {
                        QMutexLocker locker(&resultsMutex);
                        foundDevices.append(qMakePair(ipAddress, port));
                    }
                    socket.disconnectFromHost();
                    deviceFound = true; // Found on priority port, skip remaining
                    break;
                }
                
                int completed = completedOperations.fetchAndAddAcquire(1) + 1;
                if (completed % 25 == 0) { // Update progress less frequently
                    QMutexLocker locker(&progressMutex);
                    QMetaObject::invokeMethod(this, "scanProgress", Qt::QueuedConnection,
                                            Q_ARG(int, completed), Q_ARG(int, totalOperations));
                }
            }
            
            // Only scan remaining ports if not found on priority ports
            if (!deviceFound && !m_shouldStop) {
                for (int port : remainingPorts) {
                    if (m_shouldStop) break;
                    
                    QTcpSocket socket;
                    socket.connectToHost(ipAddress, port);
                    
                    if (socket.waitForConnected(200)) { // Reduced timeout
                        {
                            QMutexLocker locker(&resultsMutex);
                            foundDevices.append(qMakePair(ipAddress, port));
                        }
                        socket.disconnectFromHost();
                        deviceFound = true;
                    }
                    
                    int completed = completedOperations.fetchAndAddAcquire(1) + 1;
                    if (completed % 25 == 0) {
                        QMutexLocker locker(&progressMutex);
                        QMetaObject::invokeMethod(this, "scanProgress", Qt::QueuedConnection,
                                                Q_ARG(int, completed), Q_ARG(int, totalOperations));
                    }
                }
            } else {
                // If found on priority port, still increment counter for remaining ports
                completedOperations.fetchAndAddAcquire(remainingPorts.size());
            }
        });
        
        pool->start(task);
    }
    
    // Wait for all tasks to complete
    pool->waitForDone();
    
    // Emit all found devices
    for (const auto& device : foundDevices) {
        if (!m_shouldStop) {
            emit deviceFound(device.first, device.second);
        }
    }
    
    emit scanProgress(totalOperations, totalOperations);
    emit scanFinished();
}

// CameraDiscovery Implementation
CameraDiscovery::CameraDiscovery(QObject *parent)
    : QObject(parent)
    , m_networkManager(nullptr)
    , m_scanner(nullptr)
    , m_timeout(2000) // Reduced from 5000ms to 2000ms
    , m_maxConcurrentRequests(50) // Increased from 10 to 50
    , m_currentRequests(0)
    , m_isDiscovering(false)
    , m_totalHosts(0)
    , m_scannedHosts(0)
{
    m_networkManager = new QNetworkAccessManager(this);
    
    // Initialize common camera ports in priority order
    m_cameraPorts = {80, 554, 8080, 8081, 443, 8000, 8443, 88, 8088, 8888, 9999};
    
    LOG_INFO("Camera discovery initialized with optimized settings", "CameraDiscovery");
}

CameraDiscovery::~CameraDiscovery()
{
    stopDiscovery();
}

void CameraDiscovery::startDiscovery()
{
    QString range = detectNetworkRange();
    startDiscovery(range);
}

void CameraDiscovery::startDiscovery(const QString& networkRange)
{
    if (m_isDiscovering) {
        LOG_WARNING("Discovery already in progress", "CameraDiscovery");
        return;
    }
    
    m_networkRange = networkRange;
    m_isDiscovering = true;
    m_scannedHosts = 0;
    m_discoveredCameras.clear();
    
    LOG_INFO(QString("Starting camera discovery on network: %1").arg(networkRange), "CameraDiscovery");
    emit discoveryStarted();
    
    initializeScanner();
    startNetworkScan();
}

void CameraDiscovery::stopDiscovery()
{
    if (!m_isDiscovering) return;
    
    m_isDiscovering = false;
    
    if (m_scanner) {
        m_scanner->stop();
        m_scanner->wait(3000); // Wait up to 3 seconds
        m_scanner->deleteLater();
        m_scanner = nullptr;
    }
    
    // Cancel pending HTTP requests
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ++it) {
        it.key()->abort();
    }
    m_pendingRequests.clear();
    m_currentRequests = 0;
    
    LOG_INFO("Camera discovery stopped", "CameraDiscovery");
    emit discoveryFinished();
}

void CameraDiscovery::setNetworkRange(const QString& range)
{
    m_networkRange = range;
}

void CameraDiscovery::setTimeout(int milliseconds)
{
    m_timeout = milliseconds;
}

void CameraDiscovery::setMaxConcurrentRequests(int count)
{
    m_maxConcurrentRequests = count;
}

bool CameraDiscovery::isDiscovering() const
{
    return m_isDiscovering;
}

QList<DiscoveredCamera> CameraDiscovery::getDiscoveredCameras() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_discoveredCameras;
}

void CameraDiscovery::clearDiscoveredCameras()
{
    QMutexLocker locker(&m_dataMutex);
    m_discoveredCameras.clear();
}

QString CameraDiscovery::detectNetworkRange()
{
    QList<QHostAddress> addresses;
    
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
        if (interface.flags() & QNetworkInterface::IsUp && 
            interface.flags() & QNetworkInterface::IsRunning &&
            !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            
            for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    quint32 ip = entry.ip().toIPv4Address();
                    quint32 netmask = entry.netmask().toIPv4Address();
                    quint32 network = ip & netmask;
                    
                    return QHostAddress(network).toString() + "/24";
                }
            }
        }
    }
    
    return "192.168.1.0/24"; // Default fallback
}

QString CameraDiscovery::brandFromResponse(const QString& response, const QString& userAgent)
{
    QString lowerResponse = response.toLower();
    QString lowerAgent = userAgent.toLower();
    
    // Hikvision detection
    if (lowerResponse.contains("hikvision") || 
        lowerResponse.contains("hik-connect") ||
        lowerResponse.contains("webrec.htm") ||
        lowerAgent.contains("hikvision")) {
        return "Hikvision";
    }
    
    // CP Plus detection
    if (lowerResponse.contains("cp plus") || 
        lowerResponse.contains("cpplus") ||
        lowerResponse.contains("cp-plus") ||
        lowerResponse.contains("aditya") ||
        lowerAgent.contains("cpplus")) {
        return "CP Plus";
    }
    
    // Additional brand detection patterns
    if (lowerResponse.contains("dahua") || lowerAgent.contains("dahua")) {
        return "Dahua";
    }
    
    if (lowerResponse.contains("axis") || lowerAgent.contains("axis")) {
        return "Axis";
    }
    
    if (lowerResponse.contains("vivotek") || lowerAgent.contains("vivotek")) {
        return "Vivotek";
    }
    
    if (lowerResponse.contains("foscam") || lowerAgent.contains("foscam")) {
        return "Foscam";
    }
    
    return "Generic";
}

QString CameraDiscovery::generateRtspUrl(const QString& brand, const QString& ipAddress, int port)
{
    QString baseUrl = QString("rtsp://%1:%2").arg(ipAddress).arg(port);
    
    if (brand == "Hikvision") {
        return baseUrl + "/Streaming/Channels/101";
    } else if (brand == "CP Plus") {
        return baseUrl + "/cam/realmonitor?channel=1&subtype=0";
    } else if (brand == "Dahua") {
        return baseUrl + "/cam/realmonitor?channel=1&subtype=0";
    } else if (brand == "Axis") {
        return baseUrl + "/axis-media/media.amp";
    } else if (brand == "Vivotek") {
        return baseUrl + "/live.sdp";
    } else if (brand == "Foscam") {
        return baseUrl + "/videoMain";
    } else {
        // Generic RTSP URLs to try
        return baseUrl + "/stream1"; // Most common generic path
    }
}

QStringList CameraDiscovery::getCommonRtspPaths(const QString& brand)
{
    if (brand == "Hikvision") {
        return {
            "/Streaming/Channels/101",
            "/Streaming/Channels/102", 
            "/Streaming/Channels/1",
            "/h264_stream",
            "/ch1/main/av_stream"
        };
    } else if (brand == "CP Plus") {
        return {
            "/cam/realmonitor?channel=1&subtype=0",
            "/cam/realmonitor?channel=1&subtype=1",
            "/streaming/channels/1",
            "/stream1",
            "/cam1"
        };
    } else if (brand == "Dahua") {
        return {
            "/cam/realmonitor?channel=1&subtype=0",
            "/cam/realmonitor?channel=1&subtype=1",
            "/streaming/channels/1",
            "/stream1"
        };
    } else {
        return {
            "/stream1",
            "/video1",
            "/cam1",
            "/live.sdp",
            "/axis-media/media.amp",
            "/videoMain",
            "/streaming/channels/1",
            "/h264",
            "/mjpeg"
        };
    }
}

void CameraDiscovery::onDeviceFound(const QString& ipAddress, int port)
{
    if (!m_isDiscovering) return;
    
    LOG_INFO(QString("Device found at %1:%2").arg(ipAddress).arg(port), "CameraDiscovery");
    
    // Limit concurrent requests but be more aggressive
    if (m_currentRequests >= m_maxConcurrentRequests) {
        QTimer::singleShot(50, this, [this, ipAddress, port]() { // Reduced delay from 100ms to 50ms
            onDeviceFound(ipAddress, port);
        });
        return;
    }
    
    identifyDevice(ipAddress, port);
}

void CameraDiscovery::onHttpResponse()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    auto it = m_pendingRequests.find(reply);
    if (it == m_pendingRequests.end()) {
        reply->deleteLater();
        return;
    }
    
    QString ipAddress = it.value().first;
    int port = it.value().second;
    m_pendingRequests.erase(it);
    m_currentRequests--;
    
    if (reply->error() == QNetworkReply::NoError) {
        QString response = reply->readAll();
        QString headers;
        
        for (const auto& header : reply->rawHeaderList()) {
            headers += QString("%1: %2\n").arg(QString(header), QString(reply->rawHeader(header)));
        }
        
        DiscoveredCamera camera = analyzeHttpResponse(ipAddress, port, response, headers);
        if (!camera.brand.isEmpty()) {
            camera.isOnline = true;
            
            QMutexLocker locker(&m_dataMutex);
            m_discoveredCameras.append(camera);
            
            LOG_INFO(QString("Discovered %1 camera at %2:%3 - Model: %4")
                     .arg(camera.brand, ipAddress).arg(port).arg(camera.model), "CameraDiscovery");
            
            emit cameraDiscovered(camera);
        }
    }
    
    reply->deleteLater();
}

void CameraDiscovery::onHttpError(QNetworkReply::NetworkError error)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    auto it = m_pendingRequests.find(reply);
    if (it != m_pendingRequests.end()) {
        m_pendingRequests.erase(it);
        m_currentRequests--;
    }
    
    reply->deleteLater();
}

void CameraDiscovery::onScanProgress(int current, int total)
{
    m_scannedHosts = current;
    m_totalHosts = total;
    emit discoveryProgress(current, total);
}

void CameraDiscovery::onScanFinished()
{
    if (m_scanner) {
        m_scanner->deleteLater();
        m_scanner = nullptr;
    }
    
    // Wait for pending HTTP requests to complete with shorter timeout
    QTimer::singleShot(1000, this, [this]() { // Reduced from 2000ms to 1000ms
        if (m_currentRequests == 0) {
            m_isDiscovering = false;
            
            LOG_INFO(QString("Camera discovery finished. Found %1 cameras.")
                     .arg(m_discoveredCameras.size()), "CameraDiscovery");
            
            emit discoveryFinished();
        } else {
            // Check again in 500ms instead of 1 second
            QTimer::singleShot(500, this, &CameraDiscovery::onScanFinished);
        }
    });
}

void CameraDiscovery::initializeScanner()
{
    if (m_scanner) {
        m_scanner->deleteLater();
    }
    
    m_scanner = new NetworkScanner(m_networkRange, this);
    m_scanner->setPortRange(m_cameraPorts);
    
    connect(m_scanner, &NetworkScanner::deviceFound, this, &CameraDiscovery::onDeviceFound);
    connect(m_scanner, &NetworkScanner::scanProgress, this, &CameraDiscovery::onScanProgress);
    connect(m_scanner, &NetworkScanner::scanFinished, this, &CameraDiscovery::onScanFinished);
}

void CameraDiscovery::startNetworkScan()
{
    if (m_scanner) {
        m_scanner->start();
    }
}

void CameraDiscovery::identifyDevice(const QString& ipAddress, int port)
{
    // Try HTTP first on discovered port
    sendHttpRequest(ipAddress, port, "/");
    
    // For common web ports, also try camera-specific paths immediately
    if (port == 80 || port == 8080) {
        QTimer::singleShot(100, this, [this, ipAddress, port]() { // Reduced delay
            sendHttpRequest(ipAddress, port, "/cgi-bin/hi3510/param.cgi");
        });
        QTimer::singleShot(200, this, [this, ipAddress, port]() {
            sendHttpRequest(ipAddress, port, "/PSIA/Custom/SelfExt/userCheck");
        });
        QTimer::singleShot(300, this, [this, ipAddress, port]() {
            sendHttpRequest(ipAddress, port, "/onvif/device_service");
        });
    }
}

void CameraDiscovery::sendHttpRequest(const QString& ipAddress, int port, const QString& path)
{
    if (m_currentRequests >= m_maxConcurrentRequests) return;
    
    QString url = QString("http://%1:%2%3").arg(ipAddress).arg(port).arg(path);
    QNetworkRequest request(url);
    
    // Set headers to identify camera responses
    request.setHeader(QNetworkRequest::UserAgentHeader, "CameraDiscovery/1.0");
    request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    
    QNetworkReply* reply = m_networkManager->get(request);
    reply->setParent(this);
    
    // Set timeout
    QTimer* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(m_timeout);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start();
    
    connect(reply, &QNetworkReply::finished, this, &CameraDiscovery::onHttpResponse);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &CameraDiscovery::onHttpError);
    
    m_pendingRequests[reply] = qMakePair(ipAddress, port);
    m_currentRequests++;
}

DiscoveredCamera CameraDiscovery::analyzeHttpResponse(const QString& ipAddress, int port, 
                                                    const QString& response, const QString& headers)
{
    DiscoveredCamera camera;
    camera.ipAddress = ipAddress;
    camera.port = port;
    camera.isOnline = true;
    
    // Detect brand
    camera.brand = detectBrand(response, headers);
    
    // Detect model
    camera.model = detectModel(response, headers, camera.brand);
    
    // Extract device name
    camera.deviceName = extractDeviceName(response, headers);
    
    // Generate RTSP URL
    camera.rtspUrl = generateRtspUrl(camera.brand, ipAddress, 554);
    
    // Set supported ports (we found this port open)
    camera.supportedPorts.append(QString::number(port));
    
    return camera;
}

QString CameraDiscovery::detectBrand(const QString& response, const QString& headers)
{
    QString combined = (response + " " + headers).toLower();
    
    // Hikvision patterns
    if (combined.contains("hikvision") || 
        combined.contains("hik-connect") ||
        combined.contains("webrec.htm") ||
        combined.contains("server: app-webs/") ||
        combined.contains("ds-") ||
        combined.contains("/PSIA/")) {
        return "Hikvision";
    }
    
    // CP Plus patterns
    if (combined.contains("cp plus") || 
        combined.contains("cpplus") ||
        combined.contains("cp-plus") ||
        combined.contains("aditya") ||
        combined.contains("guard") ||
        combined.contains("realmonitor")) {
        return "CP Plus";
    }
    
    // Other brands
    if (combined.contains("dahua")) return "Dahua";
    if (combined.contains("axis")) return "Axis";
    if (combined.contains("vivotek")) return "Vivotek";
    if (combined.contains("foscam")) return "Foscam";
    if (combined.contains("acti")) return "ACTi";
    if (combined.contains("bosch")) return "Bosch";
    if (combined.contains("panasonic")) return "Panasonic";
    if (combined.contains("sony")) return "Sony";
    
    return "Generic";
}

QString CameraDiscovery::detectModel(const QString& response, const QString& headers, const QString& brand)
{
    if (brand == "Hikvision") {
        return getHikvisionModel(response);
    } else if (brand == "CP Plus") {
        return getCPPlusModel(response);
    }
      // Generic model detection
    QRegularExpression modelRegex(R"(model["\s]*[:=]["\s]*([^"<>\s]+))", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = modelRegex.match(response);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    
    return "Unknown";
}

QString CameraDiscovery::extractDeviceName(const QString& response, const QString& headers)
{    // Try to extract device name from title tag
    QRegularExpression titleRegex(R"(<title[^>]*>([^<]+)</title>)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = titleRegex.match(response);
    if (match.hasMatch()) {
        QString title = match.captured(1).trimmed();
        if (!title.isEmpty() && title != "Document") {
            return title;
        }
    }
    
    // Try device name field
    QRegularExpression nameRegex(R"(device[_\s]*name["\s]*[:=]["\s]*([^"<>\s]+))", QRegularExpression::CaseInsensitiveOption);
    match = nameRegex.match(response);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    
    return QString("Camera_%1").arg(QString(response.left(100).toUtf8().toHex()).left(8));
}

QString CameraDiscovery::getHikvisionModel(const QString& response)
{
    // Hikvision specific model patterns
    QRegularExpression modelRegex(R"((DS-\w+[\w-]*))");
    QRegularExpressionMatch match = modelRegex.match(response);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    
    return "Hikvision Camera";
}

QString CameraDiscovery::getCPPlusModel(const QString& response)
{
    // CP Plus specific model patterns
    QRegularExpression modelRegex(R"((CP-[\w-]+))");
    QRegularExpressionMatch match = modelRegex.match(response);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    
    return "CP Plus Camera";
}

void CameraDiscovery::onPingFinished()
{
    // Implementation for ping completion if needed
}
