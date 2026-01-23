#include "CameraApiService.h"
#include "AuthDialog.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>

CameraApiService::CameraApiService(WireGuardManager* wireGuardManager, QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_syncTimer(new QTimer(this))
    , m_connectivityTimer(new QTimer(this))
    , m_isOnline(true) // Start as online, will be updated by connectivity check
    , m_isSyncing(false)
    , m_baseUrl(ConfigManager::instance().getApiBaseUrl())
    , m_wireGuardManager(wireGuardManager)
{
    // Setup sync timer for processing queued operations
    m_syncTimer->setSingleShot(false);
    m_syncTimer->setInterval(60000); // Check every 60 seconds (reduced from 30)
    connect(m_syncTimer, &QTimer::timeout, this, &CameraApiService::onSyncTimerTimeout);
    m_syncTimer->start();
    
    // Setup connectivity check timer - less frequent
    m_connectivityTimer->setSingleShot(false);
    m_connectivityTimer->setInterval(120000); // Check every 2 minutes (reduced from 15 seconds)
    connect(m_connectivityTimer, &QTimer::timeout, this, &CameraApiService::checkNetworkConnectivity);
    m_connectivityTimer->start();
    
    // Initial connectivity check after a short delay to let the app initialize
    QTimer::singleShot(5000, this, &CameraApiService::checkNetworkConnectivity);
    
    // Listen for config changes to update base URL
    connect(&ConfigManager::instance(), &ConfigManager::configChanged, this, &CameraApiService::onConfigChanged);
    
    LOG_INFO(QString("Camera API Service initialized with base URL: %1 - Connectivity checks every 2 minutes").arg(m_baseUrl), "CameraApiService");
}

CameraApiService::~CameraApiService()
{
    m_syncTimer->stop();
    m_connectivityTimer->stop();
}

void CameraApiService::createCamera(const CameraConfig& camera)
{
    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        queueOperation(SyncOperation(SyncOperationType::CREATE, camera.id(), camera));
        LOG_INFO(QString("Queued camera creation (no token): %1").arg(camera.name()), "CameraApiService");
        return;
    }
    
    if (!m_isOnline) {
        queueOperation(SyncOperation(SyncOperationType::CREATE, camera.id(), camera));
        LOG_INFO(QString("Queued camera creation (offline): %1").arg(camera.name()), "CameraApiService");
        // Trigger a connectivity check to see if we can go online
        QTimer::singleShot(1000, this, &CameraApiService::checkNetworkConnectivity);
        return;
    }
    
    QNetworkRequest request(QUrl(m_baseUrl + "/cameras/"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    
    QJsonObject cameraJson = cameraToApiJson(camera);
    QJsonDocument doc(cameraJson);
    
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    
    // Track the operation
    m_replyToOperationMap[reply] = "create";
    m_replyCameraIdMap[reply] = camera.id();
    
    connect(reply, &QNetworkReply::finished, this, &CameraApiService::onCreateCameraFinished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &CameraApiService::onNetworkError);
    
    LOG_INFO(QString("Creating camera on server: %1").arg(camera.name()), "CameraApiService");
}

void CameraApiService::updateCamera(const CameraConfig& camera)
{
    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        queueOperation(SyncOperation(SyncOperationType::UPDATE, camera.id(), camera));
        return;
    }
    
    if (!m_isOnline || camera.serverCameraId().isEmpty()) {
        queueOperation(SyncOperation(SyncOperationType::UPDATE, camera.id(), camera));
        return;
    }
    
    QNetworkRequest request(QUrl(QString("%1/cameras/%2").arg(m_baseUrl).arg(camera.serverCameraId())));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    
    QJsonObject cameraJson = cameraToApiJson(camera);
    QJsonDocument doc(cameraJson);
    
    QNetworkReply* reply = m_networkManager->put(request, doc.toJson());
    
    m_replyToOperationMap[reply] = "update";
    m_replyCameraIdMap[reply] = camera.id();
    
    connect(reply, &QNetworkReply::finished, this, &CameraApiService::onUpdateCameraFinished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &CameraApiService::onNetworkError);
    
    LOG_INFO(QString("Updating camera on server: %1 (Server Camera ID: %2)").arg(camera.name()).arg(camera.serverCameraId()), "CameraApiService");
}

void CameraApiService::deleteCamera(const QString& localCameraId, const QString& serverCameraId)
{
    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        queueOperation(SyncOperation(SyncOperationType::DELETE_CAMERA, localCameraId));
        return;
    }
    
    if (!m_isOnline || serverCameraId.isEmpty()) {
        queueOperation(SyncOperation(SyncOperationType::DELETE_CAMERA, localCameraId));
        return;
    }
    
    QNetworkRequest request(QUrl(QString("%1/cameras/%2").arg(m_baseUrl).arg(serverCameraId)));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    
    QNetworkReply* reply = m_networkManager->deleteResource(request);
    
    m_replyToOperationMap[reply] = "delete";
    m_replyCameraIdMap[reply] = localCameraId;
    
    connect(reply, &QNetworkReply::finished, this, &CameraApiService::onDeleteCameraFinished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &CameraApiService::onNetworkError);
    
        LOG_INFO(QString("Deleting camera on server: %1 (Server Camera ID: %2)").arg(localCameraId).arg(serverCameraId), "CameraApiService");
}

