#include "CameraPreviewWidget.h"
#include "Logger.h"
#include <QGroupBox>
#include <QApplication>
#include <QScreen>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QVideoFrame>
#include <QPainter>

CameraPreviewWidget::CameraPreviewWidget(QWidget *parent)
    : QWidget(parent)
    , m_mediaPlayer(nullptr)
    , m_videoSink(nullptr)
    , m_audioOutput(nullptr)
    , m_isConnected(false)
    , m_isRetrying(false)
    , m_showControls(true)
    , m_compactMode(false)
    , m_retryCount(0)
{
    setupUI();
    setupMediaPlayer();
}

CameraPreviewWidget::CameraPreviewWidget(const CameraConfig& camera, QWidget *parent)
    : QWidget(parent)
    , m_camera(camera)
    , m_mediaPlayer(nullptr)
    , m_videoSink(nullptr)
    , m_audioOutput(nullptr)
    , m_isConnected(false)
    , m_isRetrying(false)
    , m_showControls(true)
    , m_compactMode(false)
    , m_retryCount(0)
{
    setupUI();
    setupMediaPlayer();
    setCamera(camera);
}

CameraPreviewWidget::~CameraPreviewWidget()
{
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
    }
    
    if (m_statusUpdateTimer) {
        m_statusUpdateTimer->stop();
    }
    
    if (m_retryTimer) {
        m_retryTimer->stop();
    }
}

void CameraPreviewWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);

    // Video display group
    m_videoGroupBox = new QGroupBox();
    m_videoLayout = new QVBoxLayout(m_videoGroupBox);
    
    // Camera info label
    m_cameraInfoLabel = new QLabel("No camera selected");
    m_cameraInfoLabel->setStyleSheet("font-weight: bold; color: #2E5984;");
    m_cameraInfoLabel->setAlignment(Qt::AlignCenter);
    m_videoLayout->addWidget(m_cameraInfoLabel);
    
    // Video display label (we render frames ourselves to avoid native window overlay)
    m_videoLabel = new QLabel;
    m_videoLabel->setMinimumSize(240, 180);
    m_videoLabel->setMaximumHeight(280);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_videoLabel->setStyleSheet("background-color: #000000; color: #ffffff; border: 2px solid #cccccc; padding: 0px;");
    m_videoLabel->setText("No camera selected");
    m_videoLayout->addWidget(m_videoLabel);
    
    // Status label
    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("padding: 5px; background-color: #f0f0f0; border-radius: 3px;");
    m_videoLayout->addWidget(m_statusLabel);
    
    m_mainLayout->addWidget(m_videoGroupBox);

    // Controls group
    m_controlsGroupBox = new QGroupBox("Controls");
    m_controlsLayout = new QHBoxLayout(m_controlsGroupBox);
    
    // Playback controls
    m_playButton = new QPushButton("Play");
    m_playButton->setEnabled(false);
    m_playButton->setMinimumSize(60, 30);
    connect(m_playButton, &QPushButton::clicked, this, &CameraPreviewWidget::togglePlayback);
    
    m_stopButton = new QPushButton("Stop");
    m_stopButton->setEnabled(false);
    m_stopButton->setMinimumSize(60, 30);
    connect(m_stopButton, &QPushButton::clicked, this, &CameraPreviewWidget::stop);
    
    m_reconnectButton = new QPushButton("Reconnect");
    m_reconnectButton->setEnabled(false);
    m_reconnectButton->setMinimumSize(70, 30);
    connect(m_reconnectButton, &QPushButton::clicked, this, &CameraPreviewWidget::reconnect);
    
    m_snapshotButton = new QPushButton("Snapshot");
    m_snapshotButton->setEnabled(false);
    m_snapshotButton->setMinimumSize(70, 30);
    connect(m_snapshotButton, &QPushButton::clicked, this, &CameraPreviewWidget::takeSnapshot);
    
    m_fullscreenButton = new QPushButton("Fullscreen");
    m_fullscreenButton->setEnabled(false);
    m_fullscreenButton->setMinimumSize(80, 30);
    connect(m_fullscreenButton, &QPushButton::clicked, this, &CameraPreviewWidget::toggleFullscreen);
    
    m_controlsLayout->addWidget(m_playButton);
    m_controlsLayout->addWidget(m_stopButton);
    m_controlsLayout->addWidget(m_reconnectButton);
    m_controlsLayout->addWidget(m_snapshotButton);
    m_controlsLayout->addWidget(m_fullscreenButton);
    
    // Volume control
    m_volumeLabel = new QLabel("Vol:");
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(50);
    m_volumeSlider->setMaximumWidth(100);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &CameraPreviewWidget::setVolume);
    
    m_controlsLayout->addWidget(m_volumeLabel);
    m_controlsLayout->addWidget(m_volumeSlider);
    
    m_controlsLayout->addStretch();
    
    // Connection status
    m_connectionLabel = new QLabel("Disconnected");
    m_connectionLabel->setStyleSheet("color: #d32f2f; font-weight: bold;");
    m_controlsLayout->addWidget(m_connectionLabel);
    
    m_connectionProgress = new QProgressBar;
    m_connectionProgress->setVisible(false);
    m_connectionProgress->setMaximumWidth(100);
    m_controlsLayout->addWidget(m_connectionProgress);
    
    // Hide controls by default - not needed for basic preview
    m_mainLayout->addWidget(m_controlsGroupBox);
    m_controlsGroupBox->setVisible(false);
    
    // Setup timers
    m_statusUpdateTimer = new QTimer(this);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &CameraPreviewWidget::updateConnectionStatus);
    
    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &CameraPreviewWidget::retryConnection);
}

