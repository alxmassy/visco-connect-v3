#include "CameraManager.h"
#include "CameraApiService.h"
#include "ConfigManager.h"
#include "Logger.h"

CameraManager::CameraManager(WireGuardManager* wireGuardManager, QObject *parent)
    : QObject(parent)
    , m_portForwarder(nullptr)
    , m_apiService(nullptr)
{
    m_portForwarder = new PortForwarder(this);
    m_apiService = new CameraApiService(wireGuardManager, this);
    
    // Connect port forwarder signals
    connect(m_portForwarder, &PortForwarder::forwardingStarted,
            this, &CameraManager::handleForwardingStarted);
    connect(m_portForwarder, &PortForwarder::forwardingStopped,
            this, &CameraManager::handleForwardingStopped);
    connect(m_portForwarder, &PortForwarder::forwardingError,
            this, &CameraManager::handleForwardingError);
    connect(m_portForwarder, &PortForwarder::connectionEstablished,
            this, &CameraManager::handleConnectionEstablished);
    connect(m_portForwarder, &PortForwarder::connectionClosed,
            this, &CameraManager::handleConnectionClosed);
    
    // Connect API service signals
    connect(m_apiService, &CameraApiService::cameraCreated,
            this, &CameraManager::handleCameraCreated);
    connect(m_apiService, &CameraApiService::cameraUpdated,
            this, &CameraManager::handleCameraUpdated);
    connect(m_apiService, &CameraApiService::cameraDeleted,
            this, &CameraManager::handleCameraDeleted);
    connect(m_apiService, &CameraApiService::cameraStatusUpdated,
            this, &CameraManager::handleCameraStatusUpdated);
    
    // Connect to ConfigManager for user switching
    connect(&ConfigManager::instance(), &ConfigManager::userSwitched,
            this, &CameraManager::handleUserSwitched);
}

CameraManager::~CameraManager()
{
    shutdown();
}

void CameraManager::initialize()
{
    loadConfiguration();
    
    // Auto-start enabled cameras
    for (const CameraConfig& camera : m_cameras.values()) {
        if (camera.isEnabled()) {
            startCamera(camera.id());
        }
    }
    
    LOG_INFO("Camera manager initialized", "CameraManager");
}

void CameraManager::shutdown()
{
    stopAllCameras();
    LOG_INFO("Camera manager shutdown", "CameraManager");
}

bool CameraManager::addCamera(const CameraConfig& camera)
{
    if (!camera.isValid()) {
        LOG_ERROR(QString("Cannot add invalid camera: %1").arg(camera.name()), "CameraManager");
        return false;
    }
    
    // Add to local storage first (local storage is source of truth)
    ConfigManager::instance().addCamera(camera);
    loadConfiguration();
    
    // Then sync to server (don't auto-start yet - wait for server confirmation)
    m_apiService->createCamera(camera);
    
    // Note: Camera will be started in handleCameraCreated() if server accepts it
    LOG_INFO(QString("Camera added locally, waiting for server confirmation: %1").arg(camera.name()), "CameraManager");
    
    emit configurationChanged();
    return true;
}

bool CameraManager::updateCamera(const QString& id, const CameraConfig& camera)
{
    if (!camera.isValid()) {
        LOG_ERROR(QString("Cannot update to invalid camera configuration: %1").arg(camera.name()), "CameraManager");
        return false;
    }
    
    // Stop camera if it's running
    bool wasRunning = isCameraRunning(id);
    if (wasRunning) {
        stopCamera(id);
    }
    
    // Update local storage first
    ConfigManager::instance().updateCamera(id, camera);
    loadConfiguration();
    
    // Sync to server
    m_apiService->updateCamera(camera);
    
    // Restart camera if it was running and still enabled
    if (wasRunning && camera.isEnabled()) {
        startCamera(id);
    }
    
    LOG_INFO(QString("Camera updated locally: %1").arg(camera.name()), "CameraManager");
    emit configurationChanged();
    return true;
}

bool CameraManager::removeCamera(const QString& id)
{
    if (!m_cameras.contains(id)) {
        LOG_WARNING(QString("Cannot remove non-existent camera: %1").arg(id), "CameraManager");
        return false;
    }
    
    stopCamera(id);
    
    CameraConfig camera = m_cameras[id];
    QString cameraName = camera.name();
    int serverId = camera.serverId();
    
    // Remove from local storage first
    ConfigManager::instance().removeCamera(id);
    loadConfiguration();
    
    // Sync deletion to server
    m_apiService->deleteCamera(id, camera.serverCameraId());
    
    LOG_INFO(QString("Camera removed locally: %1").arg(cameraName), "CameraManager");
    emit configurationChanged();
    return true;
}