void CameraApiService::updateCameraStatus(const QString& localCameraId, const QString& serverCameraId, bool isActive)
{
    QString token = AuthDialog::getCurrentAuthToken();
    QString status = isActive ? "active" : "inactive";
    
    if (token.isEmpty()) {
        queueOperation(SyncOperation(SyncOperationType::STATUS_UPDATE, localCameraId, CameraConfig(), status, serverCameraId));
        LOG_INFO(QString("Queued camera status update (no token): %1 -> %2").arg(localCameraId, status), "CameraApiService");
        return;
    }
    
    if (!m_isOnline || serverCameraId.isEmpty()) {
        queueOperation(SyncOperation(SyncOperationType::STATUS_UPDATE, localCameraId, CameraConfig(), status, serverCameraId));
        LOG_INFO(QString("Queued camera status update (offline/no server ID): %1 -> %2 (Server Camera ID: %3)")
                 .arg(localCameraId, status).arg(serverCameraId), "CameraApiService");
        // Trigger a connectivity check to see if we can go online
        QTimer::singleShot(1000, this, &CameraApiService::checkNetworkConnectivity);
        return;
    }
    
    // For status update via PUT endpoint, we need to send complete camera data
    // We'll defer this to the synchronous version that takes a full camera config
    performCameraStatusUpdate(localCameraId, serverCameraId, status);
}

void CameraApiService::startStream(const CameraConfig& camera)
{
    QString token = AuthDialog::getCurrentAuthToken();
    QJsonObject json;
    
    // Use the stream_name from the camera config (retrieved from server during creation)
    if (!camera.streamName().isEmpty()) {
        json["stream_name"] = camera.streamName();
    } else {
        // Fallback or log warning if stream name is missing
        LOG_WARNING("Stream Name missing in camera config", "CameraApiService");
        json["stream_name"] = QString("%1_%2").arg("unknown").arg(camera.name());
    }
    
    // Construct RTSP URL with external IP and port
    // e.g. rtsp://username:password@[EXTERNAL_IP]:[EXTERNAL_PORT]/path...
    QString externalIp = m_wireGuardManager->getCurrentTunnelIp();
    if (externalIp.isEmpty()) {
        externalIp = "127.0.0.1"; // Fallback, though likely won't work for external server
        LOG_WARNING("WireGuard tunnel IP not found, using localhost fallback", "CameraApiService");
    }
    
    json["rtsp_url"] = constructRtspUrlWithExternalEndpoint(camera, externalIp, camera.externalPort());
    
    QString serverCameraId = camera.serverCameraId();
    if (serverCameraId.isEmpty()) {
        serverCameraId = camera.id(); // Fallback
    }

    if (token.isEmpty()) {
        SyncOperation op(SyncOperationType::START_STREAM, camera.id(), camera, QString(), serverCameraId);
        queueOperation(op);
        LOG_INFO(QString("Queued stream start (no token): Stream Name %1").arg(json["stream_name"].toString()), "CameraApiService");
        return;
    }
    
    if (!m_isOnline) {
        SyncOperation op(SyncOperationType::START_STREAM, camera.id(), camera, QString(), serverCameraId);
        queueOperation(op);
        LOG_INFO(QString("Queued stream start (offline): Stream Name %1").arg(json["stream_name"].toString()), "CameraApiService");
        // Trigger a connectivity check to see if we can go online
        QTimer::singleShot(1000, this, &CameraApiService::checkNetworkConnectivity);
        return;
    }
    
    // Endpoint: /streams/start on Port 8001
    QUrl baseUrl(m_baseUrl);
    baseUrl.setPort(8001);
    
    QNetworkRequest request(QUrl(QString("%1/streams/start").arg(baseUrl.toString())));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    
    QJsonDocument doc(json);
    
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    
    m_replyToOperationMap[reply] = "start_stream";
    m_replyCameraIdMap[reply] = serverCameraId;
    
    connect(reply, &QNetworkReply::finished, this, &CameraApiService::onStartStreamFinished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &CameraApiService::onNetworkError);
    
    LOG_INFO(QString("Starting stream on server: Stream Name: %1, RTSP: %2").arg(json["stream_name"].toString(), json["rtsp_url"].toString()), "CameraApiService");
}

