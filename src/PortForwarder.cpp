#include "PortForwarder.h"
#include "Logger.h"
#include "NetworkInterfaceManager.h"
#include <QNetworkProxy>
#include <QTimer>
#include <QNetworkInterface>

PortForwarder::PortForwarder(QObject *parent)
    : QObject(parent)
    , m_networkManager(nullptr)
{
}

PortForwarder::~PortForwarder()
{
    stopAllForwarding();
}

bool PortForwarder::startForwarding(const CameraConfig& camera)
{
    if (!camera.isValid() || !camera.isEnabled()) {
        LOG_ERROR(QString("Invalid or disabled camera configuration: %1").arg(camera.name()), "PortForwarder");
        return false;
    }
    
    QString cameraId = camera.id();
    int externalPort = camera.externalPort();
    
    LOG_INFO(QString("Starting port forwarding for camera '%1' [ID: %2]")
             .arg(camera.name()).arg(cameraId), "PortForwarder");
    LOG_INFO(QString("  Camera IP: %1:%2").arg(camera.ipAddress()).arg(camera.port()), "PortForwarder");
    LOG_INFO(QString("  External Port: %1").arg(externalPort), "PortForwarder");
    
    // Check if external port is already in use
    if (isPortInUse(externalPort)) {
        LOG_ERROR(QString("External port %1 is already in use by another camera").arg(externalPort), "PortForwarder");
        emit forwardingError(cameraId, QString("Port %1 already in use").arg(externalPort));
        return false;
    }
    
    // Stop existing forwarding for this camera if any
    if (m_sessions.contains(cameraId)) {
        LOG_INFO(QString("Stopping existing forwarding session for camera: %1").arg(camera.name()), "PortForwarder");
        stopForwarding(cameraId);
    }
    
    // Create new session
    ForwardingSession* session = new ForwardingSession;
    session->camera = camera;
    session->server = new QTcpServer(this);
    session->isReconnecting = false;
    session->reconnectAttempts = 0;
    session->totalBytesTransferred = 0;
    session->lastActivity = QDateTime::currentDateTime();
    session->status = "Starting";
    
    // Set up reconnect timer
    session->reconnectTimer = new QTimer(this);
    session->reconnectTimer->setSingleShot(true);
    session->reconnectTimer->setInterval(RECONNECT_INTERVAL_MS);
    connect(session->reconnectTimer, &QTimer::timeout, this, &PortForwarder::handleReconnectTimer);
    
    // Set up health check timer
    session->healthCheckTimer = new QTimer(this);
    session->healthCheckTimer->setInterval(HEALTH_CHECK_INTERVAL_MS);
    connect(session->healthCheckTimer, &QTimer::timeout, this, &PortForwarder::handleHealthCheck);
    
    // Connect server signals
    connect(session->server, &QTcpServer::newConnection, this, &PortForwarder::handleNewConnection);
    
    // Start listening on all interfaces
    LOG_DEBUG(QString("Attempting to bind to all interfaces on port %1").arg(externalPort), "PortForwarder");
    
    if (!bindToAllInterfaces(session->server, externalPort)) {
        QString errorMsg = session->server->errorString();
        LOG_ERROR(QString("Failed to start listening on port %1: %2").arg(externalPort).arg(errorMsg), "PortForwarder");
        
        // Cleanup
        delete session->server;
        delete session->reconnectTimer;
        delete session->healthCheckTimer;
        delete session;
        
        emit forwardingError(cameraId, QString("Failed to bind port %1: %2").arg(externalPort).arg(errorMsg));
        return false;
    }
    
    // Store session
    m_sessions[cameraId] = session;
    
    // Start health check timer
    session->healthCheckTimer->start();
    
    // Update status
    updateSessionStatus(cameraId, "Active - Listening");
    
    LOG_INFO(QString("Successfully started port forwarding for camera '%1'")
             .arg(camera.name()), "PortForwarder");
    LOG_INFO(QString("  Listening on: 0.0.0.0:%1 -> %2:%3")
             .arg(externalPort).arg(camera.ipAddress()).arg(camera.port()), "PortForwarder");
    
    emit forwardingStarted(cameraId, externalPort);
    return true;
}