void CameraManager::startCamera(const QString& id)
{
    if (!m_cameras.contains(id)) {
        LOG_ERROR(QString("Cannot start non-existent camera: %1").arg(id), "CameraManager");
        return;
    }
    
    const CameraConfig& camera = m_cameras[id];
    if (!camera.isEnabled()) {
        LOG_WARNING(QString("Cannot start disabled camera: %1").arg(camera.name()), "CameraManager");
        return;
    }
    
    if (isCameraRunning(id)) {
        LOG_WARNING(QString("Camera already running: %1").arg(camera.name()), "CameraManager");
        return;
    }
    
    if (m_portForwarder->startForwarding(camera)) {
        m_cameraStatus[id] = true;
        LOG_INFO(QString("Camera started: %1").arg(camera.name()), "CameraManager");
        emit cameraStarted(id);
    } else {
        LOG_ERROR(QString("Failed to start camera: %1").arg(camera.name()), "CameraManager");
        emit cameraError(id, "Failed to start port forwarding");
    }
}

void CameraManager::stopCamera(const QString& id)
{
    if (!m_cameras.contains(id)) {
        LOG_WARNING(QString("Cannot stop non-existent camera: %1").arg(id), "CameraManager");
        return;
    }
    
    if (!isCameraRunning(id)) {
        return; // Already stopped
    }
    
    m_portForwarder->stopForwarding(id);
    m_cameraStatus[id] = false;
    
    LOG_INFO(QString("Camera stopped: %1").arg(m_cameras[id].name()), "CameraManager");
    emit cameraStopped(id);
}

void CameraManager::startAllCameras()
{
    for (const CameraConfig& camera : m_cameras.values()) {
        if (camera.isEnabled()) {
            startCamera(camera.id());
        }
    }
    
    LOG_INFO("Started all enabled cameras", "CameraManager");
}

void CameraManager::stopAllCameras()
{
    for (const QString& id : m_cameras.keys()) {
        stopCamera(id);
    }
    
    LOG_INFO("Stopped all cameras", "CameraManager");
}

bool CameraManager::isCameraRunning(const QString& id) const
{
    return m_cameraStatus.value(id, false);
}

QStringList CameraManager::getRunningCameras() const
{
    QStringList running;
    for (auto it = m_cameraStatus.begin(); it != m_cameraStatus.end(); ++it) {
        if (it.value()) {
            running.append(it.key());
        }
    }
    return running;
}

QList<CameraConfig> CameraManager::getAllCameras() const
{
    return m_cameras.values();
}

void CameraManager::handleForwardingStarted(const QString& cameraId, int externalPort)
{
    m_cameraStatus[cameraId] = true;
    
    // Sync status change to server using full camera data
    if (m_cameras.contains(cameraId)) {
        const CameraConfig& camera = m_cameras[cameraId];
        LOG_INFO(QString("Starting camera - Camera: %1, ServerCameraID: '%2'")
                 .arg(camera.name()).arg(camera.serverCameraId()), "CameraManager");
        // Use the new method that sends full camera data
        m_apiService->updateCameraStatusWithFullData(camera, true);
    } else {
        LOG_WARNING(QString("Camera not found in local map for status update: %1").arg(cameraId), "CameraManager");
    }
    
    emit cameraStarted(cameraId);
}

void CameraManager::handleForwardingStopped(const QString& cameraId)
{
    m_cameraStatus[cameraId] = false;
    
    // Sync status change to server using full camera data
    if (m_cameras.contains(cameraId)) {
        const CameraConfig& camera = m_cameras[cameraId];
        LOG_INFO(QString("Stopping camera - Camera: %1, ServerCameraID: '%2'")
                 .arg(camera.name()).arg(camera.serverCameraId()), "CameraManager");
        // Use the new method that sends full camera data
        m_apiService->updateCameraStatusWithFullData(camera, false);
    } else {
        LOG_WARNING(QString("Camera not found in local map for status update: %1").arg(cameraId), "CameraManager");
    }
    
    emit cameraStopped(cameraId);
}

void CameraManager::handleForwardingError(const QString& cameraId, const QString& error)
{
    m_cameraStatus[cameraId] = false;
    emit cameraError(cameraId, error);
}

void CameraManager::handleConnectionEstablished(const QString& cameraId, const QString& clientAddress)
{
    if (m_cameras.contains(cameraId)) {
        LOG_DEBUG(QString("Connection established to camera %1 from %2")
                  .arg(m_cameras[cameraId].name())
                  .arg(clientAddress), "CameraManager");
    }
}

void CameraManager::handleConnectionClosed(const QString& cameraId, const QString& clientAddress)
{
    if (m_cameras.contains(cameraId)) {
        LOG_DEBUG(QString("Connection closed to camera %1 from %2")
                  .arg(m_cameras[cameraId].name())
                  .arg(clientAddress), "CameraManager");
    }
}

