#include "ConfigManager.h"
#include "Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ConfigManager::ConfigManager()
    : m_autoStartEnabled(false)
    , m_echoServerEnabled(true)
    , m_echoServerPort(7777)
    , m_apiBaseUrl("http://54.225.63.242:8086")
    , m_currentUserEmail("")
{
    // Set up file paths
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(appDataPath);
    
    m_configFilePath = appDataPath + "/config.json";
    m_logFilePath = appDataPath + "/visco-connect.log";
}

ConfigManager::~ConfigManager()
{
    saveConfig();
}

ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadConfig()
{
    QFile file(m_configFilePath);
    if (!file.exists()) {
        LOG_INFO("Config file does not exist, creating default configuration", "Config");
        createDefaultConfig();
        return saveConfig();
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Failed to open config file: %1").arg(file.errorString()), "Config");
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("Failed to parse config file: %1").arg(parseError.errorString()), "Config");
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Load global settings (not user-specific)
    m_autoStartEnabled = root["autoStart"].toBool(false);
    m_echoServerEnabled = root["echoServerEnabled"].toBool(true);
    m_echoServerPort = root["echoServerPort"].toInt(7777);
    m_apiBaseUrl = root["apiBaseUrl"].toString("http://54.225.63.242:8086");
    
    // For cameras, only load from global config if no current user is set
    // Otherwise, cameras will be loaded from user-specific config
    if (m_currentUserEmail.isEmpty()) {
        m_cameras.clear();
        QJsonArray camerasArray = root["cameras"].toArray();
        for (const QJsonValue& value : camerasArray) {
            CameraConfig camera;
            camera.fromJson(value.toObject());
            m_cameras.append(camera);
        }
        LOG_INFO(QString("Loaded global configuration with %1 cameras").arg(m_cameras.size()), "Config");
    } else {
        LOG_INFO("Loaded global configuration (cameras loaded from user-specific config)", "Config");
    }
    
    return true;
}

bool ConfigManager::saveConfig()
{
    QJsonObject root;
    
    // Save global settings only (not user-specific cameras)
    root["autoStart"] = m_autoStartEnabled;
    root["echoServerEnabled"] = m_echoServerEnabled;
    root["echoServerPort"] = m_echoServerPort;
    root["apiBaseUrl"] = m_apiBaseUrl;
    
    // Only save cameras to global config if no current user is set
    if (m_currentUserEmail.isEmpty()) {
        QJsonArray camerasArray;
        for (const CameraConfig& camera : m_cameras) {
            camerasArray.append(camera.toJson());
        }
        root["cameras"] = camerasArray;
    } else {
        // For user-specific sessions, save empty cameras array in global config
        root["cameras"] = QJsonArray();
    }
    
    QJsonDocument doc(root);
    
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Failed to save config file: %1").arg(file.errorString()), "Config");
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    LOG_INFO("Global configuration saved successfully", "Config");
    emit configChanged();
    return true;
}

void ConfigManager::addCamera(const CameraConfig& camera)
{
    CameraConfig newCamera = camera;
    newCamera.setExternalPort(getNextExternalPort());
    m_cameras.append(newCamera);
    
    // Save to user-specific config if we have a current user
    if (!m_currentUserEmail.isEmpty()) {
        saveUserSpecificConfig(m_currentUserEmail);
    } else {
        saveConfig(); // Fallback to global config
    }
    
    LOG_INFO(QString("Added camera: %1 (%2:%3 -> %4) for user: %5")
             .arg(camera.name())
             .arg(camera.ipAddress())
             .arg(camera.port())
             .arg(newCamera.externalPort())
             .arg(m_currentUserEmail.isEmpty() ? "global" : m_currentUserEmail), "Config");
}