void PortForwarder::stopForwarding(const QString& cameraId)
{
    if (!m_sessions.contains(cameraId)) {
        LOG_DEBUG(QString("No active forwarding session found for camera: %1").arg(cameraId), "PortForwarder");
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    LOG_INFO(QString("Stopping port forwarding for camera '%1' [ID: %2]")
             .arg(session->camera.name()).arg(cameraId), "PortForwarder");
    
    // Update status
    updateSessionStatus(cameraId, "Stopping");
    
    // Stop health check timer
    if (session->healthCheckTimer) {
        session->healthCheckTimer->stop();
        session->healthCheckTimer->deleteLater();
        session->healthCheckTimer = nullptr;
    }
    
    // Stop reconnect timer
    if (session->reconnectTimer) {
        session->reconnectTimer->stop();
        session->reconnectTimer->deleteLater();
        session->reconnectTimer = nullptr;
    }
    
    // Close all connections with detailed logging
    int connectionCount = session->connections.size();
    LOG_INFO(QString("Closing %1 active connections for camera: %2")
             .arg(connectionCount).arg(session->camera.name()), "PortForwarder");
    
    for (auto it = session->connections.begin(); it != session->connections.end(); ++it) {
        QTcpSocket* clientSocket = it.key();
        ConnectionInfo* connInfo = it.value();
        
        if (connInfo) {
            logConnectionDetails(cameraId, connInfo, "Closing");
            
            if (connInfo->targetSocket) {
                connInfo->targetSocket->disconnectFromHost();
                connInfo->targetSocket->deleteLater();
            }
            
            delete connInfo;
        }
        
        if (clientSocket) {
            m_socketToCameraMap.remove(clientSocket);
            clientSocket->disconnectFromHost();
            clientSocket->deleteLater();
        }
    }
    session->connections.clear();
    
    // Stop and cleanup server
    if (session->server) {
        session->server->close();
        LOG_DEBUG(QString("Server stopped listening on port %1")
                  .arg(session->camera.externalPort()), "PortForwarder");
        session->server->deleteLater();
        session->server = nullptr;
    }
    
    // Log final statistics
    LOG_INFO(QString("Final statistics for camera '%1': %2 bytes transferred, %3 connections handled")
             .arg(session->camera.name())
             .arg(session->totalBytesTransferred)
             .arg(connectionCount), "PortForwarder");    delete session;
    m_sessions.remove(cameraId);
    
    LOG_INFO(QString("Successfully stopped port forwarding for camera: %1").arg(cameraId), "PortForwarder");
    emit forwardingStopped(cameraId);
}

void PortForwarder::stopAllForwarding()
{
    QStringList cameraIds = m_sessions.keys();
    for (const QString& cameraId : cameraIds) {
        stopForwarding(cameraId);
    }
}

bool PortForwarder::isForwarding(const QString& cameraId) const
{
    return m_sessions.contains(cameraId);
}

QStringList PortForwarder::getActiveForwards() const
{
    return m_sessions.keys();
}

void PortForwarder::restartForwarding(const QString& cameraId)
{
    if (!m_sessions.contains(cameraId)) {
        LOG_WARNING(QString("Cannot restart forwarding - no session found for camera: %1").arg(cameraId), "PortForwarder");
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    CameraConfig camera = session->camera;
    
    LOG_INFO(QString("Restarting port forwarding for camera: %1").arg(camera.name()), "PortForwarder");
    
    stopForwarding(cameraId);
    
    // Brief delay before restart
    QTimer::singleShot(1000, [this, camera]() {
        startForwarding(camera);
    });
}

bool PortForwarder::isPortInUse(int port) const
{
    for (const ForwardingSession* session : m_sessions.values()) {
        if (session->camera.externalPort() == port) {
            return true;
        }
    }
    return false;
}

int PortForwarder::getNextAvailablePort(int startPort) const
{
    int port = startPort;
    while (isPortInUse(port)) {
        port++;
        if (port > 65535) {
            return -1; // No available port found
        }
    }
    return port;
}

bool PortForwarder::changeExternalPort(const QString& cameraId, int newPort)
{
    if (!m_sessions.contains(cameraId)) {
        LOG_ERROR(QString("Cannot change port - no session found for camera: %1").arg(cameraId), "PortForwarder");
        return false;
    }
    
    if (isPortInUse(newPort)) {
        LOG_ERROR(QString("Cannot change to port %1 - already in use").arg(newPort), "PortForwarder");
        return false;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    int oldPort = session->camera.externalPort();
    
    LOG_INFO(QString("Changing external port for camera '%1' from %2 to %3")
             .arg(session->camera.name()).arg(oldPort).arg(newPort), "PortForwarder");
    
    // Update camera configuration
    session->camera.setExternalPort(newPort);
    
    // Restart the forwarding with new port
    restartForwarding(cameraId);
    
    emit portChanged(cameraId, oldPort, newPort);
    return true;
}

int PortForwarder::getConnectionCount(const QString& cameraId) const
{
    if (!m_sessions.contains(cameraId)) {
        return 0;
    }
    return m_sessions[cameraId]->connections.size();
}

qint64 PortForwarder::getBytesTransferred(const QString& cameraId) const
{
    if (!m_sessions.contains(cameraId)) {
        return 0;
    }
    return m_sessions[cameraId]->totalBytesTransferred;
}

QString PortForwarder::getConnectionStatus(const QString& cameraId) const
{
    if (!m_sessions.contains(cameraId)) {
        return "Not Active";
    }
    return m_sessions[cameraId]->status;
}

void PortForwarder::handleNewConnection()
{
    QTcpServer* server = qobject_cast<QTcpServer*>(sender());
    if (!server) {
        LOG_ERROR("handleNewConnection called with invalid server", "PortForwarder");
        return;
    }
    
    // Find which camera this server belongs to
    QString cameraId;
    ForwardingSession* session = nullptr;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value()->server == server) {
            cameraId = it.key();
            session = it.value();
            break;
        }
    }
    
    if (cameraId.isEmpty() || !session) {
        LOG_ERROR("Received connection for unknown server", "PortForwarder");
        return;
    }
    
    QTcpSocket* clientSocket = server->nextPendingConnection();
    if (!clientSocket) {
        LOG_ERROR("No pending connection available", "PortForwarder");
        return;
    }
    
    QString clientAddress = QString("%1:%2")
        .arg(clientSocket->peerAddress().toString())
        .arg(clientSocket->peerPort());
    
    LOG_INFO(QString("New client connection from %1 for camera '%2' [ID: %3]")
             .arg(clientAddress).arg(session->camera.name()).arg(cameraId), "PortForwarder");
    
    // Create connection info structure
    ConnectionInfo* connInfo = new ConnectionInfo;
    connInfo->clientSocket = clientSocket;
    connInfo->targetSocket = new QTcpSocket(this);
    connInfo->clientAddress = clientAddress;
    connInfo->bytesTransferred = 0;
    connInfo->connectedTime = QDateTime::currentDateTime();
    connInfo->isTargetConnected = false;
      // Store connection mapping
    session->connections[clientSocket] = connInfo;
    m_socketToCameraMap[clientSocket] = cameraId;
    m_socketToCameraMap[connInfo->targetSocket] = cameraId;
    
    // Optimize sockets for RTSP streaming
    optimizeSocketForStreaming(clientSocket);
    optimizeSocketForStreaming(connInfo->targetSocket);
    
    // Connect client socket signals
    connect(clientSocket, &QTcpSocket::disconnected, 
            this, &PortForwarder::handleClientDisconnected);
    connect(clientSocket, &QTcpSocket::readyRead, 
            this, &PortForwarder::handleClientDataReady);
    connect(clientSocket, &QTcpSocket::bytesWritten,  // Non-blocking write buffer flushing
            this, &PortForwarder::handleBytesWritten);
    connect(clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &PortForwarder::handleConnectionError);
    
    // Connect target socket signals
    connect(connInfo->targetSocket, &QTcpSocket::connected, 
            this, &PortForwarder::handleTargetConnected);
    connect(connInfo->targetSocket, &QTcpSocket::disconnected, 
            this, &PortForwarder::handleTargetDisconnected);
    connect(connInfo->targetSocket, &QTcpSocket::readyRead, 
            this, &PortForwarder::handleTargetDataReady);
    connect(connInfo->targetSocket, &QTcpSocket::bytesWritten,  // Non-blocking write buffer flushing
            this, &PortForwarder::handleBytesWritten);
    connect(connInfo->targetSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &PortForwarder::handleConnectionError);
      // Attempt connection to target camera
    LOG_DEBUG(QString("Connecting to target camera %1:%2 for client %3")
              .arg(session->camera.ipAddress())
              .arg(session->camera.port())
              .arg(clientAddress), "PortForwarder");
      // Set connection timeout for RTSP (extended timeout for better reliability)
    connInfo->targetSocket->connectToHost(session->camera.ipAddress(), session->camera.port());
    
    // Set connection timeout to 30 seconds for RTSP cameras
    QTimer::singleShot(30000, connInfo->targetSocket, [this, clientSocket, cameraId]() {
        if (!m_sessions.contains(cameraId)) return;
        
        ForwardingSession* session = m_sessions[cameraId];
        if (!session->connections.contains(clientSocket)) return;
        
        ConnectionInfo* info = session->connections[clientSocket];
        if (info && info->targetSocket && 
            info->targetSocket->state() == QAbstractSocket::ConnectingState) {
            LOG_WARNING(QString("Connection timeout to camera %1, aborting").arg(cameraId), "PortForwarder");
            info->targetSocket->abort();
        }
    });
    
    // Update session activity
    session->lastActivity = QDateTime::currentDateTime();
    updateSessionStatus(cameraId, QString("Active - %1 connections").arg(session->connections.size()));
    
    emit connectionEstablished(cameraId, clientAddress);
}

void PortForwarder::handleClientDisconnected()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) {
        LOG_ERROR("handleClientDisconnected called with invalid socket", "PortForwarder");
        return;
    }
    
    QString cameraId = m_socketToCameraMap.value(clientSocket);
    if (cameraId.isEmpty() || !m_sessions.contains(cameraId)) {
        LOG_DEBUG("Client disconnected for unknown camera", "PortForwarder");
        clientSocket->deleteLater();
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    ConnectionInfo* connInfo = session->connections.value(clientSocket);
    
    if (!connInfo) {
        LOG_ERROR("No connection info found for disconnecting client", "PortForwarder");
        clientSocket->deleteLater();
        return;
    }
    
    QString clientAddress = connInfo->clientAddress;
    LOG_INFO(QString("Client disconnected: %1 for camera '%2'")
             .arg(clientAddress).arg(session->camera.name()), "PortForwarder");
    
    // Log connection details before cleanup
    logConnectionDetails(cameraId, connInfo, "Client Disconnected");
    
    // Cleanup target socket
    if (connInfo->targetSocket) {
        m_socketToCameraMap.remove(connInfo->targetSocket);
        connInfo->targetSocket->disconnectFromHost();
        connInfo->targetSocket->deleteLater();
    }
    
    // Remove from session
    session->connections.remove(clientSocket);
    m_socketToCameraMap.remove(clientSocket);
    
    // Update session status
    updateSessionStatus(cameraId, QString("Active - %1 connections").arg(session->connections.size()));
    
    // Clean up connection info
    delete connInfo;
    
    emit connectionClosed(cameraId, clientAddress);
    clientSocket->deleteLater();
}

