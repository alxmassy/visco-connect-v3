#ifndef PINGRESPONDER_H
#define PINGRESPONDER_H

#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <QHostAddress>
#include <QThread>
#include <QMutex>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#endif

class PingResponder : public QObject
{
    Q_OBJECT

public:
    explicit PingResponder(QObject *parent = nullptr);
    ~PingResponder();

    // Control methods
    bool startResponder();
    void stopResponder();
    bool isRunning() const;
    
    // Configuration
    void setResponseEnabled(bool enabled);
    bool isResponseEnabled() const;
    
    // Statistics
    quint64 totalPingsReceived() const;
    quint64 totalPingsReplied() const;
    quint32 getResponseTimeMs() const;

signals:
    void pingReceived(const QString& sourceAddress, quint16 identifier, quint16 sequence);
    void pingReplied(const QString& sourceAddress, quint16 identifier, quint16 sequence, quint32 responseTime);
    void errorOccurred(const QString& error);
    void started();
    void stopped();

private slots:
    void processPingData();
    void checkSocketStatus();

private:
    void initializeWinsock();
    void cleanupWinsock();
    bool createRawSocket();
    void closeSocket();
    bool processIcmpPacket(const char* buffer, int length, const QString& sourceAddress);
    bool sendPingReply(const QString& targetAddress, quint16 identifier, quint16 sequence, const QByteArray& originalData);
    
    // Platform-specific implementation
#ifdef Q_OS_WIN
    SOCKET m_rawSocket;
    bool m_winsockInitialized;
#else
    int m_rawSocket;
#endif
    
    QSocketNotifier* m_socketNotifier;
    QTimer* m_statusTimer;
    
    bool m_running;
    bool m_responseEnabled;
    
    // Statistics
    quint64 m_totalPingsReceived;
    quint64 m_totalPingsReplied;
    quint64 m_startTime;
    
    // Thread safety
    QMutex m_mutex;
    
    static const int ICMP_ECHO = 8;
    static const int ICMP_ECHOREPLY = 0;
    static const int IP_HEADER_SIZE = 20;
    static const int ICMP_HEADER_SIZE = 8;
};

#endif // PINGRESPONDER_H
