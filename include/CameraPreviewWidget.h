#ifndef CAMERAPREVIEWWIDGET_H
#define CAMERAPREVIEWWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QProgressBar>
#include <QTimer>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QAudioOutput>
#include <QMessageBox>
#include <QFileDialog>
#include <QPixmap>
#include <QImage>
#include <QDateTime>
#include "CameraConfig.h"

QT_BEGIN_NAMESPACE
class QGroupBox;
class QSpacerItem;
QT_END_NAMESPACE

class CameraPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraPreviewWidget(QWidget *parent = nullptr);
    explicit CameraPreviewWidget(const CameraConfig& camera, QWidget *parent = nullptr);
    ~CameraPreviewWidget();

    // Camera management
    void setCamera(const CameraConfig& camera);
    void clearCamera();
    CameraConfig getCamera() const { return m_camera; }
    bool hasCamera() const { return !m_camera.id().isEmpty(); }

    // Playback control
    void play();
    void pause();
    void stop();
    void togglePlayback();

    // Stream status
    bool isPlaying() const;
    bool isConnected() const { return m_isConnected; }
    QString getLastError() const { return m_lastError; }

    // UI management
    void setShowControls(bool show);
    void setCompactMode(bool compact);

signals:
    void playbackStarted();
    void playbackStopped();
    void connectionEstablished();
    void connectionLost();
    void errorOccurred(const QString& error);
    void snapshotTaken(const QString& filePath);

public slots:
    void reconnect();
    void takeSnapshot();
    void toggleFullscreen();
    void setVolume(int volume);

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void updateConnectionStatus();
    void retryConnection();
    void onVideoFrameChanged(const QVideoFrame& frame);

private:
    void setupUI();
    void setupMediaPlayer();
    void updateControls();
    void updateStatusDisplay();
    void buildRtspUrl();
    void resetConnection();
    void showError(const QString& error);

    // Camera configuration
    CameraConfig m_camera;
    QString m_rtspUrl;

    // Media player components
    QMediaPlayer* m_mediaPlayer;
    QVideoSink* m_videoSink;
    QAudioOutput* m_audioOutput;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QGroupBox* m_videoGroupBox;
    QGroupBox* m_controlsGroupBox;
    
    // Video display
    QVBoxLayout* m_videoLayout;
    QLabel* m_statusLabel;
    QLabel* m_cameraInfoLabel;
    QLabel* m_videoLabel;
    
    // Control panel
    QHBoxLayout* m_controlsLayout;
    QPushButton* m_playButton;
    QPushButton* m_stopButton;
    QPushButton* m_reconnectButton;
    QPushButton* m_snapshotButton;
    QPushButton* m_fullscreenButton;
    
    // Volume and connection info
    QSlider* m_volumeSlider;
    QLabel* m_volumeLabel;
    QLabel* m_connectionLabel;
    QProgressBar* m_connectionProgress;
    
    // State management
    bool m_isConnected;
    bool m_isRetrying;
    bool m_showControls;
    bool m_compactMode;
    QString m_lastError;
    QImage m_lastFrameImage;
    
    // Timers
    QTimer* m_statusUpdateTimer;
    QTimer* m_retryTimer;
    
    // Constants
    static const int STATUS_UPDATE_INTERVAL = 1000; // ms
    static const int RETRY_INTERVAL = 5000; // ms
    static const int MAX_RETRY_ATTEMPTS = 3;
    
    int m_retryCount;
};

// Standalone Preview Window
class CameraPreviewWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CameraPreviewWindow(const CameraConfig& camera, QWidget *parent = nullptr);
    ~CameraPreviewWindow() = default;

    CameraPreviewWidget* getPreviewWidget() const { return m_previewWidget; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onFullscreenToggled();

private:
    void setupWindow();

    CameraPreviewWidget* m_previewWidget;
    bool m_isFullscreen;
};

#endif // CAMERAPREVIEWWIDGET_H