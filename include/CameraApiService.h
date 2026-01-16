#ifndef CAMERAAPISERVICE_H
#define CAMERAAPISERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QTimer>
#include <QJsonObject>
#include <QDateTime>
#include <QString>
#include <QHash>
#include "CameraConfig.h"
#include "WireGuardManager.h"

// Enum for sync operation types
enum class SyncOperationType {
    CREATE,
    UPDATE,
    DELETE_CAMERA,  // Renamed to avoid Windows macro conflict
    STATUS_UPDATE
};

// Structure for queued sync operations
struct SyncOperation {
    SyncOperationType type;
    QString localCameraId;
    CameraConfig camera;
    QString status; // For status updates
    QString serverCameraId; // Server-assigned camera ID
    int timestamp;
    
    SyncOperation() : timestamp(QDateTime::currentSecsSinceEpoch()) {}
    SyncOperation(SyncOperationType t, const QString& id, const CameraConfig& cam = CameraConfig(), const QString& st = QString(), const QString& sId = QString())
        : type(t), localCameraId(id), camera(cam), status(st), serverCameraId(sId), timestamp(QDateTime::currentSecsSinceEpoch()) {}
};

class CameraApiService : public QObject
{
    Q_OBJECT

public:
    explicit CameraApiService(WireGuardManager* wireGuardManager, QObject *parent = nullptr);
    ~CameraApiService();

    // Main API operations
    void createCamera(const CameraConfig& camera);
    void updateCamera(const CameraConfig& camera);
    void deleteCamera(const QString& localCameraId, const QString& serverCameraId);
    void updateCameraStatus(const QString& localCameraId, const QString& serverCameraId, bool isActive);
    void updateCameraStatusWithFullData(const CameraConfig& camera, bool isActive);

    // Sync management
    void processSyncQueue();
    bool isOnline() const { return m_isOnline; }
    int pendingSyncCount() const { return m_syncQueue.size(); }

    // Utility methods
    static QString constructRtspUrl(const CameraConfig& camera);

signals:
    void cameraCreated(const QString& localCameraId, const QString& serverCameraId, bool success, const QString& error);
    void cameraUpdated(const QString& localCameraId, bool success, const QString& error);
    void cameraDeleted(const QString& localCameraId, bool success, const QString& error);
    void cameraStatusUpdated(const QString& localCameraId, bool success, const QString& error);
    void syncCompleted();
    void syncProgress(int completed, int total);
    void networkStatusChanged(bool isOnline);

private slots:
    void onCreateCameraFinished();
    void onUpdateCameraFinished();
    void onDeleteCameraFinished();
    void onStatusUpdateFinished();
    void onNetworkError(QNetworkReply::NetworkError error);
    void onSyncTimerTimeout();
    void checkNetworkConnectivity();
    void onConfigChanged();

private:
    void queueOperation(const SyncOperation& operation);
    void processNextSyncOperation();
    void handleApiResponse(QNetworkReply* reply, const QString& operation, const QString& cameraId);
    void showApiError(const QString& operation, const QString& error);
    QJsonObject cameraToApiJson(const CameraConfig& camera) const;
    QString getStatusString(bool isEnabled) const;
    QString getWireGuardIP() const;
    void performCameraStatusUpdate(const QString& localCameraId, const QString& serverCameraId, const QString& status);
    void performCameraStatusUpdateWithFullData(const CameraConfig& camera, bool isActive);
    
    QNetworkAccessManager* m_networkManager;
    QQueue<SyncOperation> m_syncQueue;
    QTimer* m_syncTimer;
    QTimer* m_connectivityTimer;
    bool m_isOnline;
    bool m_isSyncing;
    QString m_baseUrl;
    WireGuardManager* m_wireGuardManager;
    
    // Track ongoing operations to associate responses
    QHash<QNetworkReply*, QString> m_replyToOperationMap;
    QHash<QNetworkReply*, QString> m_replyCameraIdMap;
};

#endif // CAMERAAPISERVICE_H
