#ifndef WIREGUARDMANAGER_H
#define WIREGUARDMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QProcess>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>
#include <QThread>
#include <windows.h>

struct WireGuardKeypair {
    QString publicKey;
    QString privateKey;
    
    WireGuardKeypair() = default;
    WireGuardKeypair(const QString& pub, const QString& priv) 
        : publicKey(pub), privateKey(priv) {}
    
    bool isValid() const {
        return !publicKey.isEmpty() && !privateKey.isEmpty();
    }
};

struct WireGuardPeer {
    QString publicKey;
    QString presharedKey;
    QString endpoint;
    QStringList allowedIPs;
    uint16_t persistentKeepalive = 25;  // Default to 25 seconds to keep NAT firewall hole open (prevents "retry" issues)
    uint64_t rxBytes = 0;
    uint64_t txBytes = 0;
    QDateTime lastHandshake;
};

struct WireGuardInterface {
    QString name;
    QString privateKey;
    QString publicKey;
    uint16_t listenPort = 0;
    uint16_t mtu = 1280;  // Default MTU to 1280 to prevent video packet fragmentation
    QStringList addresses;
    QStringList dns;
    QList<WireGuardPeer> peers;
};

struct WireGuardConfig {
    WireGuardInterface interfaceConfig;
    QString configFilePath;
    bool isActive = false;
    QDateTime createdAt;
    QDateTime lastConnectedAt;
};

class WireGuardManager : public QObject
{
    Q_OBJECT

public:
    enum ConnectionStatus {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };
    Q_ENUM(ConnectionStatus)

    explicit WireGuardManager(QObject *parent = nullptr);
    ~WireGuardManager();

    // Key generation
    WireGuardKeypair generateKeypair();
    QString generatePublicKey(const QString& privateKey);

    // Configuration management
    bool saveConfig(const WireGuardConfig& config);
    WireGuardConfig loadConfig(const QString& configName);
    QStringList getAvailableConfigs();
    bool deleteConfig(const QString& configName);
    
    // Connection management
    bool connectTunnel(const QString& configName);
    bool disconnectTunnel(const QString& configName = QString());
    ConnectionStatus getConnectionStatus() const;
    QString getCurrentConfigName() const;
    QString getCurrentTunnelIp() const;
    
    // Configuration parsing
    WireGuardConfig parseConfigFile(const QString& filePath);
    QString configToString(const WireGuardConfig& config);
    bool isValidConfig(const WireGuardConfig& config);
    
    // Statistics and monitoring
    WireGuardInterface getAdapterInfo(const QString& adapterName);
    QPair<uint64_t, uint64_t> getTransferStats(); // Returns (rx, tx)
    
    // Utility functions
    QString formatBytes(uint64_t bytes);
    bool isDllsAvailable();
    QString getConfigDirectory() const;

signals:
    void connectionStatusChanged(ConnectionStatus status);
    void transferStatsUpdated(uint64_t rxBytes, uint64_t txBytes);
    void configurationChanged();
    void errorOccurred(const QString& error);
    void logMessage(const QString& message);

private slots:
    void updateTransferStats();
    void checkConnectionStatus();

private:
    // DLL function declarations
    typedef bool (*WireGuardGenerateKeypairFunc)(BYTE* publicKey, BYTE* privateKey);
    typedef bool (*WireGuardTunnelServiceFunc)(LPCWSTR configFile);
    typedef HANDLE (*WireGuardOpenAdapterFunc)(LPCWSTR name);
    typedef void (*WireGuardCloseAdapterFunc)(HANDLE adapter);
    typedef bool (*WireGuardGetConfigurationFunc)(HANDLE adapter, BYTE* iface, DWORD* bytes);

    // DLL handles and functions
    HMODULE m_tunnelDll;
    HMODULE m_wireguardDll;
    WireGuardGenerateKeypairFunc m_generateKeypairFunc;
    WireGuardTunnelServiceFunc m_tunnelServiceFunc;
    WireGuardOpenAdapterFunc m_openAdapterFunc;
    WireGuardCloseAdapterFunc m_closeAdapterFunc;
    WireGuardGetConfigurationFunc m_getConfigurationFunc;

    // Windows Service Management
    bool createTunnelService(const QString& configPath, const QString& serviceName);
    bool removeTunnelService(const QString& serviceName);
    bool startTunnelService(const QString& serviceName);
    bool stopTunnelService(const QString& serviceName);
    QString generateServiceName(const QString& configName);
    
    // Internal state
    ConnectionStatus m_connectionStatus;
    QString m_currentConfigName;
    QString m_currentServiceName;
    QString m_configDirectory;
    QTimer* m_statsTimer;
    QTimer* m_statusTimer;
    QMutex m_mutex;
    
    // Helper functions
    bool loadDlls();
    void unloadDlls();
    bool initializeConfigDirectory();
    QString base64Encode(const QByteArray& data);
    QByteArray base64Decode(const QString& data);
    bool writeConfigFile(const QString& filePath, const WireGuardConfig& config);
    WireGuardConfig readConfigFile(const QString& filePath);
    
    // Constants
    static const QString CONFIG_DIR_NAME;
    static const QString CONFIG_FILE_EXTENSION;
    static const int STATS_UPDATE_INTERVAL;
    static const int STATUS_CHECK_INTERVAL;
};

#endif // WIREGUARDMANAGER_H