void CameraApiService::stopStream(const QString& streamName)
{
    if (streamName.isEmpty()) {
        LOG_WARNING("Cannot stop stream with empty name", "CameraApiService");
        emit streamStopped("", false, "Empty stream name");
        return;
    }
    
    // Use port 8001 for stream operations
    QUrl baseUrl(m_baseUrl);
    baseUrl.setPort(8001);
    
    QNetworkRequest request(QUrl(QString("%1/streams/stop/%2").arg(baseUrl.toString(), streamName)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString token = AuthDialog::getCurrentAuthToken();
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    }
    
    LOG_INFO(QString("Stopping stream on server: %1 (Port 8001)").arg(streamName), "CameraApiService");
    
    QNetworkReply* reply = m_networkManager->post(request, QByteArray());
    
    // Store streamName in reply map to retrieve it later (though we capture it in lambda too)
    // Actually we can just use the lambda capture for simplicity
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, streamName]() {
        bool success = false;
        QString error;
        
        if (reply->error() == QNetworkReply::NoError) {
            LOG_INFO(QString("Stream stopped successfully on server: %1").arg(streamName), "CameraApiService");
            success = true;
        } else {
            error = reply->errorString();
            LOG_ERROR(QString("Failed to stop stream on server: %1 - %2").arg(streamName, error), "CameraApiService");
            showApiError("stop stream", error);
        }
        
        reply->deleteLater();
        emit streamStopped(streamName, success, error);
    });
}

void CameraApiService::onCreateCameraFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString cameraId = m_replyCameraIdMap.take(reply);
    m_replyToOperationMap.remove(reply);
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = reply->readAll();
    reply->deleteLater();
    
    // Log the raw response for debugging
    LOG_INFO(QString("Camera creation response - Status: %1, Data: %2")
             .arg(statusCode)
             .arg(QString::fromUtf8(data)), "CameraApiService");
    
    if (statusCode == 201 || statusCode == 200) {
        // Parse server response to get the assigned camera_id
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject response = doc.object();
        LOG_INFO(QString("Parsed JSON response: %1").arg(doc.toJson(QJsonDocument::Compact)), "CameraApiService");
        
        QString serverCameraId;
        if (response.contains("camera_id")) {
            // Camera ID can be an integer or string, handle both cases
            QJsonValue cameraIdValue = response["camera_id"];
            if (cameraIdValue.isDouble()) {
                serverCameraId = QString::number(cameraIdValue.toInt());
            } else {
                serverCameraId = cameraIdValue.toString();
            }
            LOG_INFO(QString("Found 'camera_id' field: %1").arg(serverCameraId), "CameraApiService");
        } else if (response.contains("id")) {
            // ID can be an integer or string, handle both cases
            QJsonValue idValue = response["id"];
            if (idValue.isDouble()) {
                serverCameraId = QString::number(idValue.toInt());
            } else {
                serverCameraId = idValue.toString();
            }
            LOG_INFO(QString("Found 'id' field: %1").arg(serverCameraId), "CameraApiService");
        } else {
            LOG_WARNING("No camera_id or id field found in server response", "CameraApiService");
            QStringList keys = response.keys();
            LOG_INFO(QString("Available response fields: %1").arg(keys.join(", ")), "CameraApiService");
        }
        
        if (!serverCameraId.isEmpty()) {
            QString streamName;
            if (response.contains("stream_name")) {
                streamName = response["stream_name"].toString();
            } else {
                // Fallback attempt to construct it if missing from server response, or leave empty
                LOG_WARNING("stream_name missing in create response", "CameraApiService");
            }
            
            LOG_INFO(QString("Camera created successfully on server: %1 (Server Camera ID: %2, Stream Name: %3)")
                     .arg(cameraId).arg(serverCameraId).arg(streamName), "CameraApiService");
                     
            emit cameraCreated(cameraId, serverCameraId, streamName, true, QString());
        } else {
            LOG_WARNING(QString("Camera created on server but no valid camera_id returned: %1").arg(cameraId), "CameraApiService");
            emit cameraCreated(cameraId, QString(), QString(), false, "No valid server camera_id returned");
        }
    } else {
        QString error = QString("Server returned status code: %1, Response: %2").arg(statusCode).arg(QString::fromUtf8(data));
        LOG_ERROR(QString("Failed to create camera on server: %1 - %2").arg(cameraId, error), "CameraApiService");
        emit cameraCreated(cameraId, QString(), QString(), false, error);
        showApiError("create camera", error);
    }
}

