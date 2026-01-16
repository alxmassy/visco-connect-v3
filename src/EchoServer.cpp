#include "EchoServer.h"
#include "Logger.h"
#include <QDebug>

EchoServer::EchoServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_totalBytesReceived(0)
    , m_totalBytesSent(0)
    , m_totalConnections(0)
{
    connect(m_server, &QTcpServer::newConnection, this, &EchoServer::handleNewConnection);
}

EchoServer::~EchoServer()
{
    stopServer();
}

bool EchoServer::startServer(quint16 port, const QHostAddress& address)
{
    if (isRunning()) {
        LOG_WARNING("Echo server is already running", "EchoServer");
        return true;
    }
    
    if (!m_server->listen(address, port)) {
        const QString error = QString("Failed to start echo server on %1:%2 - %3")
                              .arg(address.toString())
                              .arg(port)
                              .arg(m_server->errorString());
        LOG_ERROR(error, "EchoServer");
        emit errorOccurred(error);
        return false;
    }
    
    resetStatistics();
    
    LOG_INFO(QString("Echo server started on %1:%2")
             .arg(address.toString())
             .arg(m_server->serverPort()), "EchoServer");
    
    emit serverStarted(m_server->serverPort());
    return true;
}

void EchoServer::stopServer()
{
    if (!isRunning()) return;
    
    // Disconnect all clients
    const auto clients = m_clients.keys();
    for (QTcpSocket* client : clients) {
        client->disconnectFromHost();
        client->deleteLater();
    }
    m_clients.clear();
    
    m_server->close();
    
    LOG_INFO(QString("Echo server stopped. Statistics: %1 connections, %2 bytes received, %3 bytes sent")
             .arg(m_totalConnections)
             .arg(m_totalBytesReceived)
             .arg(m_totalBytesSent), "EchoServer");
    
    emit serverStopped();
}

bool EchoServer::isRunning() const
{
    return m_server->isListening();
}

quint16 EchoServer::serverPort() const
{
    return m_server->serverPort();
}

QHostAddress EchoServer::serverAddress() const
{
    return m_server->serverAddress();
}

int EchoServer::connectionCount() const
{
    return m_clients.size();
}

quint64 EchoServer::totalBytesReceived() const
{
    return m_totalBytesReceived;
}

quint64 EchoServer::totalBytesSent() const
{
    return m_totalBytesSent;
}

quint64 EchoServer::totalConnections() const
{
    return m_totalConnections;
}

void EchoServer::handleNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* client = m_server->nextPendingConnection();
        
        if (m_clients.size() >= MAX_CONCURRENT_CONNECTIONS) {
            LOG_WARNING("Echo server: Maximum connections reached, rejecting client", "EchoServer");
            client->disconnectFromHost();
            client->deleteLater();
            continue;
        }
        
        const QString clientAddress = QString("%1:%2")
                                      .arg(client->peerAddress().toString())
                                      .arg(client->peerPort());
        
        m_clients[client] = clientAddress;
        m_totalConnections++;
        
        // Connect client signals
        connect(client, &QTcpSocket::disconnected, this, &EchoServer::handleClientDisconnected);
        connect(client, &QTcpSocket::readyRead, this, &EchoServer::handleClientDataReady);
        connect(client, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
                this, &EchoServer::handleSocketError);
        
        // Set up client timeout
        QTimer::singleShot(CLIENT_TIMEOUT_MS, client, [this, client]() {
            if (m_clients.contains(client)) {
                LOG_DEBUG("Echo server: Client connection timed out", "EchoServer");
                client->disconnectFromHost();
            }
        });
        
        LOG_DEBUG(QString("Echo server: New client connected from %1 (total: %2)")
                  .arg(clientAddress)
                  .arg(m_clients.size()), "EchoServer");
        
        emit clientConnected(clientAddress);
    }
}

void EchoServer::handleClientDisconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client || !m_clients.contains(client)) return;
    
    const QString clientAddress = m_clients[client];
    m_clients.remove(client);
    
    LOG_DEBUG(QString("Echo server: Client disconnected from %1 (remaining: %2)")
              .arg(clientAddress)
              .arg(m_clients.size()), "EchoServer");
    
    emit clientDisconnected(clientAddress);
    client->deleteLater();
}

void EchoServer::handleClientDataReady()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client || !m_clients.contains(client)) return;
    
    const QByteArray data = client->readAll();
    if (data.isEmpty()) return;
    
    m_totalBytesReceived += data.size();
    
    // Echo the data back to the client
    const qint64 bytesWritten = client->write(data);
    if (bytesWritten > 0) {
        m_totalBytesSent += bytesWritten;
        
        const QString clientAddress = m_clients[client];
        emit dataEchoed(clientAddress, static_cast<int>(bytesWritten));
        
        LOG_DEBUG(QString("Echo server: Echoed %1 bytes to %2")
                  .arg(bytesWritten)
                  .arg(clientAddress), "EchoServer");
    }
}

void EchoServer::handleSocketError()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;
    
    const QString error = QString("Echo server socket error: %1").arg(client->errorString());
    LOG_WARNING(error, "EchoServer");
    emit errorOccurred(error);
}

void EchoServer::resetStatistics()
{
    m_totalBytesReceived = 0;
    m_totalBytesSent = 0;
    m_totalConnections = 0;
}

QString EchoServer::getClientKey(QTcpSocket* socket) const
{
    return QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());
}