void PortForwarder::handleClientDataReady()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) {
        LOG_ERROR("handleClientDataReady called with invalid socket", "PortForwarder");
        return;
    }
    
    QString cameraId = m_socketToCameraMap.value(clientSocket);
    if (cameraId.isEmpty() || !m_sessions.contains(cameraId)) {
        LOG_DEBUG("Data ready for unknown camera connection", "PortForwarder");
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    ConnectionInfo* connInfo = session->connections.value(clientSocket);
    
    if (!connInfo || !connInfo->targetSocket) {
        LOG_ERROR("No target connection found for client data", "PortForwarder");
        return;
    }      if (connInfo->targetSocket->state() == QAbstractSocket::ConnectedState) {
        forwardData(clientSocket, connInfo->targetSocket, cameraId, "client->target");
    } else if (connInfo->targetSocket->state() == QAbstractSocket::ConnectingState) {
        // Buffer initial RTSP request data while target is connecting
        QByteArray data = clientSocket->readAll();
        if (!data.isEmpty()) {
            connInfo->pendingClientData.append(data);
            
            // Limit buffer size to prevent memory issues (32KB should be enough for RTSP handshake)
            if (connInfo->pendingClientData.size() > 32768) {
                LOG_WARNING(QString("Pending data buffer overflow for camera %1, discarding oldest data").arg(cameraId), "PortForwarder");
                connInfo->pendingClientData = connInfo->pendingClientData.right(16384); // Keep last 16KB
            }
            
            LOG_DEBUG(QString("Buffered %1 bytes of client data while connecting to camera %2 (total buffered: %3)")
                      .arg(data.size()).arg(cameraId).arg(connInfo->pendingClientData.size()), "PortForwarder");
        }
    } else {
        LOG_DEBUG(QString("Target not connected (state: %1), dropping data for camera: %2")
                  .arg(static_cast<int>(connInfo->targetSocket->state())).arg(cameraId), "PortForwarder");
        clientSocket->readAll(); // Discard data if not connecting
    }
}