void CameraApiService::onUpdateCameraFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString cameraId = m_replyCameraIdMap.take(reply);
    m_replyToOperationMap.remove(reply);
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    
    if (statusCode == 200) {
        LOG_INFO(QString("Camera updated successfully on server: %1").arg(cameraId), "CameraApiService");
        emit cameraUpdated(cameraId, true, QString());
    } else {
        QString error = QString("Server returned status code: %1").arg(statusCode);
        LOG_ERROR(QString("Failed to update camera on server: %1 - %2").arg(cameraId, error), "CameraApiService");
        emit cameraUpdated(cameraId, false, error);
        showApiError("update camera", error);
    }
}

void CameraApiService::onDeleteCameraFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString cameraId = m_replyCameraIdMap.take(reply);
    m_replyToOperationMap.remove(reply);
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    
    if (statusCode == 200 || statusCode == 204) {
        LOG_INFO(QString("Camera deleted successfully on server: %1").arg(cameraId), "CameraApiService");
        emit cameraDeleted(cameraId, true, QString());
    } else {
        // Camera was already removed from local storage, so don't show error
        // Server deletion failure is non-critical since local removal already succeeded
        LOG_DEBUG(QString("Server deletion sync failed for camera %1 (status: %2), but local removal succeeded")
                  .arg(cameraId).arg(statusCode), "CameraApiService");
        emit cameraDeleted(cameraId, true, QString()); // Report as success since local removal worked
    }
}

void CameraApiService::onStatusUpdateFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString cameraId = m_replyCameraIdMap.take(reply);
    m_replyToOperationMap.remove(reply);
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    
    if (statusCode == 200 || statusCode == 204) {
        LOG_INFO(QString("Camera status updated successfully on server: %1").arg(cameraId), "CameraApiService");
        emit cameraStatusUpdated(cameraId, true, QString());
    } else if (statusCode == 404) {
        // Camera already removed on server; treat as successful to avoid noisy errors
        LOG_DEBUG(QString("Camera status update skipped on server (not found): %1").arg(cameraId), "CameraApiService");
        emit cameraStatusUpdated(cameraId, true, QString());
    } else {
        QString error = QString("Server returned status code: %1").arg(statusCode);
        LOG_ERROR(QString("Failed to update camera status on server: %1 - %2").arg(cameraId, error), "CameraApiService");
        emit cameraStatusUpdated(cameraId, false, error);
        showApiError("update camera status", error);
    }
}

void CameraApiService::onStartStreamFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString serverCameraId = m_replyCameraIdMap.take(reply);
    m_replyToOperationMap.remove(reply);
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = reply->readAll();
    reply->deleteLater();
    
    if (statusCode == 200 || statusCode == 201 || statusCode == 204) {
        LOG_INFO(QString("Stream started successfully on server: %1").arg(serverCameraId), "CameraApiService");
        emit streamStarted(serverCameraId, true, QString());
    } else {
        QString error = QString("Server returned status code: %1, Response: %2").arg(statusCode).arg(QString::fromUtf8(data));
        LOG_ERROR(QString("Failed to start stream on server: %1 - %2").arg(serverCameraId, error), "CameraApiService");
        emit streamStarted(serverCameraId, false, error);
        // We don't show a popup error for this as it might be a background thing, or we can?
        // Let's stick to logging for now to not interrupt user if it's secondary.
    }
}