void CameraPreviewWidget::setupMediaPlayer()
{
    m_mediaPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    
    m_videoSink = new QVideoSink(this);
    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &CameraPreviewWidget::onVideoFrameChanged);
    
    m_mediaPlayer->setVideoSink(m_videoSink);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    
    // Connect media player signals
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &CameraPreviewWidget::onMediaStatusChanged);
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
            this, &CameraPreviewWidget::onPlaybackStateChanged);
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred,
            this, &CameraPreviewWidget::onErrorOccurred);
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged,
            this, &CameraPreviewWidget::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged,
            this, &CameraPreviewWidget::onDurationChanged);
    
    // Set initial volume
    setVolume(50);
}

void CameraPreviewWidget::setCamera(const CameraConfig& camera)
{
    if (m_mediaPlayer && m_mediaPlayer->playbackState() != QMediaPlayer::StoppedState) {
        stop();
    }
    
    m_camera = camera;
    buildRtspUrl();
    
    // Update UI
    QString cameraInfo = QString("%1 (%2:%3)")
                         .arg(camera.name(), camera.ipAddress())
                         .arg(camera.port());
    m_cameraInfoLabel->setText(cameraInfo);
    // m_videoGroupBox->setTitle(QString("Camera Preview - %1").arg(camera.name()));
    m_videoLabel->setText("");
    
    // Enable controls
    m_playButton->setEnabled(true);
    m_reconnectButton->setEnabled(true);
    
    updateStatusDisplay();
    
    LOG_INFO(QString("Camera preview widget configured for: %1").arg(camera.name()), "CameraPreviewWidget");
}

void CameraPreviewWidget::clearCamera()
{
    stop();
    
    m_camera = CameraConfig();
    m_rtspUrl.clear();
    m_lastFrameImage = QImage();
    
    m_cameraInfoLabel->setText("No camera selected");
    m_videoGroupBox->setTitle("");
    m_videoLabel->setPixmap(QPixmap());
    
    // Disable controls
    m_playButton->setEnabled(false);
    m_stopButton->setEnabled(false);
    m_reconnectButton->setEnabled(false);
    m_snapshotButton->setEnabled(false);
    m_fullscreenButton->setEnabled(false);
    
    resetConnection();
}

void CameraPreviewWidget::buildRtspUrl()
{
    if (!m_camera.isValid()) {
        m_rtspUrl.clear();
        return;
    }
    
    QString username = m_camera.username();
    QString password = m_camera.password();
    QString ipAddress = m_camera.ipAddress();
    int port = m_camera.port();
    QString brand = m_camera.brand();
    
    // Generate brand-specific RTSP path
    QString rtspPath = "/stream1"; // Default
    if (brand == "Hikvision") {
        rtspPath = "/Streaming/Channels/101";
    } else if (brand == "CP Plus") {
        rtspPath = "/cam/realmonitor?channel=1&subtype=0";
    } else if (brand == "Dahua") {
        rtspPath = "/cam/realmonitor?channel=1&subtype=0";
    } else if (brand == "Axis") {
        rtspPath = "/axis-media/media.amp";
    } else if (brand == "Vivotek") {
        rtspPath = "/live.sdp";
    } else if (brand == "Foscam") {
        rtspPath = "/videoMain";
    }
    
    // Build RTSP URL with credentials if provided
    if (!username.isEmpty() || !password.isEmpty()) {
        if (!username.isEmpty() && !password.isEmpty()) {
            m_rtspUrl = QString("rtsp://%1:%2@%3:%4%5").arg(username, password, ipAddress).arg(port).arg(rtspPath);
        } else if (!username.isEmpty()) {
            m_rtspUrl = QString("rtsp://%1@%2:%3%4").arg(username, ipAddress).arg(port).arg(rtspPath);
        } else {
            m_rtspUrl = QString("rtsp://:%1@%2:%3%4").arg(password, ipAddress).arg(port).arg(rtspPath);
        }
    } else {
        m_rtspUrl = QString("rtsp://%1:%2%3").arg(ipAddress).arg(port).arg(rtspPath);
    }
    
    LOG_DEBUG(QString("Built RTSP URL for %1: %2").arg(m_camera.name(), m_rtspUrl), "CameraPreviewWidget");
}

