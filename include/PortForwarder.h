#ifndef PORTFORWARDER_H
#define PORTFORWARDER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QTimer>
#include <QHash>
#include <QHostAddress>
#include "CameraConfig.h"

class NetworkInterfaceManager;

class PortForwarder : public QObject
{
    Q_OBJECT

public:
    explicit PortForwarder(QObject *parent = nullptr);    ~PortForwarder();
      bool startForwarding(const CameraConfig& camera);
    void stopForwarding(const QString& cameraId);
    void stopAllForwarding();
    void restartForwarding(const QString& cameraId);
    
    bool isForwarding(const QString& cameraId) const;
    QStringList getActiveForwards() const;
    
    // Port management
    bool isPortInUse(int port) const;
    int getNextAvailablePort(int startPort = 8551) const;
    bool changeExternalPort(const QString& cameraId, int newPort);
      // Connection statistics
    int getConnectionCount(const QString& cameraId) const;
    qint64 getBytesTransferred(const QString& cameraId) const;
    QString getConnectionStatus(const QString& cameraId) const;
    QString getBindingInfo(const QString& cameraId) const;

    // Network interface management
    void setNetworkInterfaceManager(NetworkInterfaceManager* manager);
    NetworkInterfaceManager* networkInterfaceManager() const;

signals:
    void forwardingStarted(const QString& cameraId, int externalPort);
    void forwardingStopped(const QString& cameraId);
    void forwardingError(const QString& cameraId, const QString& error);
    void connectionEstablished(const QString& cameraId, const QString& clientAddress);
    void connectionClosed(const QString& cameraId, const QString& clientAddress);
    void dataTransferred(const QString& cameraId, qint64 bytes, const QString& direction);
    void reconnectionAttempt(const QString& cameraId, int attemptNumber);
    void portChanged(const QString& cameraId, int oldPort, int newPort);

private slots:
    void handleNewConnection();
    void handleClientDisconnected();
    void handleClientDataReady();
    void handleTargetConnected();
    void handleTargetDisconnected();
    void handleTargetDataReady();
    void handleConnectionError(QAbstractSocket::SocketError error);    void handleReconnectTimer();    void onNetworkInterfacesChanged();
    void onWireGuardStateChanged(bool active);
    void handleHealthCheck();

private:    struct ConnectionInfo {
        QTcpSocket* clientSocket;
        QTcpSocket* targetSocket;
        QString clientAddress;
        qint64 bytesTransferred;
        QDateTime connectedTime;
        bool isTargetConnected;
        QByteArray pendingClientData;  // Buffer for data received before target connection
    };
    
    struct ForwardingSession {
        QTcpServer* server;
        CameraConfig camera;
        QHash<QTcpSocket*, ConnectionInfo*> connections; // client -> connection info
        QTimer* reconnectTimer;
        QTimer* healthCheckTimer;
        bool isReconnecting;
        int reconnectAttempts;
        qint64 totalBytesTransferred;
        QDateTime lastActivity;
        QString status;
    };
      void setupReconnectTimer(const QString& cameraId);
    void setupHealthCheckTimer(const QString& cameraId);
    void cleanupSession(const QString& cameraId);
    void cleanupConnection(const QString& cameraId, QTcpSocket* clientSocket);    void forwardData(QTcpSocket* from, QTcpSocket* to, const QString& cameraId, const QString& direction);
    void optimizeSocketForStreaming(QTcpSocket* socket);
    bool bindToAllInterfaces(QTcpServer* server, quint16 port);
    void restartAllForwarding();
    void updateSessionStatus(const QString& cameraId, const QString& status);
    void logConnectionDetails(const QString& cameraId, const ConnectionInfo* info, const QString& event);
    
    QHash<QString, ForwardingSession*> m_sessions;
    QHash<QTcpSocket*, QString> m_socketToCameraMap;
    NetworkInterfaceManager* m_networkManager;
    
    // Constants
    static const int MAX_RECONNECT_ATTEMPTS = 10;
    static const int RECONNECT_INTERVAL_MS = 5000;
    static const int HEALTH_CHECK_INTERVAL_MS = 30000;
};

#endif // PORTFORWARDER_H