void PortForwarder::handleTargetConnected()
{
    QTcpSocket* targetSocket = qobject_cast<QTcpSocket*>(sender());
    if (!targetSocket) {
        LOG_ERROR("handleTargetConnected called with invalid socket", "PortForwarder");
        return;
    }
    
    QString cameraId = m_socketToCameraMap.value(targetSocket);
    if (cameraId.isEmpty() || !m_sessions.contains(cameraId)) {
        LOG_ERROR("Target connected for unknown camera", "PortForwarder");
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];      // Find the connection info and mark target as connected
    for (auto it = session->connections.begin(); it != session->connections.end(); ++it) {
        ConnectionInfo* info = it.value();
        if (info && info->targetSocket == targetSocket) {
            info->isTargetConnected = true;
            
            // Optimize the connected socket for streaming
            optimizeSocketForStreaming(targetSocket);
            
            // Send any buffered client data that arrived before target connection
            if (!info->pendingClientData.isEmpty()) {
                LOG_INFO(QString("Sending %1 bytes of buffered data to camera %2")
                         .arg(info->pendingClientData.size()).arg(cameraId), "PortForwarder");
                
                qint64 bytesWritten = targetSocket->write(info->pendingClientData);
                if (bytesWritten == -1) {
                    LOG_ERROR(QString("Failed to send buffered data to camera %1: %2")
                              .arg(cameraId).arg(targetSocket->errorString()), "PortForwarder");
                } else {
                    if (bytesWritten != info->pendingClientData.size()) {
                        LOG_WARNING(QString("Partial write of buffered data: %1/%2 bytes for camera %3")
                                    .arg(bytesWritten).arg(info->pendingClientData.size()).arg(cameraId), "PortForwarder");
                    }
                    info->bytesTransferred += bytesWritten;
                    session->totalBytesTransferred += bytesWritten;
                    targetSocket->flush(); // Ensure data is sent immediately
                }
                
                info->pendingClientData.clear(); // Clear buffer after sending
            }
            
            LOG_INFO(QString("Successfully connected to camera '%1' at %2:%3 for client %4")
                     .arg(session->camera.name())
                     .arg(session->camera.ipAddress())
                     .arg(session->camera.port())
                     .arg(info->clientAddress), "PortForwarder");
            break;
        }
    }
    
    // Reset reconnect attempts on successful connection
    session->reconnectAttempts = 0;
    session->lastActivity = QDateTime::currentDateTime();
    updateSessionStatus(cameraId, QString("Connected - %1 active connections").arg(session->connections.size()));
}