void CameraPreviewWidget::play()
{
    if (!m_camera.isValid() || m_rtspUrl.isEmpty()) {
        showError("No camera configured or invalid RTSP URL");
        return;
    }
    
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        return; // Already playing
    }
    
    LOG_INFO(QString("Starting playback for camera: %1").arg(m_camera.name()), "CameraPreviewWidget");
    
    m_mediaPlayer->setSource(QUrl(m_rtspUrl));
    m_mediaPlayer->play();
    
    m_statusLabel->setText("Connecting...");
    m_connectionProgress->setVisible(true);
    m_connectionProgress->setRange(0, 0); // Indeterminate progress
    
    m_retryCount = 0;
    m_isRetrying = false;
    
    m_statusUpdateTimer->start(STATUS_UPDATE_INTERVAL);
}

void CameraPreviewWidget::pause()
{
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        m_mediaPlayer->pause();
        LOG_INFO(QString("Paused playback for camera: %1").arg(m_camera.name()), "CameraPreviewWidget");
    }
}

void CameraPreviewWidget::stop()
{
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
        LOG_INFO(QString("Stopped playback for camera: %1").arg(m_camera.name()), "CameraPreviewWidget");
    }
    
    m_statusUpdateTimer->stop();
    m_retryTimer->stop();
    
    resetConnection();
}

void CameraPreviewWidget::togglePlayback()
{
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        pause();
    } else {
        play();
    }
}

void CameraPreviewWidget::reconnect()
{
    stop();
    QTimer::singleShot(500, this, &CameraPreviewWidget::play); // Small delay before reconnecting
}

