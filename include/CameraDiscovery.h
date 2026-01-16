#ifndef CAMERADISCOVERY_H
#define CAMERADISCOVERY_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHostAddress>
#include <QTimer>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QThread>
#include <QMutex>
#include <QStringList>

// Discovered camera information
struct DiscoveredCamera
{
    QString ipAddress;
    int port;
    QString brand;          // CP Plus, Hikvision, Generic
    QString model;
    QString macAddress;
    QString deviceName;
    QString rtspUrl;        // Suggested RTSP URL format
    QStringList supportedPorts; // Common ports found open
    bool isOnline;
    int responseTime;       // Ping response time in ms

    DiscoveredCamera() : port(554), isOnline(false), responseTime(-1) {}
};

class NetworkScanner : public QThread
{
    Q_OBJECT

public:
    explicit NetworkScanner(const QString& networkRange, QObject *parent = nullptr);
    void setPortRange(const QList<int>& ports);
    void stop();

protected:
    void run() override;

signals:
    void deviceFound(const QString& ipAddress, int port);
    void scanProgress(int current, int total);
    void scanFinished();

private:
    QString m_networkRange;
    QList<int> m_ports;
    bool m_shouldStop;
    QMutex m_mutex;
};

class CameraDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit CameraDiscovery(QObject *parent = nullptr);
    ~CameraDiscovery();

    // Discovery methods
    void startDiscovery();
    void startDiscovery(const QString& networkRange);
    void stopDiscovery();
    
    // Configuration
    void setNetworkRange(const QString& range);
    void setTimeout(int milliseconds);
    void setMaxConcurrentRequests(int count);

    // State
    bool isDiscovering() const;
    QList<DiscoveredCamera> getDiscoveredCameras() const;
    void clearDiscoveredCameras();

    // Static utility methods
    static QString detectNetworkRange();
    static QString brandFromResponse(const QString& response, const QString& userAgent = QString());
    static QString generateRtspUrl(const QString& brand, const QString& ipAddress, int port = 554);
    static QStringList getCommonRtspPaths(const QString& brand);

signals:
    void discoveryStarted();
    void discoveryFinished();
    void discoveryProgress(int current, int total);
    void cameraDiscovered(const DiscoveredCamera& camera);
    void error(const QString& errorMessage);

private slots:
    void onDeviceFound(const QString& ipAddress, int port);
    void onHttpResponse();
    void onHttpError(QNetworkReply::NetworkError error);
    void onScanProgress(int current, int total);
    void onScanFinished();
    void onPingFinished();

private:
    // Network scanning
    void initializeScanner();
    void startNetworkScan();
    QString getDefaultNetworkRange();
    
    // Device identification
    void identifyDevice(const QString& ipAddress, int port);
    void sendHttpRequest(const QString& ipAddress, int port, const QString& path = "/");
    void sendOnvifDiscovery(const QString& ipAddress);
    void performDevicePing(const QString& ipAddress);
    
    // Response analysis
    DiscoveredCamera analyzeHttpResponse(const QString& ipAddress, int port, 
                                       const QString& response, const QString& headers);
    QString detectBrand(const QString& response, const QString& headers);
    QString detectModel(const QString& response, const QString& headers, const QString& brand);
    QString extractDeviceName(const QString& response, const QString& headers);
    
    // Brand-specific detection
    bool isHikvision(const QString& response, const QString& headers);
    bool isCPPlus(const QString& response, const QString& headers);
    QString getHikvisionModel(const QString& response);
    QString getCPPlusModel(const QString& response);

private:
    QNetworkAccessManager* m_networkManager;
    NetworkScanner* m_scanner;
    QList<DiscoveredCamera> m_discoveredCameras;
    
    // Configuration
    QString m_networkRange;
    int m_timeout;
    int m_maxConcurrentRequests;
    int m_currentRequests;
    
    // State
    bool m_isDiscovering;
    int m_totalHosts;
    int m_scannedHosts;
    
    // Common camera ports
    QList<int> m_cameraPorts;
      // Pending operations
    QHash<QNetworkReply*, QPair<QString, int>> m_pendingRequests;
    mutable QMutex m_dataMutex;
};

#endif // CAMERADISCOVERY_H