void PortForwarder::handleTargetDisconnected()
{
    QTcpSocket* targetSocket = qobject_cast<QTcpSocket*>(sender());
    if (!targetSocket) return;
    
    QString cameraId = m_socketToCameraMap.value(targetSocket);
    if (cameraId.isEmpty() || !m_sessions.contains(cameraId)) {
        targetSocket->deleteLater();
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
      // Find and disconnect corresponding client
    QTcpSocket* clientSocket = nullptr;
    for (auto it = session->connections.begin(); it != session->connections.end(); ++it) {
        if (it.value()->targetSocket == targetSocket) {
            clientSocket = it.key();
            break;
        }
    }
    
    if (clientSocket) {
        session->connections.remove(clientSocket);
        m_socketToCameraMap.remove(clientSocket);
        clientSocket->disconnectFromHost();
        clientSocket->deleteLater();
    }
    
    m_socketToCameraMap.remove(targetSocket);
    targetSocket->deleteLater();
    
    // Setup reconnect if camera is still enabled
    if (session->camera.isEnabled() && !session->isReconnecting) {
        setupReconnectTimer(cameraId);
    }
}

void PortForwarder::handleTargetDataReady()
{
    QTcpSocket* targetSocket = qobject_cast<QTcpSocket*>(sender());
    if (!targetSocket) {
        LOG_ERROR("handleTargetDataReady called with invalid socket", "PortForwarder");
        return;
    }
    
    QString cameraId = m_socketToCameraMap.value(targetSocket);
    if (cameraId.isEmpty() || !m_sessions.contains(cameraId)) {
        LOG_DEBUG("Target data ready for unknown camera", "PortForwarder");
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    
    // Find corresponding client socket
    QTcpSocket* clientSocket = nullptr;
    ConnectionInfo* connInfo = nullptr;
    
    for (auto it = session->connections.begin(); it != session->connections.end(); ++it) {
        ConnectionInfo* info = it.value();
        if (info && info->targetSocket == targetSocket) {
            clientSocket = it.key();
            connInfo = info;
            break;
        }
    }
    
    if (!clientSocket || !connInfo) {
        LOG_ERROR("No client connection found for target data", "PortForwarder");
        return;
    }
    
    if (clientSocket->state() == QAbstractSocket::ConnectedState) {
        forwardData(targetSocket, clientSocket, cameraId, "target->client");
    } else {
        LOG_DEBUG(QString("Client not connected, dropping data for camera: %1").arg(cameraId), "PortForwarder");
    }
}

void PortForwarder::handleConnectionError(QAbstractSocket::SocketError error)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    QString cameraId = m_socketToCameraMap.value(socket);
    if (cameraId.isEmpty()) return;
    
    QString errorString = socket->errorString();
    
    // Log different error types with appropriate severity
    switch (error) {
        case QAbstractSocket::ConnectionRefusedError:
            LOG_ERROR(QString("Camera %1 connection refused: %2").arg(cameraId).arg(errorString), "PortForwarder");
            break;
        case QAbstractSocket::RemoteHostClosedError:
            LOG_INFO(QString("Camera %1 remote host closed connection: %2").arg(cameraId).arg(errorString), "PortForwarder");
            break;
        case QAbstractSocket::HostNotFoundError:
            LOG_ERROR(QString("Camera %1 host not found: %2").arg(cameraId).arg(errorString), "PortForwarder");
            break;
        case QAbstractSocket::SocketTimeoutError:
            LOG_WARNING(QString("Camera %1 connection timeout: %2").arg(cameraId).arg(errorString), "PortForwarder");
            break;
        case QAbstractSocket::NetworkError:
            LOG_WARNING(QString("Camera %1 network error: %2").arg(cameraId).arg(errorString), "PortForwarder");
            break;
        default:
            LOG_WARNING(QString("Camera %1 connection error (code %2): %3").arg(cameraId).arg(static_cast<int>(error)).arg(errorString), "PortForwarder");
            break;
    }
    
    // Only emit forwardingError for serious errors that should be reported to user
    if (error == QAbstractSocket::ConnectionRefusedError || 
        error == QAbstractSocket::HostNotFoundError ||
        error == QAbstractSocket::NetworkError) {
        emit forwardingError(cameraId, errorString);
    }
}

void PortForwarder::handleReconnectTimer()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    
    // Find which camera this timer belongs to
    QString cameraId;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value()->reconnectTimer == timer) {
            cameraId = it.key();
            break;
        }
    }
    
    if (cameraId.isEmpty()) return;
    
    ForwardingSession* session = m_sessions[cameraId];
    session->isReconnecting = false;
    
    LOG_INFO(QString("Reconnect timer expired for camera: %1").arg(session->camera.name()), "PortForwarder");
}

void PortForwarder::setupReconnectTimer(const QString& cameraId)
{
    if (!m_sessions.contains(cameraId)) return;
    
    ForwardingSession* session = m_sessions[cameraId];
    if (session->isReconnecting) return;
    
    session->isReconnecting = true;
    session->reconnectTimer->start();
    
    LOG_INFO(QString("Setup reconnect timer for camera: %1").arg(session->camera.name()), "PortForwarder");
}