void CameraPreviewWidget::takeSnapshot()
{
    if (!isPlaying() || !m_isConnected) {
        QMessageBox::warning(this, "Visco Connect - Snapshot Error", 
                           "Camera must be connected and playing to take a snapshot.");
        return;
    }
    
    // Create snapshots directory
    QString snapshotsDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/ViscoConnect";
    QDir().mkpath(snapshotsDir);
    
    // Generate filename with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    QString filename = QString("%1_%2_%3.jpg")
                       .arg(m_camera.name().replace(" ", "_"), m_camera.ipAddress(), timestamp);
    QString filePath = snapshotsDir + "/" + filename;
    
    // Get current frame from last received image (already letterboxed)
    QPixmap pixmap;
    if (!m_lastFrameImage.isNull()) {
        QPixmap target(m_videoLabel->size());
        target.fill(Qt::black);
        QPainter painter(&target);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QPixmap framePixmap = QPixmap::fromImage(m_lastFrameImage);
        const QSize scaledSize = framePixmap.size().scaled(target.size(), Qt::KeepAspectRatio);
        const QPoint topLeft((target.width() - scaledSize.width()) / 2,
                            (target.height() - scaledSize.height()) / 2);
        painter.drawPixmap(topLeft, framePixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        painter.end();
        pixmap = target;
    } else {
        pixmap = m_videoLabel->grab();
    }
    
    if (pixmap.save(filePath, "JPG", 90)) {
        LOG_INFO(QString("Snapshot saved: %1").arg(filePath), "CameraPreviewWidget");
        QMessageBox::information(this, "Visco Connect - Snapshot Saved", 
                               QString("Snapshot saved to:\n%1").arg(filePath));
        emit snapshotTaken(filePath);
    } else {
        LOG_ERROR(QString("Failed to save snapshot: %1").arg(filePath), "CameraPreviewWidget");
        QMessageBox::warning(this, "Visco Connect - Snapshot Error", 
                           "Failed to save snapshot. Please check permissions and disk space.");
    }
}

void CameraPreviewWidget::toggleFullscreen()
{
    // Emit signal to parent window to handle fullscreen toggle
    // This will be handled by CameraPreviewWindow or MainWindow
    emit playbackStarted(); // Reuse existing signal or create new one for fullscreen
}

void CameraPreviewWidget::setVolume(int volume)
{
    if (m_audioOutput) {
        qreal linearVolume = QAudio::convertVolume(volume / 100.0, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
        m_audioOutput->setVolume(linearVolume);
    }
    
    m_volumeSlider->setValue(volume);
}

bool CameraPreviewWidget::isPlaying() const
{
    return m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState;
}

void CameraPreviewWidget::setShowControls(bool show)
{
    m_showControls = show;
    m_controlsGroupBox->setVisible(show);
}

void CameraPreviewWidget::setCompactMode(bool compact)
{
    m_compactMode = compact;
    
    if (compact) {
        m_videoLabel->setMinimumSize(160, 120);
        m_videoLabel->setMaximumHeight(180);
        m_cameraInfoLabel->setVisible(false);
        m_videoGroupBox->setTitle("");
    } else {
        m_videoLabel->setMinimumSize(240, 180);
        m_videoLabel->setMaximumHeight(280);
        m_cameraInfoLabel->setVisible(true);
        m_videoGroupBox->setTitle("");
    }
}

void CameraPreviewWidget::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::LoadingMedia:
        m_statusLabel->setText("Loading...");
        break;
    case QMediaPlayer::LoadedMedia:
        m_statusLabel->setText("Loaded");
        break;
    case QMediaPlayer::BufferingMedia:
        m_statusLabel->setText("Buffering...");
        break;
    case QMediaPlayer::BufferedMedia:
        m_statusLabel->setText("Buffered");
        m_isConnected = true;
        m_connectionLabel->setText("Connected");
        m_connectionLabel->setStyleSheet("color: #388e3c; font-weight: bold;");
        m_connectionProgress->setVisible(false);
        emit connectionEstablished();
        break;
    case QMediaPlayer::EndOfMedia:
        // For live streams, this usually means connection lost
        m_statusLabel->setText("Stream ended");
        m_isConnected = false;
        emit connectionLost();
        break;
    case QMediaPlayer::InvalidMedia:
        m_statusLabel->setText("Invalid media");
        showError("Invalid media source");
        break;
    default:
        break;
    }
    
    updateControls();
}

void CameraPreviewWidget::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    switch (state) {
    case QMediaPlayer::PlayingState:
        m_playButton->setText("Pause");
        m_stopButton->setEnabled(true);
        m_snapshotButton->setEnabled(true);
        m_fullscreenButton->setEnabled(true);
        emit playbackStarted();
        break;
    case QMediaPlayer::PausedState:
        m_playButton->setText("Play");
        m_statusLabel->setText("Paused");
        break;
    case QMediaPlayer::StoppedState:
        m_playButton->setText("Play");
        m_stopButton->setEnabled(false);
        m_snapshotButton->setEnabled(false);
        m_fullscreenButton->setEnabled(false);
        m_statusLabel->setText("Stopped");
        emit playbackStopped();
        resetConnection();
        break;
    }
}

void CameraPreviewWidget::onErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    QString errorMsg = QString("Media player error: %1").arg(errorString);
    LOG_ERROR(errorMsg, "CameraPreviewWidget");
    
    m_lastError = errorString;
    showError(errorMsg);
    
    emit errorOccurred(errorString);
    
    // Try to reconnect on network errors
    if (error == QMediaPlayer::NetworkError && m_retryCount < MAX_RETRY_ATTEMPTS) {
        m_retryCount++;
        m_isRetrying = true;
        m_statusLabel->setText(QString("Retrying... (%1/%2)").arg(m_retryCount).arg(MAX_RETRY_ATTEMPTS));
        m_retryTimer->start(RETRY_INTERVAL);
    }
}

void CameraPreviewWidget::onPositionChanged(qint64 position)
{
    // For live streams, position usually stays at 0
    Q_UNUSED(position)
}

void CameraPreviewWidget::onDurationChanged(qint64 duration)
{
    // For live streams, duration is usually unknown (-1)
    Q_UNUSED(duration)
}

void CameraPreviewWidget::updateConnectionStatus()
{
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState && m_isConnected) {
        // Connection is active, update status
        m_statusLabel->setText("Playing");
    }
}