void ConfigManager::updateCamera(const QString& id, const CameraConfig& camera)
{
    for (int i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i].id() == id) {
            CameraConfig updatedCamera = camera;
            // Preserve external port
            updatedCamera.setExternalPort(m_cameras[i].externalPort());
            m_cameras[i] = updatedCamera;
            
            // Save to user-specific config if we have a current user
            if (!m_currentUserEmail.isEmpty()) {
                saveUserSpecificConfig(m_currentUserEmail);
            } else {
                saveConfig(); // Fallback to global config
            }
            
            LOG_INFO(QString("Updated camera: %1 for user: %2").arg(camera.name())
                     .arg(m_currentUserEmail.isEmpty() ? "global" : m_currentUserEmail), "Config");
            return;
        }
    }
    
    LOG_WARNING(QString("Camera not found for update: %1").arg(id), "Config");
}

void ConfigManager::removeCamera(const QString& id)
{
    for (int i = 0; i < m_cameras.size(); ++i) {
        if (m_cameras[i].id() == id) {
            QString cameraName = m_cameras[i].name();
            m_cameras.removeAt(i);
            
            // Save to user-specific config if we have a current user
            if (!m_currentUserEmail.isEmpty()) {
                saveUserSpecificConfig(m_currentUserEmail);
            } else {
                saveConfig(); // Fallback to global config
            }
            
            LOG_INFO(QString("Removed camera: %1 for user: %2").arg(cameraName)
                     .arg(m_currentUserEmail.isEmpty() ? "global" : m_currentUserEmail), "Config");
            return;
        }
    }
    
    LOG_WARNING(QString("Camera not found for removal: %1").arg(id), "Config");
}

QList<CameraConfig> ConfigManager::getAllCameras() const
{
    return m_cameras;
}

CameraConfig ConfigManager::getCamera(const QString& id) const
{
    for (const CameraConfig& camera : m_cameras) {
        if (camera.id() == id) {
            return camera;
        }
    }
    return CameraConfig(); // Return empty config if not found
}

void ConfigManager::setAutoStartEnabled(bool enabled)
{
    if (m_autoStartEnabled != enabled) {
        m_autoStartEnabled = enabled;
        updateWindowsAutoStart();
        saveConfig();
        
        LOG_INFO(QString("Auto-start %1").arg(enabled ? "enabled" : "disabled"), "Config");
    }
}

void ConfigManager::setEchoServerEnabled(bool enabled)
{
    if (m_echoServerEnabled != enabled) {
        m_echoServerEnabled = enabled;
        saveConfig();
        
        LOG_INFO(QString("Echo server %1").arg(enabled ? "enabled" : "disabled"), "Config");
        emit configChanged();
    }
}

void ConfigManager::setEchoServerPort(int port)
{
    if (port < 1 || port > 65535) {
        LOG_WARNING(QString("Invalid echo server port: %1").arg(port), "Config");
        return;
    }
    
    if (m_echoServerPort != port) {
        m_echoServerPort = port;
        saveConfig();
        
        LOG_INFO(QString("Echo server port changed to %1").arg(port), "Config");
        emit configChanged();
    }
}

void ConfigManager::setApiBaseUrl(const QString& url)
{
    if (url.isEmpty()) {
        LOG_WARNING("Invalid API base URL: empty string", "Config");
        return;
    }
    
    if (m_apiBaseUrl != url) {
        m_apiBaseUrl = url;
        saveConfig();
        
        LOG_INFO(QString("API base URL changed to %1").arg(url), "Config");
        emit configChanged();
    }
}

int ConfigManager::getNextExternalPort() const
{
    int maxPort = 8550; // Start from 8551
    
    for (const CameraConfig& camera : m_cameras) {
        if (camera.externalPort() > maxPort) {
            maxPort = camera.externalPort();
        }
    }
    
    return maxPort + 1;
}

QString ConfigManager::getConfigFilePath() const
{
    return m_configFilePath;
}

QString ConfigManager::getLogFilePath() const
{
    return m_logFilePath;
}

void ConfigManager::createDefaultConfig()
{
    m_cameras.clear();
    m_autoStartEnabled = false;
    m_echoServerEnabled = true;
    m_echoServerPort = 7777;
    m_apiBaseUrl = "http://54.225.63.242:8086";
    
    LOG_INFO("Created default configuration", "Config");
}