void PortForwarder::forwardData(QTcpSocket* from, QTcpSocket* to, const QString& cameraId, const QString& direction)
{
    if (!from || !to || !from->isReadable() || !to->isWritable()) {
        return;
    }
    
    // Read available data in chunks to handle streaming properly
    QByteArray data = from->readAll();
    if (data.isEmpty()) {
        return;
    }
      // Log detailed information for RTSP debugging
    if (data.size() > 0) {
        // Enhanced RTSP protocol detection
        bool isRtspData = data.startsWith("RTSP/") || data.startsWith("OPTIONS ") || 
                         data.startsWith("DESCRIBE ") || data.startsWith("SETUP ") ||
                         data.startsWith("PLAY ") || data.startsWith("PAUSE ") ||
                         data.startsWith("TEARDOWN ") || data.startsWith("RECORD ") ||
                         data.startsWith("ANNOUNCE ") || data.startsWith("REDIRECT ") ||
                         data.startsWith("GET_PARAMETER ") || data.startsWith("SET_PARAMETER ");
        
        // Also check for interleaved RTP data (binary data with $ prefix)
        bool isRtpData = data.size() >= 4 && data[0] == '$';
        
        if (isRtspData) {
            LOG_INFO(QString("RTSP %1 data: %2 bytes - %3")
                      .arg(direction)
                      .arg(data.size())
                      .arg(QString::fromUtf8(data.left(150)).replace('\r', "\\r").replace('\n', "\\n")), 
                      "PortForwarder");
        } else if (isRtpData) {
            LOG_DEBUG(QString("RTP %1 data: %2 bytes [Channel: %3, Length: %4]")
                      .arg(direction)
                      .arg(data.size())
                      .arg(static_cast<unsigned char>(data[1]))
                      .arg((static_cast<unsigned char>(data[2]) << 8) | static_cast<unsigned char>(data[3])),
                      "PortForwarder");
        } else if (data.size() > 100) {
            LOG_DEBUG(QString("Binary %1 data: %2 bytes").arg(direction).arg(data.size()), "PortForwarder");
        }
    }
    
    // Write data with proper error handling for streaming
    // OPTIMIZATION: Use non-blocking writes for real-time video streaming
    // Previously used waitForBytesWritten(100) which could block for up to 100ms,
    // causing video frames to be dropped (at 30fps, 100ms = 3 dropped frames)
    
    qint64 totalWritten = 0;
    qint64 dataSize = data.size();
    
    // Try to write all data without blocking
    qint64 bytesWritten = to->write(data.constData(), dataSize);
    
    if (bytesWritten == -1) {
        LOG_ERROR(QString("Failed to write data %1 for camera %2: %3")
                  .arg(direction).arg(cameraId).arg(to->errorString()), "PortForwarder");
        return;
    }
    
    totalWritten = bytesWritten;
    
    // If we couldn't write all data at once, buffer the remaining data
    // The OS will notify us via bytesWritten() signal when more buffer space is available
    if (totalWritten < dataSize) {
        // Find connection info to buffer remaining data
        for (auto it = m_sessions[cameraId]->connections.begin(); 
             it != m_sessions[cameraId]->connections.end(); ++it) {
            ConnectionInfo* info = it.value();
            if ((direction == "client->target" && info->clientSocket == from && info->targetSocket == to) ||
                (direction == "target->client" && info->targetSocket == from && info->clientSocket == to)) {
                
                QByteArray* writeBuffer = (direction == "client->target") ? 
                    &info->pendingClientWrite : &info->pendingTargetWrite;
                
                // Append remaining data to buffer
                writeBuffer->append(data.constData() + totalWritten, dataSize - totalWritten);
                
                LOG_DEBUG(QString("Buffered %1 bytes (socket write buffer full) %2 for camera %3. Total buffered: %4")
                          .arg(dataSize - totalWritten).arg(direction).arg(cameraId).arg(writeBuffer->size()), 
                          "PortForwarder");
                break;
            }
        }
    }
    
    // Try to flush data for real-time streaming, but don't spam logs if it fails
    // Note: flush() failure is normal for high-throughput video streaming due to TCP buffering
    if (totalWritten > 0) {
        bool flushed = to->flush();
        
        // Only log flush failures occasionally to avoid spam (every 5 seconds max)
        if (!flushed) {
            static QHash<QString, qint64> lastFlushWarning;
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            QString key = cameraId + ":" + direction;
            
            if (!lastFlushWarning.contains(key) || currentTime - lastFlushWarning[key] > 5000) {
                LOG_DEBUG(QString("TCP buffer full for %1 on camera %2 (normal for video streaming)")
                          .arg(direction).arg(cameraId), "PortForwarder");
                lastFlushWarning[key] = currentTime;
            }
        }
    }
    
    if (totalWritten > 0) {
        // Update connection statistics
        if (m_sessions.contains(cameraId)) {
            ForwardingSession* session = m_sessions[cameraId];
            session->totalBytesTransferred += totalWritten;
            session->lastActivity = QDateTime::currentDateTime();
            
            // Update connection-specific stats
            QString socketCameraId = m_socketToCameraMap.value(from);
            if (socketCameraId == cameraId) {
                // Find the connection info
                for (auto it = session->connections.begin(); it != session->connections.end(); ++it) {
                    ConnectionInfo* info = it.value();
                    if ((direction == "client->target" && info->clientSocket == from) ||
                        (direction == "target->client" && info->targetSocket == from)) {
                        info->bytesTransferred += totalWritten;
                        break;
                    }
                }
            }
        }
        
        // Emit data transfer signal (throttled logging)
        static QHash<QString, qint64> lastLogTime;
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (!lastLogTime.contains(cameraId) || currentTime - lastLogTime[cameraId] > 5000) {
            LOG_DEBUG(QString("Data forwarded: %1 bytes %2 for camera %3")
                      .arg(totalWritten).arg(direction).arg(cameraId), "PortForwarder");
            lastLogTime[cameraId] = currentTime;
        }
        
        emit dataTransferred(cameraId, totalWritten, direction);
    } else {
        LOG_ERROR(QString("Failed to forward %1 bytes %2 for camera %3")
                  .arg(dataSize).arg(direction).arg(cameraId), "PortForwarder");
    }
}

void PortForwarder::setNetworkInterfaceManager(NetworkInterfaceManager* manager)
{
    if (m_networkManager) {
        disconnect(m_networkManager, nullptr, this, nullptr);
    }
    
    m_networkManager = manager;
    
    if (m_networkManager) {
        connect(m_networkManager, &NetworkInterfaceManager::interfacesChanged,
                this, &PortForwarder::onNetworkInterfacesChanged);
        connect(m_networkManager, &NetworkInterfaceManager::wireGuardInterfaceStateChanged,
                this, &PortForwarder::onWireGuardStateChanged);
    }
}

NetworkInterfaceManager* PortForwarder::networkInterfaceManager() const
{
    return m_networkManager;
}