void CameraPreviewWidget::retryConnection()
{
    if (m_isRetrying && m_retryCount <= MAX_RETRY_ATTEMPTS) {
        LOG_INFO(QString("Retrying connection for camera: %1 (attempt %2)").arg(m_camera.name()).arg(m_retryCount), "CameraPreviewWidget");
        play();
    } else {
        m_isRetrying = false;
        m_statusLabel->setText("Connection failed");
        showError("Failed to connect after multiple attempts");
    }
}

void CameraPreviewWidget::onVideoFrameChanged(const QVideoFrame& frame)
{
    QVideoFrame cloneFrame(frame);
    if (!cloneFrame.isValid()) {
        return;
    }

    const QImage image = cloneFrame.toImage();
    if (image.isNull()) {
        return;
    }

    m_lastFrameImage = image;

    // Render into an offscreen pixmap sized to the label, with letterboxing
    const QSize targetSize = m_videoLabel->size();
    if (targetSize.isEmpty()) {
        return;
    }

    QPixmap targetPixmap(targetSize);
    targetPixmap.fill(Qt::black);

    QPainter painter(&targetPixmap);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPixmap framePixmap = QPixmap::fromImage(image);
    const QSize scaledSize = framePixmap.size().scaled(targetSize, Qt::KeepAspectRatio);
    const QPoint topLeft((targetSize.width() - scaledSize.width()) / 2,
                        (targetSize.height() - scaledSize.height()) / 2);
    painter.drawPixmap(topLeft, framePixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    painter.end();

    m_videoLabel->setPixmap(targetPixmap);
}

void CameraPreviewWidget::updateControls()
{
    bool hasCamera = !m_camera.id().isEmpty();
    bool isPlaying = m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState;
    
    m_playButton->setEnabled(hasCamera);
    m_stopButton->setEnabled(isPlaying);
    m_reconnectButton->setEnabled(hasCamera);
    m_snapshotButton->setEnabled(isPlaying && m_isConnected);
    m_fullscreenButton->setEnabled(isPlaying && m_isConnected);
}

void CameraPreviewWidget::updateStatusDisplay()
{
    if (m_camera.isValid()) {
        m_statusLabel->setText("Ready to play");
    } else {
        m_statusLabel->setText("No camera selected");
    }
}

void CameraPreviewWidget::resetConnection()
{
    m_isConnected = false;
    m_isRetrying = false;
    m_retryCount = 0;
    m_connectionLabel->setText("Disconnected");
    m_connectionLabel->setStyleSheet("color: #d32f2f; font-weight: bold;");
    m_connectionProgress->setVisible(false);
}

void CameraPreviewWidget::showError(const QString& error)
{
    m_statusLabel->setText(QString("Error: %1").arg(error));
    m_statusLabel->setStyleSheet("color: #d32f2f;");
    
    // Reset style after 5 seconds
    QTimer::singleShot(5000, [this]() {
        m_statusLabel->setStyleSheet("");
        updateStatusDisplay();
    });
}

// CameraPreviewWindow implementation
CameraPreviewWindow::CameraPreviewWindow(const CameraConfig& camera, QWidget *parent)
    : QWidget(parent)
    , m_isFullscreen(false)
{
    setupWindow();
    
    m_previewWidget = new CameraPreviewWidget(camera, this);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_previewWidget);
    
    // Connect fullscreen signal
    connect(m_previewWidget, &CameraPreviewWidget::playbackStarted,
            this, &CameraPreviewWindow::onFullscreenToggled);
    
    // Auto-start playback
    QTimer::singleShot(500, m_previewWidget, &CameraPreviewWidget::play);
}

void CameraPreviewWindow::setupWindow()
{
    setWindowTitle("Visco Connect - Camera Preview");
    setMinimumSize(480, 360);
    resize(640, 480);
    
    // Set window icon if available
    setWindowIcon(QIcon(":/icons/camera.png"));
    
    // Set window flags for better user experience
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
}

void CameraPreviewWindow::closeEvent(QCloseEvent *event)
{
    if (m_previewWidget) {
        m_previewWidget->stop();
    }
    event->accept();
}

void CameraPreviewWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F11 || event->key() == Qt::Key_Escape) {
        onFullscreenToggled();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void CameraPreviewWindow::onFullscreenToggled()
{
    if (m_isFullscreen) {
        showNormal();
        m_isFullscreen = false;
    } else {
        showFullScreen();
        m_isFullscreen = true;
    }
}