void CameraApiService::onNetworkError(QNetworkReply::NetworkError error)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString cameraId = m_replyCameraIdMap.take(reply);
    QString operation = m_replyToOperationMap.take(reply);
    
    QString errorString = reply->errorString();
    reply->deleteLater();
    
    // If this is a connectivity error, mark as offline and queue the operation
    if (error == QNetworkReply::NetworkSessionFailedError || 
        error == QNetworkReply::HostNotFoundError ||
        error == QNetworkReply::ConnectionRefusedError) {
        
        if (m_isOnline) {
            m_isOnline = false;
            LOG_INFO("Network error detected, switching to offline mode", "CameraApiService");
            emit networkStatusChanged(m_isOnline);
        }
    }
    
    LOG_ERROR(QString("Network error during %1 for camera %2: %3").arg(operation, cameraId, errorString), "CameraApiService");
    
    // Emit appropriate signals based on operation type
    if (operation == "create") {
        emit cameraCreated(cameraId, QString(), QString(), false, errorString);
    } else if (operation == "update") {
        emit cameraUpdated(cameraId, false, errorString);
    } else if (operation == "delete") {
        emit cameraDeleted(cameraId, false, errorString);
    } else if (operation == "status_update" || operation == "status_update_full") {
        emit cameraStatusUpdated(cameraId, false, errorString);
    } else if (operation == "start_stream") {
        emit streamStarted(cameraId, false, errorString);
    }
    
    showApiError(operation, errorString);
}

void CameraApiService::queueOperation(const SyncOperation& operation)
{
    m_syncQueue.enqueue(operation);
    LOG_INFO(QString("Queued sync operation: %1 for camera %2 (Queue size: %3)")
             .arg(static_cast<int>(operation.type))
             .arg(operation.localCameraId)
             .arg(m_syncQueue.size()), "CameraApiService");
}

void CameraApiService::onSyncTimerTimeout()
{
    if (m_isOnline && !m_syncQueue.isEmpty() && !m_isSyncing) {
        processSyncQueue();
    }
}

void CameraApiService::processSyncQueue()
{
    if (m_syncQueue.isEmpty() || m_isSyncing) {
        return;
    }
    
    // Check if we have a valid token and are online
    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty() || !m_isOnline) {
        return;
    }
    
    m_isSyncing = true;
    LOG_INFO(QString("Processing sync queue with %1 operations").arg(m_syncQueue.size()), "CameraApiService");
    
    processNextSyncOperation();
}

void CameraApiService::processNextSyncOperation()
{
    if (m_syncQueue.isEmpty()) {
        m_isSyncing = false;
        emit syncCompleted();
        LOG_INFO("Sync queue processing completed", "CameraApiService");
        return;
    }
    
    SyncOperation operation = m_syncQueue.dequeue();
    
    LOG_INFO(QString("Processing sync operation: Type=%1, Camera=%2")
             .arg(static_cast<int>(operation.type))
             .arg(operation.localCameraId), "CameraApiService");
    
    // Process the operation based on type
    switch (operation.type) {
        case SyncOperationType::CREATE:
            LOG_INFO(QString("Syncing camera creation: %1").arg(operation.camera.name()), "CameraApiService");
            createCamera(operation.camera);
            break;
        case SyncOperationType::UPDATE:
            LOG_INFO(QString("Syncing camera update: %1").arg(operation.camera.name()), "CameraApiService");
            updateCamera(operation.camera);
            break;
        case SyncOperationType::DELETE_CAMERA:
            LOG_INFO(QString("Syncing camera deletion: %1").arg(operation.localCameraId), "CameraApiService");
            // For delete operations, we need the server ID from the stored camera
            if (!operation.serverCameraId.isEmpty()) {
                deleteCamera(operation.localCameraId, operation.serverCameraId);
            } else {
                LOG_WARNING(QString("Cannot delete camera - no server ID: %1").arg(operation.localCameraId), "CameraApiService");
            }
            break;
        case SyncOperationType::STATUS_UPDATE:
            LOG_INFO(QString("Syncing camera status: %1 -> %2 (Server ID: %3)")
                     .arg(operation.localCameraId, operation.status).arg(operation.serverCameraId), "CameraApiService");
            if (!operation.serverCameraId.isEmpty()) {
                // Check if we have a full camera config for this operation
                if (!operation.camera.id().isEmpty()) {
                    // We have full camera data, use the full data update method
                    bool isActive = (operation.status == "active");
                    performCameraStatusUpdateWithFullData(operation.camera, isActive);
                } else {
                    // Fallback to status-only update
                    performCameraStatusUpdate(operation.localCameraId, operation.serverCameraId, operation.status);
                }
            } else {
                LOG_WARNING(QString("Cannot update camera status - no server ID: %1").arg(operation.localCameraId), "CameraApiService");
            }
            break;
        case SyncOperationType::START_STREAM:
            LOG_INFO(QString("Syncing start stream: Stream Name %1").arg(operation.serverCameraId), "CameraApiService");
            if (!operation.camera.id().isEmpty()) {
                startStream(operation.camera);
            } else {
                LOG_WARNING("Cannot sync start stream - missing camera config", "CameraApiService");
            }
            break;
    }
    
    // Continue processing after a short delay to avoid overwhelming the server
    QTimer::singleShot(2000, this, &CameraApiService::processNextSyncOperation);
}