bool PortForwarder::bindToAllInterfaces(QTcpServer* server, quint16 port)
{
    // First try IPv4 all interfaces (0.0.0.0)
    if (server->listen(QHostAddress::Any, port)) {
        LOG_INFO(QString("Successfully bound to all IPv4 interfaces (0.0.0.0:%1)").arg(port), "PortForwarder");
        return true;
    }
    
    // Try IPv6 all interfaces (::)
    if (server->listen(QHostAddress::AnyIPv6, port)) {
        LOG_INFO(QString("Successfully bound to all IPv6 interfaces ([::]:%1)").arg(port), "PortForwarder");
        return true;
    }
    
    LOG_WARNING(QString("Failed to bind to 0.0.0.0:%1 and [::]:%1, trying specific interfaces").arg(port), "PortForwarder");
    
    // If we have a network manager, try to bind to specific interfaces
    if (m_networkManager) {
        const auto activeInterfaces = m_networkManager->getActiveInterfaces();
        const auto addresses = m_networkManager->getAllAddresses();
        
        // Try to bind to each active interface address
        for (const QHostAddress& address : addresses) {
            if (server->listen(address, port)) {
                LOG_INFO(QString("Successfully bound to specific interface (%1:%2)")
                         .arg(address.toString()).arg(port), "PortForwarder");
                return true;
            }
        }
        
        // Try WireGuard interface specifically
        const QHostAddress wgAddress = m_networkManager->getWireGuardAddress();
        if (!wgAddress.isNull() && server->listen(wgAddress, port)) {
            LOG_INFO(QString("Successfully bound to WireGuard interface (%1:%2)")
                     .arg(wgAddress.toString()).arg(port), "PortForwarder");
            return true;
        }
    }
    
    // Last resort - try localhost
    if (server->listen(QHostAddress::LocalHost, port)) {
        LOG_WARNING(QString("Only bound to localhost (127.0.0.1:%1) - external access limited").arg(port), "PortForwarder");
        return true;
    }
    
    return false;
}

void PortForwarder::onNetworkInterfacesChanged()
{
    if (m_networkManager) {
        const QString status = m_networkManager->getInterfaceStatus();
        LOG_INFO(QString("Network interfaces changed: %1").arg(status), "PortForwarder");
    }
    
    // Consider restarting forwarding if we have active sessions
    // This is commented out to avoid disruption, but can be enabled if needed
    // restartAllForwarding();
}

void PortForwarder::onWireGuardStateChanged(bool active)
{
    const QString state = active ? "ACTIVE" : "INACTIVE";
    LOG_INFO(QString("WireGuard state changed to %1").arg(state), "PortForwarder");
    
    if (m_networkManager) {
        const QHostAddress wgAddress = m_networkManager->getWireGuardAddress();
        LOG_INFO(QString("WireGuard address: %1").arg(wgAddress.toString()), "PortForwarder");
    }
    
    // Optionally restart all forwarding when WireGuard state changes
    // This ensures we bind to the new WireGuard interface if it becomes available
    if (active && !m_sessions.isEmpty()) {
        LOG_INFO("WireGuard activated - restarting port forwarding to ensure proper binding", "PortForwarder");
        QTimer::singleShot(1000, this, &PortForwarder::restartAllForwarding); // Small delay to let interface stabilize
    }
}

void PortForwarder::restartAllForwarding()
{
    if (m_sessions.isEmpty()) return;
    
    LOG_INFO("Restarting all port forwarding sessions", "PortForwarder");
    
    // Save current camera configurations
    QList<CameraConfig> cameras;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        cameras.append(it.value()->camera);
    }
    
    // Stop all current forwarding
    stopAllForwarding();
      // Restart forwarding for each camera
    for (const CameraConfig& camera : cameras) {
        if (camera.isEnabled()) {
            QTimer::singleShot(500, this, [this, camera]() {
                startForwarding(camera);
            });
        }
    }
}

void PortForwarder::updateSessionStatus(const QString& cameraId, const QString& status)
{
    if (m_sessions.contains(cameraId)) {
        m_sessions[cameraId]->status = status;
        LOG_DEBUG(QString("Camera '%1' status: %2").arg(cameraId).arg(status), "PortForwarder");
    }
}

void PortForwarder::logConnectionDetails(const QString& cameraId, const ConnectionInfo* info, const QString& event)
{
    if (!info) return;
    
    qint64 durationMs = info->connectedTime.msecsTo(QDateTime::currentDateTime());
    double durationSec = durationMs / 1000.0;
    
    LOG_INFO(QString("%1 - Camera: %2, Client: %3, Duration: %4s, Bytes: %5")
             .arg(event)
             .arg(cameraId)
             .arg(info->clientAddress)
             .arg(QString::number(durationSec, 'f', 1))
             .arg(info->bytesTransferred), "PortForwarder");
}

