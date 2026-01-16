#ifndef ECHOSERVER_H
#define ECHOSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QHash>

class EchoServer : public QObject
{
    Q_OBJECT

public:
    explicit EchoServer(QObject *parent = nullptr);
    ~EchoServer();

    // Server control
    bool startServer(quint16 port = 7777, const QHostAddress& address = QHostAddress::Any);
    void stopServer();
    bool isRunning() const;
    
    // Server information
    quint16 serverPort() const;
    QHostAddress serverAddress() const;
    int connectionCount() const;
    
    // Statistics
    quint64 totalBytesReceived() const;
    quint64 totalBytesSent() const;
    quint64 totalConnections() const;

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void clientConnected(const QString& clientAddress);
    void clientDisconnected(const QString& clientAddress);
    void dataEchoed(const QString& clientAddress, int bytesEchoed);
    void errorOccurred(const QString& error);

private slots:
    void handleNewConnection();
    void handleClientDisconnected();
    void handleClientDataReady();
    void handleSocketError();

private:
    void resetStatistics();
    QString getClientKey(QTcpSocket* socket) const;
    
    QTcpServer* m_server;
    QHash<QTcpSocket*, QString> m_clients; // socket -> client address
    
    // Statistics
    quint64 m_totalBytesReceived;
    quint64 m_totalBytesSent;
    quint64 m_totalConnections;
    
    static const int MAX_CONCURRENT_CONNECTIONS = 50;
    static const int CLIENT_TIMEOUT_MS = 30000; // 30 seconds
};

#endif // ECHOSERVER_H