void CameraManager::loadConfiguration()
{
    m_cameras.clear();
    m_cameraStatus.clear();
    
    QList<CameraConfig> cameras = ConfigManager::instance().getAllCameras();
    for (const CameraConfig& camera : cameras) {
        m_cameras[camera.id()] = camera;
        m_cameraStatus[camera.id()] = false;
    }
}

void CameraManager::saveConfiguration()
{
    // Configuration is automatically saved by ConfigManager
}

void CameraManager::handleCameraCreated(const QString& localCameraId, const QString& serverCameraId, bool success, const QString& error)
{
    LOG_INFO(QString("handleCameraCreated called - LocalID: %1, ServerCameraID: %2, Success: %3, Error: %4")
             .arg(localCameraId).arg(serverCameraId).arg(success ? "true" : "false").arg(error), "CameraManager");
             
    if (success && !localCameraId.isEmpty() && !serverCameraId.isEmpty()) {
        // Update the local camera with the server's camera ID
        if (m_cameras.contains(localCameraId)) {
            CameraConfig camera = m_cameras[localCameraId];
            LOG_INFO(QString("Before update - Camera: %1, ServerCameraID: %2")
                     .arg(camera.name()).arg(camera.serverCameraId()), "CameraManager");
                     
            camera.setServerCameraId(serverCameraId);
            
            LOG_INFO(QString("After setServerCameraId - Camera: %1, ServerCameraID: %2")
                     .arg(camera.name()).arg(camera.serverCameraId()), "CameraManager");
            
            // Update the camera in local storage
            m_cameras[localCameraId] = camera;
            ConfigManager::instance().updateCamera(localCameraId, camera);
            
            LOG_INFO(QString("Camera synchronized with server: %1 (Server Camera ID: %2)")
                     .arg(camera.name()).arg(serverCameraId), "CameraManager");
            
            // Now that server accepted it, auto-start if enabled
            if (camera.isEnabled()) {
                startCamera(localCameraId);
                LOG_INFO(QString("Camera started after server confirmation: %1").arg(camera.name()), "CameraManager");
            }
        } else {
            LOG_WARNING(QString("Camera not found in local map: %1").arg(localCameraId), "CameraManager");
        }
    } else {
        // Server rejected the camera - remove from local storage
        LOG_ERROR(QString("Server rejected camera creation: %1 - %2").arg(localCameraId, error), "CameraManager");
        
        if (m_cameras.contains(localCameraId)) {
            QString cameraName = m_cameras[localCameraId].name();
            
            // Remove from local storage
            ConfigManager::instance().removeCamera(localCameraId);
            loadConfiguration();
            
            LOG_INFO(QString("Removed rejected camera from local storage: %1").arg(cameraName), "CameraManager");
            emit configurationChanged();
            
            // Emit error so UI can show appropriate message
            emit cameraError(localCameraId, QString("Failed to add camera to server: %1").arg(error));
        }
    }
}

void CameraManager::handleCameraUpdated(const QString& localCameraId, bool success, const QString& error)
{
    if (success) {
        LOG_INFO(QString("Camera update synchronized with server: %1").arg(localCameraId), "CameraManager");
    } else {
        LOG_WARNING(QString("Failed to update camera on server: %1 - %2")
                   .arg(localCameraId, error), "CameraManager");
    }
}

void CameraManager::handleCameraDeleted(const QString& localCameraId, bool success, const QString& error)
{
    if (success) {
        LOG_INFO(QString("Camera deletion synchronized with server: %1").arg(localCameraId), "CameraManager");
    } else {
        LOG_WARNING(QString("Failed to delete camera on server: %1 - %2")
                   .arg(localCameraId, error), "CameraManager");
    }
}

void CameraManager::handleCameraStatusUpdated(const QString& localCameraId, bool success, const QString& error)
{
    if (success) {
        LOG_INFO(QString("Camera status synchronized with server: %1").arg(localCameraId), "CameraManager");
    } else {
        LOG_WARNING(QString("Failed to update camera status on server: %1 - %2")
                   .arg(localCameraId, error), "CameraManager");
    }
}

void CameraManager::handleUserSwitched(const QString& userEmail)
{
    LOG_INFO(QString("User switched to: %1, reloading camera configuration").arg(userEmail.isEmpty() ? "logout" : userEmail), "CameraManager");
    
    // Stop all currently running cameras
    stopAllCameras();
    
    // Reload configuration for the new user
    loadConfiguration();
    
    // Notify that configuration has changed
    emit configurationChanged();
    
    // Auto-start enabled cameras for the new user
    if (!userEmail.isEmpty()) {
        for (const CameraConfig& camera : m_cameras.values()) {
            if (camera.isEnabled()) {
                startCamera(camera.id());
            }
        }
    }
}