void CameraApiService::checkNetworkConnectivity()
{
    // Skip connectivity check if we don't have pending operations and we're considered online
    if (m_syncQueue.isEmpty() && m_isOnline) {
        LOG_DEBUG("Skipping connectivity check - no pending operations and currently online", "CameraApiService");
        return;
    }
    
    // Check connectivity using a lightweight request
    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        // If no token, assume online but can't sync
        bool wasOnline = m_isOnline;
        m_isOnline = true; // Consider online but unable to authenticate
        
        if (wasOnline != m_isOnline) {
            LOG_INFO("Network status: Online but no authentication token", "CameraApiService");
            emit networkStatusChanged(m_isOnline);
        }
        return;
    }
    
    // Only do a full connectivity check if we have queued operations or think we're offline
    if (!m_syncQueue.isEmpty() || !m_isOnline) {
        LOG_DEBUG(QString("Performing connectivity check - Queue: %1 items, Current status: %2")
                  .arg(m_syncQueue.size())
                  .arg(m_isOnline ? "Online" : "Offline"), "CameraApiService");
        
        QNetworkRequest request(QUrl(m_baseUrl + "/me/profile"));
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
        request.setRawHeader("User-Agent", "CameraServer/1.0");
        
        QNetworkReply* reply = m_networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, [this, reply]() {
            bool wasOnline = m_isOnline;
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            
            // Consider online if we get any HTTP response (200, 401, etc.)
            // Only consider offline for network errors (no response at all)
            m_isOnline = (reply->error() == QNetworkReply::NoError || 
                         reply->error() == QNetworkReply::AuthenticationRequiredError ||
                         statusCode > 0); // Any HTTP status code means server is reachable
            
            reply->deleteLater();
            
            if (wasOnline != m_isOnline) {
                LOG_INFO(QString("Network status changed: %1 (HTTP Status: %2)")
                         .arg(m_isOnline ? "Online" : "Offline")
                         .arg(statusCode), "CameraApiService");
                emit networkStatusChanged(m_isOnline);
                
                // If we just came online, try to process sync queue
                if (m_isOnline && !m_syncQueue.isEmpty()) {
                    LOG_INFO(QString("Coming online, scheduling sync queue processing (%1 items)")
                             .arg(m_syncQueue.size()), "CameraApiService");
                    QTimer::singleShot(3000, this, &CameraApiService::processSyncQueue);
                }
            }
        });
    }
}

QJsonObject CameraApiService::cameraToApiJson(const CameraConfig& camera) const
{
    QJsonObject json;
    json["name"] = camera.name();
    
    // Send local camera details for reference
    json["camera_ip"] = camera.ipAddress();
    json["camera_port"] = camera.port();
    
    // Send WireGuard IP and external port for KVS streaming
    json["wireguard_ip"] = getWireGuardIP();
    json["external_port"] = camera.externalPort();
    
    // Backward compatibility fields
    json["c_ip"] = camera.ipAddress();  // Keep for old API compatibility
    json["port"] = camera.externalPort();  // External port for KVS streaming
    
    json["status"] = getStatusString(camera.isEnabled());
    json["stream_url"] = constructRtspUrl(camera);
    json["username"] = camera.username();
    json["password"] = camera.password();
    
    return json;
}

