#ifndef VPNWIDGET_H
#define VPNWIDGET_H

#include <QWidget>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "WireGuardManager.h" // Full include for WireGuardManager::ConnectionStatus enum

// Forward-declare Qt classes to reduce header dependencies and improve compile times
QT_BEGIN_NAMESPACE
class QGroupBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QTextEdit;
class QProcess;
class QTimer;
class QVBoxLayout;
QT_END_NAMESPACE

class VpnWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VpnWidget(QWidget *parent = nullptr);
    ~VpnWidget();

    // Public methods for external access
    WireGuardManager::ConnectionStatus getConnectionStatus() const;
    QString getCurrentConfigName() const;
    bool isConnected() const;
    void connectToNetwork();
    void disconnectFromNetwork();
    void disconnectAndCleanupOnLogout();  // New method for logout disconnect + cleanup
    WireGuardManager* getWireGuardManager() const;
    
    // Always-connected functionality  
    void onLoginSuccessful();  // Called when user logs in

signals:
    void statusChanged(const QString& status);
    void logMessage(const QString& message);

private slots:
    // Legacy manual connection slots (kept for compatibility)
    void onConnectClicked();
    void onDisconnectClicked();
    void onPingTestClicked();
    
    // Network slots for config fetching
    void onConfigFetchFinished();
    void onConfigFetchError(QNetworkReply::NetworkError error);
    void onAutoConnectConfigReceived();

    // Slots for QProcess (Ping)
    void onPingFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onPingError(QProcess::ProcessError error);
    
    // Slots for WireGuardManager signals
    void onConnectionStatusChanged(WireGuardManager::ConnectionStatus status);
    void onTransferStatsUpdated(uint64_t rxBytes, uint64_t txBytes);
    void onWireGuardError(const QString& error);
    void onWireGuardLogMessage(const QString& message);

    // Internal timer slot
    void updateConnectionStatus();

private:    // UI Setup
    void setupUI();
    void setupConnectionGroup();
    void setupStatusGroup();
    void setupPingTestGroup();
    void connectSignals();

    // UI State Management
    void updateUI();
    QString getStatusText(WireGuardManager::ConnectionStatus status);
    QPixmap getStatusIcon(WireGuardManager::ConnectionStatus status);
    // Config management
    void fetchWireGuardConfig();
    void fetchWireGuardConfigForAutoConnect();
    void saveWireGuardConfig(const QString& configContent);
    QString getSavedWireGuardConfig();
    QString getWireGuardConfigPath();
    void validateAndConnect();
    void autoConnect();
    
    // Core components
    WireGuardManager* m_wireGuardManager;
    QTimer* m_statusUpdateTimer;
    QProcess* m_pingProcess;
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_configReply;    // UI Components (pointers managed by Qt's parent-child system)
    QVBoxLayout* m_mainLayout;
    QGroupBox* m_connectionGroup;
    QGroupBox* m_statusGroup;
    QGroupBox* m_pingTestGroup;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_pingTestButton;
    QProgressBar* m_connectionProgress;
    QLabel* m_connectionStatusLabel;
    QLabel* m_connectionIconLabel;
    QLabel* m_currentConfigLabel;
    QLabel* m_uptimeLabel;
    QLabel* m_transferLabel;
    QLabel* m_pingStatusLabel;
    QLabel* m_autoModeLabel;
    
    // State tracking
    QString m_loadedConfigPath;
    QDateTime m_connectionStartTime;
    bool m_autoConnectMode;
    bool m_autoConnectInProgress;
    QTimer* m_reconnectTimer;
};

#endif // VPNWIDGET_H
