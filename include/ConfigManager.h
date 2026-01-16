#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QList>
#include "CameraConfig.h"

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager& instance();
    
    bool loadConfig();
    bool saveConfig();
    
    // Camera management
    void addCamera(const CameraConfig& camera);
    void updateCamera(const QString& id, const CameraConfig& camera);
    void removeCamera(const QString& id);
    QList<CameraConfig> getAllCameras() const;
    CameraConfig getCamera(const QString& id) const;
    
    // User-specific configuration management
    void switchToUser(const QString& userEmail);
    void clearCurrentUserCameras();
    QString getCurrentUserEmail() const { return m_currentUserEmail; }
      // Settings
    bool isAutoStartEnabled() const { return m_autoStartEnabled; }
    void setAutoStartEnabled(bool enabled);
    
    // Echo server settings
    bool isEchoServerEnabled() const { return m_echoServerEnabled; }
    void setEchoServerEnabled(bool enabled);
    int getEchoServerPort() const { return m_echoServerPort; }
    void setEchoServerPort(int port);
    
    // API settings
    QString getApiBaseUrl() const { return m_apiBaseUrl; }
    void setApiBaseUrl(const QString& url);
    
    int getNextExternalPort() const;
    
    // File paths
    QString getConfigFilePath() const;
    QString getLogFilePath() const;

signals:
    void configChanged();
    void userSwitched(const QString& userEmail);

private:
    ConfigManager();
    ~ConfigManager();
    
    void createDefaultConfig();
    void updateWindowsAutoStart();
    QString getUserConfigFilePath(const QString& userEmail) const;
    void loadUserSpecificConfig(const QString& userEmail);
    void saveUserSpecificConfig(const QString& userEmail);
      QList<CameraConfig> m_cameras;
    bool m_autoStartEnabled;
    bool m_echoServerEnabled;
    int m_echoServerPort;
    QString m_apiBaseUrl;
    QString m_configFilePath;
    QString m_logFilePath;
    QString m_currentUserEmail; // Track current user for user-specific configs
};

#endif // CONFIGMANAGER_H