QString CameraApiService::getWireGuardIP() const
{
    // TODO: Implement WireGuard IP retrieval properly
    // For now, return a placeholder
    return "10.0.0.2"; // Placeholder WireGuard IP
}

QString CameraApiService::getStatusString(bool isEnabled) const
{
    return isEnabled ? "active" : "inactive";
}

QString CameraApiService::constructRtspUrl(const CameraConfig& camera)
{
    if (camera.ipAddress().isEmpty()) {
        return QString();
    }
    
    QString brand = camera.brand().toLower();
    QString rtspPath;
    
    // Brand-specific RTSP paths
    if (brand.contains("hikvision")) {
        rtspPath = "/Streaming/Channels/101";
    } else if (brand.contains("dahua")) {
        rtspPath = "/cam/realmonitor?channel=1&subtype=0";
    } else if (brand.contains("cp plus") || brand.contains("cpplus")) {
        rtspPath = "/cam/realmonitor?channel=1&subtype=0";
    } else if (brand.contains("axis")) {
        rtspPath = "/axis-media/media.amp";
    } else if (brand.contains("onvif")) {
        rtspPath = "/onvif1";
    } else {
        // Generic RTSP path
        rtspPath = "/stream1";
    }
    
    QString rtspUrl;
    if (!camera.username().isEmpty() && !camera.password().isEmpty()) {
        rtspUrl = QString("rtsp://%1:%2@%3:%4%5")
                  .arg(camera.username())
                  .arg(camera.password())
                  .arg(camera.ipAddress())
                  .arg(camera.port())
                  .arg(rtspPath);
    } else if (!camera.username().isEmpty()) {
        rtspUrl = QString("rtsp://%1@%2:%3%4")
                  .arg(camera.username())
                  .arg(camera.ipAddress())
                  .arg(camera.port())
                  .arg(rtspPath);
    } else {
        rtspUrl = QString("rtsp://%1:%2%3")
                  .arg(camera.ipAddress())
                  .arg(camera.port())
                  .arg(rtspPath);
    }
    
    return rtspUrl;
}

QString CameraApiService::constructRtspUrlWithExternalEndpoint(const CameraConfig& camera, const QString& ip, int port)
{
    if (ip.isEmpty()) {
        return QString();
    }
    
    QString brand = camera.brand().toLower();
    QString rtspPath;
    
    // Brand-specific RTSP paths
    if (brand.contains("hikvision")) {
        rtspPath = "/Streaming/Channels/101";
    } else if (brand.contains("dahua")) {
        rtspPath = "/cam/realmonitor?channel=1&subtype=0";
    } else if (brand.contains("cp plus") || brand.contains("cpplus")) {
        rtspPath = "/cam/realmonitor?channel=1&subtype=0";
    } else if (brand.contains("axis")) {
        rtspPath = "/axis-media/media.amp";
    } else if (brand.contains("onvif")) {
        rtspPath = "/onvif1";
    } else {
        // Generic RTSP path
        rtspPath = "/stream1";
    }
    
    QString rtspUrl;
    if (!camera.username().isEmpty() && !camera.password().isEmpty()) {
        rtspUrl = QString("rtsp://%1:%2@%3:%4%5")
                  .arg(camera.username())
                  .arg(camera.password())
                  .arg(ip)
                  .arg(port)
                  .arg(rtspPath);
    } else if (!camera.username().isEmpty()) {
        rtspUrl = QString("rtsp://%1@%2:%3%4")
                  .arg(camera.username())
                  .arg(ip)
                  .arg(port)
                  .arg(rtspPath);
    } else {
        rtspUrl = QString("rtsp://%1:%2%3")
                  .arg(ip)
                  .arg(port)
                  .arg(rtspPath);
    }
    
    return rtspUrl;
}