void ConfigManager::updateWindowsAutoStart()
{
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
      if (m_autoStartEnabled) {
        QString appPath = QCoreApplication::applicationFilePath();
        settings.setValue("ViscoConnect", appPath);
        LOG_INFO("Added application to Windows startup", "Config");
    } else {
        settings.remove("ViscoConnect");
        LOG_INFO("Removed application from Windows startup", "Config");
    }
#endif
}

void ConfigManager::switchToUser(const QString& userEmail)
{
    if (m_currentUserEmail == userEmail) {
        return; // Already switched to this user
    }
    
    // Save current user's config before switching (if we have a current user)
    if (!m_currentUserEmail.isEmpty()) {
        saveUserSpecificConfig(m_currentUserEmail);
        LOG_INFO(QString("Saved configuration for previous user: %1").arg(m_currentUserEmail), "Config");
    }
    
    // Clear current cameras
    m_cameras.clear();
    
    // Switch to new user
    m_currentUserEmail = userEmail;
    
    // Load new user's config
    if (!userEmail.isEmpty()) {
        loadUserSpecificConfig(userEmail);
        LOG_INFO(QString("Switched to user: %1 with %2 cameras").arg(userEmail).arg(m_cameras.size()), "Config");
    } else {
        LOG_INFO("Switched to no user (logged out)", "Config");
    }
    
    emit userSwitched(userEmail);
    emit configChanged();
}

void ConfigManager::clearCurrentUserCameras()
{
    m_cameras.clear();
    
    // Save the cleared state for current user
    if (!m_currentUserEmail.isEmpty()) {
        saveUserSpecificConfig(m_currentUserEmail);
        LOG_INFO(QString("Cleared cameras for user: %1").arg(m_currentUserEmail), "Config");
    }
    
    emit configChanged();
}

QString ConfigManager::getUserConfigFilePath(const QString& userEmail) const
{
    if (userEmail.isEmpty()) {
        return m_configFilePath; // Return global config path
    }
    
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString userDirName = QString(userEmail).replace("@", "_at_").replace(".", "_dot_");
    QString userConfigDir = appDataPath + "/users/" + userDirName;
    QDir().mkpath(userConfigDir);
    
    return userConfigDir + "/config.json";
}

void ConfigManager::loadUserSpecificConfig(const QString& userEmail)
{
    QString configPath = getUserConfigFilePath(userEmail);
    
    QFile file(configPath);
    if (!file.exists()) {
        LOG_INFO(QString("User-specific config file does not exist, starting with empty configuration for user: %1").arg(userEmail), "Config");
        m_cameras.clear();
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Failed to open user config file: %1 - %2").arg(configPath).arg(file.errorString()), "Config");
        m_cameras.clear();
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("Failed to parse user config file: %1").arg(parseError.errorString()), "Config");
        m_cameras.clear();
        return;
    }
    
    QJsonObject root = doc.object();
    
    // Load cameras for this user
    m_cameras.clear();
    QJsonArray camerasArray = root["cameras"].toArray();
    for (const QJsonValue& value : camerasArray) {
        CameraConfig camera;
        camera.fromJson(value.toObject());
        m_cameras.append(camera);
    }
    
    LOG_INFO(QString("Loaded user-specific configuration with %1 cameras for user: %2").arg(m_cameras.size()).arg(userEmail), "Config");
}

void ConfigManager::saveUserSpecificConfig(const QString& userEmail)
{
    QString configPath = getUserConfigFilePath(userEmail);
    
    QJsonObject root;
    
    // Save only cameras for user-specific config
    QJsonArray camerasArray;
    for (const CameraConfig& camera : m_cameras) {
        camerasArray.append(camera.toJson());
    }
    root["cameras"] = camerasArray;
    
    QJsonDocument doc(root);
    
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Failed to save user config file: %1 - %2").arg(configPath).arg(file.errorString()), "Config");
        return;
    }
    
    file.write(doc.toJson());
    file.close();
    
    LOG_INFO(QString("User-specific configuration saved successfully for user: %1").arg(userEmail), "Config");
}