void PortForwarder::cleanupConnection(const QString& cameraId, QTcpSocket* clientSocket)
{
    if (!m_sessions.contains(cameraId) || !clientSocket) {
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    ConnectionInfo* connInfo = session->connections.value(clientSocket);
    
    if (connInfo) {
        logConnectionDetails(cameraId, connInfo, "Cleanup");
        
        if (connInfo->targetSocket) {
            m_socketToCameraMap.remove(connInfo->targetSocket);
            connInfo->targetSocket->deleteLater();
        }
        
        delete connInfo;
    }
    
    session->connections.remove(clientSocket);
    m_socketToCameraMap.remove(clientSocket);
    clientSocket->deleteLater();
}

void PortForwarder::setupHealthCheckTimer(const QString& cameraId)
{
    if (!m_sessions.contains(cameraId)) {
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    if (session->healthCheckTimer) {
        session->healthCheckTimer->start();
        LOG_DEBUG(QString("Health check timer started for camera: %1").arg(cameraId), "PortForwarder");
    }
}

void PortForwarder::handleHealthCheck()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    
    // Find which camera this timer belongs to
    QString cameraId;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value()->healthCheckTimer == timer) {
            cameraId = it.key();
            break;
        }
    }
    
    if (cameraId.isEmpty()) {
        return;
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    
    // Log health status
    LOG_DEBUG(QString("Health check - Camera: %1, Connections: %2, Total bytes: %3, Status: %4")
              .arg(session->camera.name())
              .arg(session->connections.size())
              .arg(session->totalBytesTransferred)
              .arg(session->status), "PortForwarder");
    
    // Check for inactive connections (optional cleanup)
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-300); // 5 minutes
    QList<QTcpSocket*> toRemove;
    
    for (auto it = session->connections.begin(); it != session->connections.end(); ++it) {
        ConnectionInfo* info = it.value();
        if (info && info->connectedTime < cutoff && info->bytesTransferred == 0) {
            LOG_WARNING(QString("Removing inactive connection: %1").arg(info->clientAddress), "PortForwarder");
            toRemove.append(it.key());
        }
    }
    
    for (QTcpSocket* socket : toRemove) {
        cleanupConnection(cameraId, socket);
    }
}

void PortForwarder::optimizeSocketForStreaming(QTcpSocket* socket)
{
    if (!socket) return;
    
    // Set socket options for optimal RTSP streaming performance
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);  // TCP_NODELAY equivalent - critical for RTSP
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);  // SO_KEEPALIVE to detect dead connections
    
    // Set larger buffer sizes for streaming data
    socket->setReadBufferSize(128 * 1024);  // 128KB read buffer for video data
    socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 256 * 1024);  // 256KB send buffer
    socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 256 * 1024);  // 256KB receive buffer
    
    // Disable proxy for direct connection
    socket->setProxy(QNetworkProxy::NoProxy);
    
    // Set type of service for real-time data (if supported)
    socket->setSocketOption(QAbstractSocket::TypeOfServiceOption, 0x10); // IPTOS_LOWDELAY
    
    LOG_DEBUG("Socket optimized for RTSP streaming with enhanced buffer sizes", "PortForwarder");
}

void PortForwarder::handleBytesWritten()
{
    // This slot is called when a socket is ready to accept more data
    // We use it to flush any buffered write data for non-blocking I/O
    
    QTcpSocket* writableSocket = qobject_cast<QTcpSocket*>(sender());
    if (!writableSocket) return;
    
    // Find which connection this socket belongs to
    for (auto sessionIt = m_sessions.begin(); sessionIt != m_sessions.end(); ++sessionIt) {
        ForwardingSession* session = sessionIt.value();
        
        for (auto connIt = session->connections.begin(); connIt != session->connections.end(); ++connIt) {
            ConnectionInfo* info = connIt.value();
            
            // Check if this is a client or target socket
            QByteArray* writeBuffer = nullptr;
            QString direction;
            
            if (info->clientSocket == writableSocket && !info->pendingClientWrite.isEmpty()) {
                writeBuffer = &info->pendingClientWrite;
                direction = "client->target (buffered)";
            } else if (info->targetSocket == writableSocket && !info->pendingTargetWrite.isEmpty()) {
                writeBuffer = &info->pendingTargetWrite;
                direction = "target->client (buffered)";
            }
            
            if (writeBuffer && !writeBuffer->isEmpty()) {
                // Try to write as much buffered data as possible
                qint64 bytesWritten = writableSocket->write(writeBuffer->constData(), writeBuffer->size());
                
                if (bytesWritten > 0) {
                    // Remove the written bytes from buffer
                    writeBuffer->remove(0, bytesWritten);
                    
                    LOG_DEBUG(QString("Flushed %1 bytes %2 for camera %3. Buffer remaining: %4")
                              .arg(bytesWritten).arg(direction).arg(sessionIt.key()).arg(writeBuffer->size()), 
                              "PortForwarder");
                } else if (bytesWritten < 0) {
                    LOG_ERROR(QString("Failed to flush buffered data %1 for camera %2: %3")
                              .arg(direction).arg(sessionIt.key()).arg(writableSocket->errorString()), 
                              "PortForwarder");
                }
                // Continue checking other buffers for this socket
            }
        }
    }
}
QString PortForwarder::getBindingInfo(const QString& cameraId) const
{
    if (!m_sessions.contains(cameraId)) {
        return "Camera session not found";
    }
    
    ForwardingSession* session = m_sessions[cameraId];
    if (!session->server) {
        return "Server not initialized";
    }
    
    QString info = QString("Listening on %1:%2")
                   .arg(session->server->serverAddress().toString())
                   .arg(session->server->serverPort());
    
    // Check if bound to all interfaces
    if (session->server->serverAddress() == QHostAddress::Any) {
        info += " (All IPv4 interfaces)";
    } else if (session->server->serverAddress() == QHostAddress::AnyIPv6) {
        info += " (All IPv6 interfaces)";
    } else {
        info += " (Specific interface only)";
    }
    
    return info;
}