void CameraApiService::showApiError(const QString& operation, const QString& error)
{
    QMessageBox::warning(nullptr, 
                        "Visco Connect - API Error",
                        QString("Failed to %1:\n\n%2\n\nThe operation has been queued for retry when connection is restored.")
                        .arg(operation, error));
}

void CameraApiService::performCameraStatusUpdate(const QString& localCameraId, const QString& serverCameraId, const QString& status)
{
    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        LOG_ERROR("Cannot perform status update - no authentication token", "CameraApiService");
        emit cameraStatusUpdated(localCameraId, false, "No authentication token");
        return;
    }
    
    QNetworkRequest request(QUrl(QString("%1/cameras/%2").arg(m_baseUrl).arg(serverCameraId)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    
    QJsonObject statusJson;
    statusJson["status"] = status;
    QJsonDocument doc(statusJson);
    
    QNetworkReply* reply = m_networkManager->put(request, doc.toJson());
    
    m_replyToOperationMap[reply] = "status_update";
    m_replyCameraIdMap[reply] = localCameraId;
    
    connect(reply, &QNetworkReply::finished, this, &CameraApiService::onStatusUpdateFinished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &CameraApiService::onNetworkError);
    
    LOG_INFO(QString("Performing camera status update on server: %1 -> %2 (Server Camera ID: %3)")
             .arg(localCameraId, status).arg(serverCameraId), "CameraApiService");
}

void CameraApiService::updateCameraStatusWithFullData(const CameraConfig& camera, bool isActive)
{
    QString token = AuthDialog::getCurrentAuthToken();
    
    if (token.isEmpty()) {
        CameraConfig cameraCopy = camera;
        cameraCopy.setEnabled(isActive);
        queueOperation(SyncOperation(SyncOperationType::UPDATE, camera.id(), cameraCopy));
        LOG_INFO(QString("Queued camera full data status update (no token): %1").arg(camera.name()), "CameraApiService");
        return;
    }
    
    if (!m_isOnline || camera.serverCameraId().isEmpty()) {
        CameraConfig cameraCopy = camera;
        cameraCopy.setEnabled(isActive);
        queueOperation(SyncOperation(SyncOperationType::UPDATE, camera.id(), cameraCopy));
        LOG_INFO(QString("Queued camera full data status update (offline): %1").arg(camera.name()), "CameraApiService");
        QTimer::singleShot(1000, this, &CameraApiService::checkNetworkConnectivity);
        return;
    }
    
    performCameraStatusUpdateWithFullData(camera, isActive);
}

void CameraApiService::performCameraStatusUpdateWithFullData(const CameraConfig& camera, bool isActive)
{
    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        LOG_ERROR("Cannot perform full data status update - no authentication token", "CameraApiService");
        emit cameraStatusUpdated(camera.id(), false, "No authentication token");
        return;
    }
    
    QNetworkRequest request(QUrl(QString("%1/cameras/%2").arg(m_baseUrl).arg(camera.serverCameraId())));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    
    // Create a copy of the camera with the updated status
    CameraConfig updatedCamera = camera;
    updatedCamera.setEnabled(isActive);
    
    // Send complete camera data
    QJsonObject cameraJson = cameraToApiJson(updatedCamera);
    QJsonDocument doc(cameraJson);
    
    QNetworkReply* reply = m_networkManager->put(request, doc.toJson());
    
    m_replyToOperationMap[reply] = "status_update_full";
    m_replyCameraIdMap[reply] = camera.id();
    
    connect(reply, &QNetworkReply::finished, this, &CameraApiService::onStatusUpdateFinished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &CameraApiService::onNetworkError);
    
    LOG_INFO(QString("Performing camera full data status update on server: %1 -> %2 (Server Camera ID: %3)")
             .arg(camera.name()).arg(isActive ? "active" : "inactive").arg(camera.serverCameraId()), "CameraApiService");
}

void CameraApiService::onConfigChanged()
{
    QString newBaseUrl = ConfigManager::instance().getApiBaseUrl();
    if (m_baseUrl != newBaseUrl) {
        LOG_INFO(QString("API base URL updated from %1 to %2").arg(m_baseUrl, newBaseUrl), "CameraApiService");
        m_baseUrl = newBaseUrl;
        
        // Trigger a connectivity check with the new URL
        QTimer::singleShot(1000, this, &CameraApiService::checkNetworkConnectivity);
    }
}
