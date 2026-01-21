#include "MainWindow.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "WindowsService.h"
#include "CameraDiscovery.h"
#include "VpnWidget.h"
#include "UserProfileWidget.h"
#include "NetworkInterfaceManager.h"
#include "EchoServer.h"
#include "PingResponder.h"
#include "CameraPreviewWidget.h"
#include <QApplication>
#include <QScreen>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QSplitter>
#include <QGroupBox>
#include <QTableWidget>
#include <QNetworkInterface>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QTextEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QTimer>
#include <QProcess>
#include <QProgressBar>
#include <QComboBox>
#include <QListWidget>
#include <QClipboard>
#include <QScrollArea>

Q_DECLARE_METATYPE(DiscoveredCamera)

// Camera Configuration Dialog
class CameraConfigDialog : public QDialog
{
    Q_OBJECT

public:    explicit CameraConfigDialog(const CameraConfig& camera = CameraConfig(), QWidget *parent = nullptr)
        : QDialog(parent), m_camera(camera)
    {
        setWindowTitle(camera.name().isEmpty() ? "Visco Connect - Add Camera" : "Visco Connect - Edit Camera");
        setModal(true);
        
        // Adapt to screen size
        if (parent) {
            QScreen* screen = parent->screen();
            if (screen) {
                QRect screenGeometry = screen->availableGeometry();
                int width = qMin(600, screenGeometry.width() - 200);
                int height = qMin(500, screenGeometry.height() - 200);
                resize(width, height);
            }
        } else {
            resize(500, 420);
        }
        
        setupUI();
        loadCamera();
    }
    
    CameraConfig getCamera() const { return m_camera; }

private slots:
    void accept() override
    {
        // Check for @ character in username or password
        QString username = m_usernameEdit->text().trimmed();
        QString password = m_passwordEdit->text();
        
        if (username.contains('@')) {
            QMessageBox::warning(this, "Visco Connect - Invalid Character", 
                "Username cannot contain '@' character as it conflicts with RTSP URL format.\n\n"
                "Please change the Username from Camera Provider's website.");
            m_usernameEdit->setFocus();
            return;
        }
        
        if (password.contains('@')) {
            QMessageBox::warning(this, "Visco Connect - Invalid Character", 
                "Password cannot contain '@' character as it conflicts with RTSP URL format.\n\n"
                "Please change the Password from Camera Provider's website.");
            m_passwordEdit->setFocus();
            return;
        }
        
        saveCamera();
        if (m_camera.isValid()) {
            QDialog::accept();
        } else {            QMessageBox::warning(this, "Visco Connect - Configuration Error", 
                               "Please check all fields are correctly filled.");
        }
    }

private:    void setupUI()
    {
        // Use scroll area for small screens
        QScrollArea* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        
        QWidget* contentWidget = new QWidget;
        QFormLayout* layout = new QFormLayout(contentWidget);
        layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
          m_nameEdit = new QLineEdit(contentWidget);
        m_ipEdit = new QLineEdit(contentWidget);
        m_portSpinBox = new QSpinBox(contentWidget);
        m_portSpinBox->setRange(1, 65535);
        m_portSpinBox->setValue(554);
        
        // External port field with auto-assignment
        m_externalPortSpinBox = new QSpinBox(contentWidget);
        m_externalPortSpinBox->setRange(1, 65535);
        m_externalPortSpinBox->setValue(8551);
        
        m_autoAssignPortButton = new QPushButton("Auto-Assign", contentWidget);
        m_autoAssignPortButton->setToolTip("Automatically assign next available port");
        connect(m_autoAssignPortButton, &QPushButton::clicked, this, &CameraConfigDialog::autoAssignPort);
        
        QWidget* externalPortWidget = new QWidget(contentWidget);
        QHBoxLayout* externalPortLayout = new QHBoxLayout(externalPortWidget);
        externalPortLayout->setContentsMargins(0, 0, 0, 0);
        externalPortLayout->addWidget(m_externalPortSpinBox);
        externalPortLayout->addWidget(m_autoAssignPortButton);
        
        m_brandComboBox = new QComboBox(contentWidget);
        m_brandComboBox->addItems({"Generic", "Hikvision", "CP Plus", "Dahua", "Axis", "Vivotek", "Foscam"});
        m_brandComboBox->setCurrentText("Generic");
        
        m_modelEdit = new QLineEdit(contentWidget);
        
        // Credentials section with group box for better organization
        QGroupBox* credentialsGroup = new QGroupBox("Camera Credentials", contentWidget);
        QFormLayout* credentialsLayout = new QFormLayout(credentialsGroup);
        
        m_usernameEdit = new QLineEdit(contentWidget);
        m_usernameEdit->setPlaceholderText("Enter camera username (e.g., admin)");
        
        // Password field with visibility toggle
        QWidget* passwordWidget = new QWidget(contentWidget);
        QHBoxLayout* passwordLayout = new QHBoxLayout(passwordWidget);
        passwordLayout->setContentsMargins(0, 0, 0, 0);
        
        m_passwordEdit = new QLineEdit(contentWidget);
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_passwordEdit->setPlaceholderText("Enter camera password");
          m_passwordVisibilityButton = new QPushButton(contentWidget);
        m_passwordVisibilityButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        m_passwordVisibilityButton->setToolTip("Show/Hide Password");
        m_passwordVisibilityButton->setMaximumWidth(30);
        m_passwordVisibilityButton->setFlat(true);
        
        connect(m_passwordVisibilityButton, &QPushButton::clicked, this, &CameraConfigDialog::togglePasswordVisibility);
        
        passwordLayout->addWidget(m_passwordEdit);
        passwordLayout->addWidget(m_passwordVisibilityButton);
        
        credentialsLayout->addRow("Username:", m_usernameEdit);
        credentialsLayout->addRow("Password:", passwordWidget);
        
        // Add credential presets button
        m_credentialPresetsButton = new QPushButton("Load Common Credentials", contentWidget);
        connect(m_credentialPresetsButton, &QPushButton::clicked, this, &CameraConfigDialog::showCredentialPresets);
        credentialsLayout->addRow("", m_credentialPresetsButton);
        
        m_enabledCheckBox = new QCheckBox(contentWidget);
        m_enabledCheckBox->setChecked(true);
          layout->addRow("Camera Name:", m_nameEdit);
        layout->addRow("IP Address:", m_ipEdit);
        layout->addRow("Port:", m_portSpinBox);
        layout->addRow("External Port:", externalPortWidget);
        layout->addRow("Brand:", m_brandComboBox);
        layout->addRow("Model:", m_modelEdit);
        layout->addRow(credentialsGroup);
        layout->addRow("Enabled:", m_enabledCheckBox);
        
        // RTSP URL preview
        m_rtspPreviewGroup = new QGroupBox("RTSP URL Preview", contentWidget);
        QVBoxLayout* rtspLayout = new QVBoxLayout(m_rtspPreviewGroup);
          m_rtspUrlLabel = new QLabel("rtsp://username:password@192.168.1.100:554/stream", contentWidget);
        m_rtspUrlLabel->setWordWrap(true);
        m_rtspUrlLabel->setMinimumHeight(40);
        m_rtspUrlLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 8px; border: 1px solid #ccc; border-radius: 4px; font-family: monospace; }");
        m_rtspUrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        
        QPushButton* copyUrlButton = new QPushButton("Copy to Clipboard", contentWidget);
        connect(copyUrlButton, &QPushButton::clicked, this, &CameraConfigDialog::copyRtspUrl);
        
        rtspLayout->addWidget(m_rtspUrlLabel);
        rtspLayout->addWidget(copyUrlButton);
        
        layout->addRow(m_rtspPreviewGroup);
        
        scrollArea->setWidget(contentWidget);
        
        // Main dialog layout with scroll area and buttons
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(scrollArea);
        
        QDialogButtonBox* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        
        mainLayout->addWidget(buttonBox);
          // Connect signals to update RTSP preview
        connect(m_usernameEdit, &QLineEdit::textChanged, this, &CameraConfigDialog::updateRtspPreview);
        connect(m_passwordEdit, &QLineEdit::textChanged, this, &CameraConfigDialog::updateRtspPreview);
        connect(m_ipEdit, &QLineEdit::textChanged, this, &CameraConfigDialog::updateRtspPreview);
        connect(m_portSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &CameraConfigDialog::updateRtspPreview);
        connect(m_externalPortSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &CameraConfigDialog::updateRtspPreview);
        connect(m_brandComboBox, &QComboBox::currentTextChanged, this, &CameraConfigDialog::updateRtspPreview);
    }    void loadCamera()
    {
        m_nameEdit->setText(m_camera.name());
        m_ipEdit->setText(m_camera.ipAddress());
        m_portSpinBox->setValue(m_camera.port() > 0 ? m_camera.port() : 554);
        m_externalPortSpinBox->setValue(m_camera.externalPort() > 0 ? m_camera.externalPort() : 8551);
        m_brandComboBox->setCurrentText(m_camera.brand().isEmpty() ? "Generic" : m_camera.brand());
        m_modelEdit->setText(m_camera.model());
        m_usernameEdit->setText(m_camera.username());
        m_passwordEdit->setText(m_camera.password());
        m_enabledCheckBox->setChecked(m_camera.isEnabled());
        
        // Update RTSP preview after loading
        updateRtspPreview();
    }
      void saveCamera()
    {
        m_camera.setName(m_nameEdit->text().trimmed());
        m_camera.setIpAddress(m_ipEdit->text().trimmed());
        m_camera.setPort(m_portSpinBox->value());
        m_camera.setExternalPort(m_externalPortSpinBox->value());
        m_camera.setBrand(m_brandComboBox->currentText());
        m_camera.setModel(m_modelEdit->text().trimmed());
        m_camera.setUsername(m_usernameEdit->text().trimmed());
        m_camera.setPassword(m_passwordEdit->text());
        m_camera.setEnabled(m_enabledCheckBox->isChecked());
    }CameraConfig m_camera;
    QLineEdit* m_nameEdit;
    QLineEdit* m_ipEdit;
    QSpinBox* m_portSpinBox;
    QSpinBox* m_externalPortSpinBox;
    QPushButton* m_autoAssignPortButton;
    QComboBox* m_brandComboBox;
    QLineEdit* m_modelEdit;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QCheckBox* m_enabledCheckBox;
    
    // UI enhancement elements
    QPushButton* m_passwordVisibilityButton;
    QPushButton* m_credentialPresetsButton;
    QGroupBox* m_rtspPreviewGroup;
    QLabel* m_rtspUrlLabel;
    
private slots:
    void togglePasswordVisibility()
    {
        if (m_passwordEdit->echoMode() == QLineEdit::Password) {
            m_passwordEdit->setEchoMode(QLineEdit::Normal);
            m_passwordVisibilityButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
            m_passwordVisibilityButton->setToolTip("Hide Password");
        } else {
            m_passwordEdit->setEchoMode(QLineEdit::Password);
            m_passwordVisibilityButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
            m_passwordVisibilityButton->setToolTip("Show Password");
        }
    }
      void showCredentialPresets()
    {
        QDialog presetDialog(this);
        presetDialog.setWindowTitle("Visco Connect - Camera Credentials");
        presetDialog.setModal(true);
        
        // Flexible sizing
        QScreen* screen = this->screen();
        if (screen) {
            QRect screenGeometry = screen->availableGeometry();
            int width = qMin(500, screenGeometry.width() - 300);
            int height = qMin(400, screenGeometry.height() - 300);
            presetDialog.resize(width, height);
        } else {
            presetDialog.resize(450, 350);
        }
        
        QVBoxLayout* layout = new QVBoxLayout(&presetDialog);
        
        QLabel* infoLabel = new QLabel("Select common camera credentials based on brand:", &presetDialog);
        layout->addWidget(infoLabel);
        
        // Add scroll area for list
        QScrollArea* scrollArea = new QScrollArea(&presetDialog);
        scrollArea->setWidgetResizable(true);
        QListWidget* presetList = new QListWidget;
        scrollArea->setWidget(presetList);
        
        // Add common presets
        QListWidgetItem* hikvisionItem = new QListWidgetItem("Hikvision: admin / admin");
        hikvisionItem->setData(Qt::UserRole, QStringList() << "admin" << "admin");
        presetList->addItem(hikvisionItem);
        
        QListWidgetItem* cpplusItem = new QListWidgetItem("CP Plus: admin / admin");
        cpplusItem->setData(Qt::UserRole, QStringList() << "admin" << "admin");
        presetList->addItem(cpplusItem);
        
        QListWidgetItem* dahuaItem = new QListWidgetItem("Dahua: admin / admin");
        dahuaItem->setData(Qt::UserRole, QStringList() << "admin" << "admin");
        presetList->addItem(dahuaItem);
        
        QListWidgetItem* axisItem = new QListWidgetItem("Axis: root / pass");
        axisItem->setData(Qt::UserRole, QStringList() << "root" << "pass");
        presetList->addItem(axisItem);
        
        QListWidgetItem* foscamItem = new QListWidgetItem("Foscam: admin / (empty)");
        foscamItem->setData(Qt::UserRole, QStringList() << "admin" << "");
        presetList->addItem(foscamItem);
        
        QListWidgetItem* genericItem = new QListWidgetItem("Generic: admin / password");
        genericItem->setData(Qt::UserRole, QStringList() << "admin" << "password");
        presetList->addItem(genericItem);
        
        QListWidgetItem* emptyItem = new QListWidgetItem("No Authentication (empty credentials)");
        emptyItem->setData(Qt::UserRole, QStringList() << "" << "");
        presetList->addItem(emptyItem);
        
        layout->addWidget(scrollArea);
        
        QDialogButtonBox* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &presetDialog);
        layout->addWidget(buttonBox);
        
        connect(buttonBox, &QDialogButtonBox::accepted, &presetDialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &presetDialog, &QDialog::reject);
        
        connect(presetList, &QListWidget::itemDoubleClicked, [&](QListWidgetItem* item) {
            QStringList credentials = item->data(Qt::UserRole).toStringList();
            if (credentials.size() >= 2) {
                m_usernameEdit->setText(credentials[0]);
                m_passwordEdit->setText(credentials[1]);
                presetDialog.accept();
            }
        });
        
        if (presetDialog.exec() == QDialog::Accepted) {
            QListWidgetItem* currentItem = presetList->currentItem();
            if (currentItem) {
                QStringList credentials = currentItem->data(Qt::UserRole).toStringList();
                if (credentials.size() >= 2) {
                    m_usernameEdit->setText(credentials[0]);
                    m_passwordEdit->setText(credentials[1]);
                    updateRtspPreview();
                }
            }
        }
    }
    
    void updateRtspPreview()
    {
        QString username = m_usernameEdit->text().trimmed();
        QString password = m_passwordEdit->text();
        QString ipAddress = m_ipEdit->text().trimmed();
        int port = m_portSpinBox->value();
        QString brand = m_brandComboBox->currentText();
        
        if (ipAddress.isEmpty()) {
            ipAddress = "192.168.1.100"; // Default placeholder
        }
        
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
        
        // Build RTSP URL with proper credentials handling
        QString rtspUrl;
        if (!username.isEmpty() || !password.isEmpty()) {
            if (!username.isEmpty() && !password.isEmpty()) {
                rtspUrl = QString("rtsp://%1:%2@%3:%4%5").arg(username, password, ipAddress).arg(port).arg(rtspPath);
            } else if (!username.isEmpty()) {
                rtspUrl = QString("rtsp://%1@%2:%3%4").arg(username, ipAddress).arg(port).arg(rtspPath);
            } else {
                // Only password (unusual but handled)
                rtspUrl = QString("rtsp://:%1@%2:%3%4").arg(password, ipAddress).arg(port).arg(rtspPath);
            }
        } else {
            // No credentials
            rtspUrl = QString("rtsp://%1:%2%3").arg(ipAddress).arg(port).arg(rtspPath);
        }
        
        m_rtspUrlLabel->setText(rtspUrl);
        
        // Update tooltip with additional format examples
        QString tooltip = QString("RTSP URL for %1 camera\n\nCommon formats for %1:\n").arg(brand);
        
        if (brand == "Hikvision") {
            tooltip += "• /Streaming/Channels/101 (Main stream)\n";
            tooltip += "• /Streaming/Channels/102 (Sub stream)\n";
            tooltip += "• /h264_stream";
        } else if (brand == "CP Plus") {
            tooltip += "• /cam/realmonitor?channel=1&subtype=0 (Main)\n";
            tooltip += "• /cam/realmonitor?channel=1&subtype=1 (Sub)\n";
            tooltip += "• /streaming/channels/1";
        } else if (brand == "Dahua") {
            tooltip += "• /cam/realmonitor?channel=1&subtype=0\n";
            tooltip += "• /streaming/channels/1";
        } else if (brand == "Axis") {
            tooltip += "• /axis-media/media.amp\n";
            tooltip += "• /mjpg/video.mjpg";
        } else {
            tooltip += "• /stream1\n• /live\n• /video1";
        }
        
        m_rtspUrlLabel->setToolTip(tooltip);
    }
      void copyRtspUrl()
    {
        QApplication::clipboard()->setText(m_rtspUrlLabel->text());
        QMessageBox::information(this, "Visco Connect - URL Copied", "RTSP URL copied to clipboard!");
    }
    
    void autoAssignPort()
    {
        // Start from 8551 and find next available port
        int nextPort = 8551;
        QSet<int> usedPorts;
          // Get all used external ports from main window (need parent access)
        MainWindow* mainWindow = qobject_cast<MainWindow*>(parent());
        if (mainWindow) {
            // Get used ports from camera manager
            const auto& cameras = mainWindow->getCameraManager()->getAllCameras();
            for (const auto& camera : cameras) {
                if (camera.externalPort() > 0) {
                    usedPorts.insert(camera.externalPort());
                }
            }
        }
        
        // Find next available port
        while (usedPorts.contains(nextPort)) {
            nextPort++;
        }
        
        m_externalPortSpinBox->setValue(nextPort);
        updateRtspPreview();
          QMessageBox::information(this, "Visco Connect - Port Assignment", 
            QString("Auto-assigned external port: %1").arg(nextPort));
    }
};

// Camera Information Dialog
class CameraInfoDialog : public QDialog
{
    Q_OBJECT

public:    explicit CameraInfoDialog(const CameraConfig& camera, QWidget *parent = nullptr)
        : QDialog(parent), m_camera(camera)
    {
        setWindowTitle(QString("Visco Connect - Camera Information - %1").arg(camera.name()));
        setModal(true);
        
        // Adapt to screen size
        if (parent) {
            QScreen* screen = parent->screen();
            if (screen) {
                QRect screenGeometry = screen->availableGeometry();
                int width = qMin(750, static_cast<int>(screenGeometry.width() * 0.6));
                int height = qMin(650, static_cast<int>(screenGeometry.height() * 0.7));
                resize(width, height);
            }
        } else {
            resize(650, 550);
        }
        
        setupUI();
        updateRtspInfo();
    }

private slots:
    void copyMainRtspUrl()
    {
        QApplication::clipboard()->setText(m_mainRtspLabel->text());
        showCopyMessage("Main RTSP URL copied to clipboard!");
    }
    
    void copyExternalRtspUrl()
    {
        QApplication::clipboard()->setText(m_externalRtspLabel->text());
        showCopyMessage("External RTSP URL copied to clipboard!");
    }
    
    void copyAlternativeUrl()
    {
        QListWidgetItem* currentItem = m_alternativeUrlsList->currentItem();
        if (currentItem) {
            QApplication::clipboard()->setText(currentItem->text());
            showCopyMessage("Alternative RTSP URL copied to clipboard!");
        }
    }
    
    void editCamera()
    {
        accept();
        QTimer::singleShot(0, parent(), [this]() {
            if (MainWindow* mainWindow = qobject_cast<MainWindow*>(parent())) {
                mainWindow->editCamera();
            }
        });
    }
    
    void testConnection()
    {
        accept();
        QTimer::singleShot(0, parent(), [this]() {
            if (MainWindow* mainWindow = qobject_cast<MainWindow*>(parent())) {
                mainWindow->testCamera();
            }
        });
    }

private:
    void setupUI()
    {
        // Use scroll area for small screens
        QScrollArea* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        
        QWidget* contentWidget = new QWidget;
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        
        // Camera details group
        QGroupBox* detailsGroup = new QGroupBox("Camera Details", contentWidget);
        QFormLayout* detailsLayout = new QFormLayout(detailsGroup);
        
        detailsLayout->addRow("Name:", new QLabel(m_camera.name()));
        detailsLayout->addRow("Brand:", new QLabel(m_camera.brand().isEmpty() ? "Generic" : m_camera.brand()));
        detailsLayout->addRow("Model:", new QLabel(m_camera.model().isEmpty() ? "Unknown" : m_camera.model()));
        detailsLayout->addRow("IP Address:", new QLabel(m_camera.ipAddress()));
        detailsLayout->addRow("Port:", new QLabel(QString::number(m_camera.port())));
        detailsLayout->addRow("External Port:", new QLabel(QString::number(m_camera.externalPort())));
        
        // Credentials info with privacy
        QString credentialInfo = generateCredentialInfo();
        detailsLayout->addRow("Credentials:", new QLabel(credentialInfo));
        
        detailsLayout->addRow("Status:", new QLabel(m_camera.isEnabled() ? "Enabled" : "Disabled"));
        
        contentLayout->addWidget(detailsGroup);
        
        // RTSP URLs group
        QGroupBox* rtspGroup = new QGroupBox("RTSP URLs", this);
        QVBoxLayout* rtspLayout = new QVBoxLayout(rtspGroup);
        
        // Main RTSP URL (local network)
        QHBoxLayout* mainRtspLayout = new QHBoxLayout;
        QLabel* mainLabel = new QLabel("Local Network URL:", this);
        mainLabel->setStyleSheet("font-weight: bold;");
        mainRtspLayout->addWidget(mainLabel);
        
        m_mainRtspLabel = new QLabel(this);
        m_mainRtspLabel->setWordWrap(true);
        m_mainRtspLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_mainRtspLabel->setStyleSheet("QLabel { background-color: #f0f8ff; padding: 8px; border: 1px solid #ddd; border-radius: 4px; font-family: monospace; }");
        
        QPushButton* copyMainBtn = new QPushButton("Copy", this);
        copyMainBtn->setMaximumWidth(60);
        connect(copyMainBtn, &QPushButton::clicked, this, &CameraInfoDialog::copyMainRtspUrl);
        
        QHBoxLayout* mainUrlLayout = new QHBoxLayout;
        mainUrlLayout->addWidget(m_mainRtspLabel, 1);
        mainUrlLayout->addWidget(copyMainBtn);
        
        rtspLayout->addLayout(mainRtspLayout);
        rtspLayout->addLayout(mainUrlLayout);
        
        // External RTSP URL (for external access)
        QHBoxLayout* externalRtspLayout = new QHBoxLayout;
        QLabel* externalLabel = new QLabel("External Access URL:", this);
        externalLabel->setStyleSheet("font-weight: bold;");
        externalRtspLayout->addWidget(externalLabel);
        
        m_externalRtspLabel = new QLabel(this);
        m_externalRtspLabel->setWordWrap(true);
        m_externalRtspLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_externalRtspLabel->setStyleSheet("QLabel { background-color: #f0fff0; padding: 8px; border: 1px solid #ddd; border-radius: 4px; font-family: monospace; }");
        
        QPushButton* copyExternalBtn = new QPushButton("Copy", this);
        copyExternalBtn->setMaximumWidth(60);
        connect(copyExternalBtn, &QPushButton::clicked, this, &CameraInfoDialog::copyExternalRtspUrl);
        
        QHBoxLayout* externalUrlLayout = new QHBoxLayout;
        externalUrlLayout->addWidget(m_externalRtspLabel, 1);
        externalUrlLayout->addWidget(copyExternalBtn);
        
        rtspLayout->addLayout(externalRtspLayout);
        rtspLayout->addLayout(externalUrlLayout);
        
        // Alternative RTSP paths
        QLabel* altLabel = new QLabel("Alternative RTSP Paths:", this);
        altLabel->setStyleSheet("font-weight: bold;");
        rtspLayout->addWidget(altLabel);
        
        m_alternativeUrlsList = new QListWidget(this);
        m_alternativeUrlsList->setMaximumHeight(120);
        connect(m_alternativeUrlsList, &QListWidget::itemDoubleClicked, this, &CameraInfoDialog::copyAlternativeUrl);
        rtspLayout->addWidget(m_alternativeUrlsList);
        
        QPushButton* copyAltBtn = new QPushButton("Copy Selected Alternative", this);
        connect(copyAltBtn, &QPushButton::clicked, this, &CameraInfoDialog::copyAlternativeUrl);
        rtspLayout->addWidget(copyAltBtn);
        
        contentLayout->addWidget(rtspGroup);
        
        scrollArea->setWidget(contentWidget);
        
        // Main dialog layout with scroll area and action buttons
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(scrollArea);
        
        // Action buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        
        QPushButton* editBtn = new QPushButton("Edit Camera", this);
        editBtn->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        connect(editBtn, &QPushButton::clicked, this, &CameraInfoDialog::editCamera);
        buttonLayout->addWidget(editBtn);
        
        QPushButton* testBtn = new QPushButton("Test Connection", this);
        testBtn->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
        connect(testBtn, &QPushButton::clicked, this, &CameraInfoDialog::testConnection);
        buttonLayout->addWidget(testBtn);
        
        buttonLayout->addStretch();
        
        QPushButton* closeBtn = new QPushButton("Close", this);
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        buttonLayout->addWidget(closeBtn);
        
        mainLayout->addLayout(buttonLayout);
    }
    
    void updateRtspInfo()
    {
        QString username = m_camera.username();
        QString password = m_camera.password();
        QString ipAddress = m_camera.ipAddress();
        int port = m_camera.port();
        int externalPort = m_camera.externalPort();
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
        
        // Build RTSP URLs with proper credentials handling
        QString baseLocalUrl = QString("rtsp://%1:%2").arg(ipAddress).arg(port);
        QString baseExternalUrl = QString("rtsp://[EXTERNAL_IP]:%1").arg(externalPort);
        
        QString localRtspUrl, externalRtspUrl;
        
        if (!username.isEmpty() || !password.isEmpty()) {
            if (!username.isEmpty() && !password.isEmpty()) {
                localRtspUrl = QString("rtsp://%1:%2@%3:%4%5").arg(username, password, ipAddress).arg(port).arg(rtspPath);
                externalRtspUrl = QString("rtsp://%1:%2@[EXTERNAL_IP]:%3%4").arg(username, password).arg(externalPort).arg(rtspPath);
            } else if (!username.isEmpty()) {
                localRtspUrl = QString("rtsp://%1@%2:%3%4").arg(username, ipAddress).arg(port).arg(rtspPath);
                externalRtspUrl = QString("rtsp://%1@[EXTERNAL_IP]:%2%3").arg(username).arg(externalPort).arg(rtspPath);
            } else {
                // Only password (unusual but handled)
                localRtspUrl = QString("rtsp://:%1@%2:%3%4").arg(password, ipAddress).arg(port).arg(rtspPath);
                externalRtspUrl = QString("rtsp://:%1@[EXTERNAL_IP]:%2%3").arg(password).arg(externalPort).arg(rtspPath);
            }
        } else {
            // No credentials
            localRtspUrl = QString("rtsp://%1:%2%3").arg(ipAddress).arg(port).arg(rtspPath);
            externalRtspUrl = QString("rtsp://[EXTERNAL_IP]:%1%2").arg(externalPort).arg(rtspPath);
        }
        
        m_mainRtspLabel->setText(localRtspUrl);
        m_externalRtspLabel->setText(externalRtspUrl);
        
        // Populate alternative RTSP paths
        populateAlternativePaths(username, password, ipAddress, port, brand);
    }
    
    void populateAlternativePaths(const QString& username, const QString& password, 
                                  const QString& ipAddress, int port, const QString& brand)
    {
        m_alternativeUrlsList->clear();
        
        QStringList alternativePaths;
        
        if (brand == "Hikvision") {
            alternativePaths << "/Streaming/Channels/102" << "/h264_stream" << "/ch1/main/av_stream";
        } else if (brand == "CP Plus") {
            alternativePaths << "/cam/realmonitor?channel=1&subtype=1" << "/streaming/channels/1" << "/stream1";
        } else if (brand == "Dahua") {
            alternativePaths << "/cam/realmonitor?channel=1&subtype=1" << "/streaming/channels/1" << "/stream1";
        } else if (brand == "Axis") {
            alternativePaths << "/mjpg/video.mjpg" << "/axis-media/media.amp?resolution=640x480";
        } else if (brand == "Foscam") {
            alternativePaths << "/videoSub" << "/mjpeg_stream";
        } else {
            alternativePaths << "/live" << "/video1" << "/cam1" << "/h264" << "/mjpeg";
        }
        
        for (const QString& path : alternativePaths) {
            QString url;
            if (!username.isEmpty() && !password.isEmpty()) {
                url = QString("rtsp://%1:%2@%3:%4%5").arg(username, password, ipAddress).arg(port).arg(path);
            } else if (!username.isEmpty()) {
                url = QString("rtsp://%1@%2:%3%4").arg(username, ipAddress).arg(port).arg(path);
            } else if (!password.isEmpty()) {
                url = QString("rtsp://:%1@%2:%3%4").arg(password, ipAddress).arg(port).arg(path);
            } else {
                url = QString("rtsp://%1:%2%3").arg(ipAddress).arg(port).arg(path);
            }
            
            QListWidgetItem* item = new QListWidgetItem(url);
            item->setToolTip("Double-click to copy this URL");
            m_alternativeUrlsList->addItem(item);
        }
    }
    
    QString generateCredentialInfo()
    {
        QString username = m_camera.username();
        QString password = m_camera.password();
        
        if (username.isEmpty() && password.isEmpty()) {
            return "No authentication";
        } else if (!username.isEmpty() && !password.isEmpty()) {
            return QString("Username: %1, Password: %2").arg(username, QString("*").repeated(password.length()));
        } else if (!username.isEmpty()) {
            return QString("Username: %1, No password").arg(username);
        } else {
            return QString("No username, Password: %1").arg(QString("*").repeated(password.length()));
        }
    }
    
    void showCopyMessage(const QString& message)
    {
        QLabel* statusLabel = new QLabel(message, this);
        statusLabel->setStyleSheet("QLabel { background-color: #d4edda; color: #155724; padding: 5px; border: 1px solid #c3e6cb; border-radius: 4px; }");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setAttribute(Qt::WA_DeleteOnClose);
        
        // Position at bottom of dialog temporarily
        statusLabel->setParent(this);
        statusLabel->setGeometry(10, height() - 40, width() - 20, 30);
        statusLabel->show();
        
        // Auto-hide after 2 seconds
        QTimer::singleShot(2000, statusLabel, &QLabel::deleteLater);
    }

private:
    CameraConfig m_camera;
    QLabel* m_mainRtspLabel;
    QLabel* m_externalRtspLabel;
    QListWidget* m_alternativeUrlsList;
};

// Camera Discovery Dialog
class CameraDiscoveryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraDiscoveryDialog(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_discovery(nullptr)
        , m_isScanning(false)
    {        setWindowTitle("Visco Connect - Discover Cameras");
        setModal(true);
        setMinimumSize(500, 400);
        
        // Adapt to screen size
        if (parent) {
            QScreen* screen = parent->screen();
            if (screen) {
                QRect screenGeometry = screen->availableGeometry();
                int width = qMin(900, static_cast<int>(screenGeometry.width() * 0.7));
                int height = qMin(700, static_cast<int>(screenGeometry.height() * 0.75));
                resize(width, height);
            }
        } else {
            resize(700, 500);
        }
        
        setupUI();
        setupDiscovery();
    }
    
    QList<DiscoveredCamera> getSelectedCameras() const { return m_selectedCameras; }

private slots:
    void startDiscovery()
    {
        if (m_isScanning) return;
        
        m_isScanning = true;
        m_discoveredCamerasWidget->clear();
        m_selectedCameras.clear();
        m_progressBar->setValue(0);
        m_progressBar->setVisible(true);
        m_statusLabel->setText("Scanning network for cameras...");
        m_scanButton->setText("Stop Scan");
        m_scanButton->setEnabled(true);
        
        QString networkRange = m_networkEdit->text().trimmed();
        if (networkRange.isEmpty()) {
            networkRange = CameraDiscovery::detectNetworkRange();
            m_networkEdit->setText(networkRange);
        }
        
        m_discovery->startDiscovery(networkRange);
    }
    
    void stopDiscovery()
    {
        if (!m_isScanning) return;
        
        m_discovery->stopDiscovery();
        m_isScanning = false;
        m_progressBar->setVisible(false);
        m_statusLabel->setText("Scan stopped");
        m_scanButton->setText("Start Scan");
    }
    
    void onDiscoveryStarted()
    {
        m_isScanning = true;
        m_statusLabel->setText("Scanning network...");
    }
    
    void onDiscoveryFinished()
    {
        m_isScanning = false;
        m_progressBar->setVisible(false);
        m_statusLabel->setText(QString("Scan completed. Found %1 cameras.").arg(m_discoveredCamerasWidget->count()));
        m_scanButton->setText("Start Scan");
        m_scanButton->setEnabled(true);
    }
    
    void onDiscoveryProgress(int current, int total)
    {
        if (total > 0) {
            int percentage = (current * 100) / total;
            m_progressBar->setValue(percentage);
            m_statusLabel->setText(QString("Scanning... %1/%2 (%3%)")
                                   .arg(current).arg(total).arg(percentage));
        }
    }
    
    void onCameraDiscovered(const DiscoveredCamera& camera)
    {
        addCameraToList(camera);
    }
    
    void onSelectionChanged()
    {
        m_selectedCameras.clear();
        
        for (int i = 0; i < m_discoveredCamerasWidget->count(); ++i) {
            QListWidgetItem* item = m_discoveredCamerasWidget->item(i);
            if (item->checkState() == Qt::Checked) {
                DiscoveredCamera camera = item->data(Qt::UserRole).value<DiscoveredCamera>();
                m_selectedCameras.append(camera);
            }
        }
        
        m_addSelectedButton->setEnabled(!m_selectedCameras.isEmpty());
        m_selectedCountLabel->setText(QString("Selected: %1").arg(m_selectedCameras.size()));
    }
    
    void onAddSelected()
    {
        accept();
    }
    
    void onItemDoubleClicked(QListWidgetItem* item)
    {
        if (item) {
            // Toggle selection on double-click
            Qt::CheckState newState = (item->checkState() == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
            item->setCheckState(newState);
        }
    }

private:
    void setupUI()
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        
        // Network configuration
        QGroupBox* networkGroup = new QGroupBox("Network Configuration");
        QFormLayout* networkLayout = new QFormLayout(networkGroup);
        
        m_networkEdit = new QLineEdit(this);
        m_networkEdit->setText(CameraDiscovery::detectNetworkRange());
        m_networkEdit->setPlaceholderText("e.g., 192.168.1.0/24");
        networkLayout->addRow("Network Range:", m_networkEdit);
        
        mainLayout->addWidget(networkGroup);
        
        // Control buttons
        QHBoxLayout* controlLayout = new QHBoxLayout;
        m_scanButton = new QPushButton("Start Scan", this);
        connect(m_scanButton, &QPushButton::clicked, this, [this]() {
            if (m_isScanning) {
                stopDiscovery();
            } else {
                startDiscovery();
            }
        });
        controlLayout->addWidget(m_scanButton);
        
        m_statusLabel = new QLabel("Ready to scan", this);
        controlLayout->addWidget(m_statusLabel);
        controlLayout->addStretch();
        
        m_progressBar = new QProgressBar(this);
        m_progressBar->setVisible(false);
        controlLayout->addWidget(m_progressBar);
        
        mainLayout->addLayout(controlLayout);
        
        // Discovered cameras
        QGroupBox* camerasGroup = new QGroupBox("Discovered Cameras");
        QVBoxLayout* camerasLayout = new QVBoxLayout(camerasGroup);
        
        m_discoveredCamerasWidget = new QListWidget(this);
        m_discoveredCamerasWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        connect(m_discoveredCamerasWidget, &QListWidget::itemChanged, this, &CameraDiscoveryDialog::onSelectionChanged);
        connect(m_discoveredCamerasWidget, &QListWidget::itemDoubleClicked, this, &CameraDiscoveryDialog::onItemDoubleClicked);
        camerasLayout->addWidget(m_discoveredCamerasWidget);
        
        QHBoxLayout* selectionLayout = new QHBoxLayout;
        m_selectedCountLabel = new QLabel("Selected: 0", this);
        selectionLayout->addWidget(m_selectedCountLabel);
        selectionLayout->addStretch();
        camerasLayout->addLayout(selectionLayout);
        
        mainLayout->addWidget(camerasGroup);
        
        // Dialog buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        m_addSelectedButton = new QPushButton("Add Selected Cameras", this);
        m_addSelectedButton->setEnabled(false);
        connect(m_addSelectedButton, &QPushButton::clicked, this, &CameraDiscoveryDialog::onAddSelected);
        buttonLayout->addWidget(m_addSelectedButton);
        
        QPushButton* cancelButton = new QPushButton("Cancel", this);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        buttonLayout->addWidget(cancelButton);
        
        mainLayout->addLayout(buttonLayout);
    }
    
    void setupDiscovery()
    {
        m_discovery = new CameraDiscovery(this);
        
        connect(m_discovery, &CameraDiscovery::discoveryStarted, this, &CameraDiscoveryDialog::onDiscoveryStarted);
        connect(m_discovery, &CameraDiscovery::discoveryFinished, this, &CameraDiscoveryDialog::onDiscoveryFinished);
        connect(m_discovery, &CameraDiscovery::discoveryProgress, this, &CameraDiscoveryDialog::onDiscoveryProgress);
        connect(m_discovery, &CameraDiscovery::cameraDiscovered, this, &CameraDiscoveryDialog::onCameraDiscovered);
    }
    
    void addCameraToList(const DiscoveredCamera& camera)
    {
        QListWidgetItem* item = new QListWidgetItem(m_discoveredCamerasWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        
        // Create display text with brand, IP, and model info
        QString displayText = QString("[%1] %2:%3")
                              .arg(camera.brand, camera.ipAddress).arg(camera.port);
        
        if (!camera.model.isEmpty() && camera.model != "Unknown") {
            displayText += QString(" - %1").arg(camera.model);
        }
        
        if (!camera.deviceName.isEmpty()) {
            displayText += QString(" (%1)").arg(camera.deviceName);
        }
        
        // Add RTSP URL hint
        displayText += QString("\nRTSP: %1").arg(camera.rtspUrl);
        
        item->setText(displayText);
        
        // Set icon based on brand
        if (camera.brand == "Hikvision") {
            item->setBackground(QColor(230, 250, 230)); // Light green
        } else if (camera.brand == "CP Plus") {
            item->setBackground(QColor(230, 230, 250)); // Light blue
        } else {
            item->setBackground(QColor(250, 250, 230)); // Light yellow for generic
        }
        
        // Store camera data
        item->setData(Qt::UserRole, QVariant::fromValue(camera));
        
        m_discoveredCamerasWidget->addItem(item);
    }

private:
    CameraDiscovery* m_discovery;
    bool m_isScanning;
    QList<DiscoveredCamera> m_selectedCameras;
    
    // UI elements
    QLineEdit* m_networkEdit;
    QPushButton* m_scanButton;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QListWidget* m_discoveredCamerasWidget;
    QLabel* m_selectedCountLabel;
    QPushButton* m_addSelectedButton;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_isClosingToTray(false)
    , m_forceQuit(false)
    , m_pingProcess(nullptr)
{    setWindowTitle("Visco Connect");
    
    // Make window size adapt to available screen space (70-80% of screen)
    QScreen* screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int defaultWidth = static_cast<int>(screenGeometry.width() * 0.75);
    int defaultHeight = static_cast<int>(screenGeometry.height() * 0.80);
    resize(defaultWidth, defaultHeight);
    
    // Set minimum size to very small to allow flexibility, allow maximize
    setMinimumSize(640, 480);
    // No maximum size - allow full screen
    
    // Create VpnWidget first as other components depend on its WireGuardManager
    LOG_INFO("Creating VpnWidget...", "MainWindow");
    m_vpnWidget = new VpnWidget;
    
    // Initialize camera manager with WireGuardManager
    LOG_INFO("Creating CameraManager...", "MainWindow");
    m_cameraManager = new CameraManager(m_vpnWidget->getWireGuardManager(), this);
    
    // Initialize network interface manager
    LOG_INFO("Creating NetworkInterfaceManager...", "MainWindow");
    m_networkManager = new NetworkInterfaceManager(this);
      // Initialize echo server for ping testing
    LOG_INFO("Creating EchoServer...", "MainWindow");
    m_echoServer = new EchoServer(this);
    
    // Initialize ping responder for ICMP ping replies
    LOG_INFO("Creating PingResponder...", "MainWindow");
    m_pingResponder = new PingResponder(this);
    
    LOG_INFO("Creating menu bar...", "MainWindow");
    createMenuBar();
    LOG_INFO("Creating status bar...", "MainWindow");
    createStatusBar();
    LOG_INFO("Creating central widget...", "MainWindow");
    createCentralWidget();
    LOG_INFO("Setting up connections...", "MainWindow");
    setupConnections();    // Initialize system tray
    LOG_INFO("Creating system tray manager...", "MainWindow");
    m_trayManager = new SystemTrayManager(this, m_vpnWidget, this);
    LOG_INFO("Initializing system tray...", "MainWindow");
    m_trayManager->initialize();
    
    // Setup system tray connections
    LOG_INFO("Setting up system tray connections...", "MainWindow");
    connect(m_trayManager, &SystemTrayManager::showMainWindow, this, [this]() {
        show();
        raise();
        activateWindow();
    });
    connect(m_trayManager, &SystemTrayManager::joinNetwork, this, [this]() {
        if (m_vpnWidget) {
            m_vpnWidget->connectToNetwork();
        }
    });
    connect(m_trayManager, &SystemTrayManager::leaveNetwork, this, [this]() {
        if (m_vpnWidget) {
            m_vpnWidget->disconnectFromNetwork();
        }
    });
    connect(m_trayManager, &SystemTrayManager::quitApplication, this, [this]() {
        m_forceQuit = true;
        close();
        qApp->quit();
    });
    
    // Connect network status changes to system tray updates
    LOG_INFO("Connecting network status monitoring...", "MainWindow");
    connect(m_vpnWidget, &VpnWidget::statusChanged, this, [this](const QString& status) {
        if (m_trayManager) {
            m_trayManager->updateVpnStatus();
            
            // Use the dedicated network notification method
            if (status.contains("Connected") && !status.contains("Connecting")) {
                m_trayManager->notifyVpnStatusChange(status, true);
            } else if (status == "Disconnected") {
                m_trayManager->notifyVpnStatusChange(status, false);
            }
        }
    });

    
    // Initial network status update for system tray
    LOG_INFO("Performing initial network status update...", "MainWindow");
    if (m_trayManager) {
        m_trayManager->updateVpnStatus();
    }
      // Load settings and initialize
    LOG_INFO("Loading settings...", "MainWindow");
    loadSettings();
    LOG_INFO("Initializing camera manager...", "MainWindow");
    m_cameraManager->initialize();
    
    // Connect network manager to port forwarder
    if (m_cameraManager->getPortForwarder()) {
        m_cameraManager->getPortForwarder()->setNetworkInterfaceManager(m_networkManager);
    }
    
    // Start network interface monitoring
    LOG_INFO("Starting network interface monitoring...", "MainWindow");
    m_networkManager->startMonitoring();    // Start echo server for remote ping testing
    LOG_INFO("Starting echo server...", "MainWindow");
    ConfigManager& config = ConfigManager::instance();
    if (config.isEchoServerEnabled()) {
        if (m_echoServer->startServer(config.getEchoServerPort())) {
            LOG_INFO(QString("Echo server started on port %1").arg(m_echoServer->serverPort()), "MainWindow");
        } else {
            LOG_WARNING("Failed to start echo server", "MainWindow");
        }
    } else {
        LOG_INFO("Echo server disabled in configuration", "MainWindow");
    }
    
    // Start ping responder for ICMP ping replies
    LOG_INFO("Starting ping responder...", "MainWindow");
    if (m_pingResponder->startResponder()) {
        LOG_INFO("ICMP ping responder started successfully", "MainWindow");
        showMessage("ICMP ping responder started - server will respond to ping requests");
    } else {
        LOG_WARNING("Failed to start ping responder - may need administrator privileges", "MainWindow");
        showMessage("Warning: ICMP ping responder failed to start. Run as administrator for ping functionality.");
    }
      LOG_INFO("Updating camera table...", "MainWindow");
    updateCameraTable();
    LOG_INFO("Updating buttons...", "MainWindow");
    updateButtons();
    
    // Initialize statistics refresh timer
    m_statisticsRefreshTimer = new QTimer(this);
    connect(m_statisticsRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshConnectionStatistics);
    m_statisticsRefreshTimer->start(2000); // Refresh every 2 seconds
    LOG_INFO("Connection statistics refresh timer started", "MainWindow");
    
    statusBar()->showMessage("Ready", 2000);
    LOG_INFO("MainWindow initialized successfully", "MainWindow");
}

MainWindow::~MainWindow()
{
    // Critical: Block ALL signals immediately to prevent any signal emission during destruction
    blockSignals(true);
    
    // Disconnect all signals before doing anything else
    disconnect();
    
    // Block signals on all child components to prevent cascading signals
    if (m_statisticsRefreshTimer) {
        m_statisticsRefreshTimer->blockSignals(true);
        m_statisticsRefreshTimer->stop();
    }
    
    if (m_cameraManager) {
        m_cameraManager->blockSignals(true);
    }
    
    if (m_networkManager) {
        m_networkManager->blockSignals(true);
    }
    
    if (m_echoServer) {
        m_echoServer->blockSignals(true);
    }
    
    if (m_pingResponder) {
        m_pingResponder->blockSignals(true);
    }
    
    if (m_vpnWidget) {
        m_vpnWidget->blockSignals(true);
    }
    
    if (m_userProfileWidget) {
        m_userProfileWidget->blockSignals(true);
    }
    
    if (m_trayManager) {
        m_trayManager->blockSignals(true);
    }
    
    if (m_previewWidget) {
        m_previewWidget->blockSignals(true);
    }
    
    // Now safely clean up resources without worrying about signals
    if (m_pingProcess) {
        if (m_pingProcess->state() != QProcess::NotRunning) {
            m_pingProcess->kill();
            m_pingProcess->waitForFinished(500);
        }
        delete m_pingProcess;
        m_pingProcess = nullptr;
    }
    
    if (m_cameraManager) {
        m_cameraManager->stopAllCameras();
        delete m_cameraManager;
        m_cameraManager = nullptr;
    }
    
    if (m_networkManager) {
        m_networkManager->stopMonitoring();
        delete m_networkManager;
        m_networkManager = nullptr;
    }
    
    if (m_echoServer) {
        m_echoServer->stopServer();
        delete m_echoServer;
        m_echoServer = nullptr;
    }
    
    if (m_pingResponder) {
        m_pingResponder->stopResponder();
        delete m_pingResponder;
        m_pingResponder = nullptr;
    }
    
    // Close preview windows
    for (QWidget* window : m_previewWindows) {
        window->close();
        window->deleteLater();
    }
    m_previewWindows.clear();
    
    // Clean up VPN widget
    if (m_vpnWidget) {
        delete m_vpnWidget;
        m_vpnWidget = nullptr;
    }
    
    if (m_userProfileWidget) {
        delete m_userProfileWidget;
        m_userProfileWidget = nullptr;
    }
    
    // Hide and clean tray
    if (m_trayManager) {
        m_trayManager->hide();
        delete m_trayManager;
        m_trayManager = nullptr;
    }
    
    saveSettings();
}

void MainWindow::showMessage(const QString& message)
{
    statusBar()->showMessage(message, 3000);
}

void MainWindow::appendLog(const QString& message)
{
    if (m_logTextEdit) {
        m_logTextEdit->append(message);
        
        // Limit log size
        if (m_logTextEdit->document()->blockCount() > 1000) {
            QTextCursor cursor = m_logTextEdit->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 100);
            cursor.removeSelectedText();
        }
        
        // Auto-scroll to bottom
        QScrollBar* scrollBar = m_logTextEdit->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{    // If the system tray is available and we're not forcing quit, close to tray
    if (m_trayManager && m_trayManager->isVisible() && !m_forceQuit) {
        hide();
        if (m_trayManager) {
            m_trayManager->showNotification("Visco Connect", 
                "Application closed to system tray. Right-click the tray icon for options.");
        }
        event->ignore();
        return;
    }
    
    // If we're force quitting or tray is not available, actually close
    try {
        // Block signals on this window immediately
        blockSignals(true);
        
        // Stop the statistics refresh timer first
        if (m_statisticsRefreshTimer) {
            m_statisticsRefreshTimer->stop();
            m_statisticsRefreshTimer->blockSignals(true);
        }
        
        saveSettings();
        
        // Disconnect everything before shutdown
        disconnect();
        
        // Block signals on all components
        if (m_cameraManager) {
            m_cameraManager->blockSignals(true);
            m_cameraManager->disconnect();
            m_cameraManager->stopAllCameras();
            m_cameraManager->shutdown();
        }
        
        // Stop network monitoring
        if (m_networkManager) {
            m_networkManager->blockSignals(true);
            m_networkManager->disconnect();
            m_networkManager->stopMonitoring();
        }
        
        // Stop echo server
        if (m_echoServer) {
            m_echoServer->blockSignals(true);
            m_echoServer->disconnect();
            m_echoServer->stopServer();
        }
        
        // Stop ping responder
        if (m_pingResponder) {
            m_pingResponder->blockSignals(true);
            m_pingResponder->disconnect();
            m_pingResponder->stopResponder();
        }
        
        // Stop any running ping process
        if (m_pingProcess) {
            m_pingProcess->kill();
            m_pingProcess->waitForFinished(500);
        }
        
        // Hide tray icon before quitting
        if (m_trayManager) {
            m_trayManager->blockSignals(true);
            m_trayManager->disconnect();
            m_trayManager->hide();
        }
    } catch (...) {
        // Silently ignore any exceptions during shutdown
        LOG_ERROR("Exception during window close event", "MainWindow");
    }
    
    event->accept();
}

void MainWindow::changeEvent(QEvent *event)
{
    // Let the window minimize normally to the taskbar
    // Don't hide to system tray on minimize
    QMainWindow::changeEvent(event);
}

void MainWindow::addCamera()
{
    CameraConfigDialog dialog(CameraConfig(), this);
    if (dialog.exec() == QDialog::Accepted) {
        CameraConfig camera = dialog.getCamera();
        if (m_cameraManager->addCamera(camera)) {
            showMessage(QString("Camera '%1' added successfully").arg(camera.name()));
        } else {
            QMessageBox::warning(this, "Visco Connect - Error", "Failed to add camera");
        }
    }
}

void MainWindow::discoverCameras()
{
    CameraDiscoveryDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QList<DiscoveredCamera> selectedCameras = dialog.getSelectedCameras();
        
        if (selectedCameras.isEmpty()) {
            showMessage("No cameras selected");
            return;
        }
        
        int addedCount = 0;
        for (const DiscoveredCamera& discoveredCamera : selectedCameras) {
            // Create CameraConfig from DiscoveredCamera
            CameraConfig camera;
            
            // Generate a name based on brand and IP
            QString cameraName = QString("%1_Camera_%2")
                                .arg(discoveredCamera.brand)
                                .arg(discoveredCamera.ipAddress.split('.').last());
            
            if (!discoveredCamera.deviceName.isEmpty() && 
                discoveredCamera.deviceName != cameraName) {
                cameraName = discoveredCamera.deviceName;
            }
            
            camera.setName(cameraName);
            camera.setIpAddress(discoveredCamera.ipAddress);
            camera.setPort(discoveredCamera.port == 80 ? 554 : discoveredCamera.port); // Default to RTSP port
            camera.setBrand(discoveredCamera.brand);
            camera.setModel(discoveredCamera.model);
            camera.setEnabled(true);
            
            // Set default credentials based on brand
            if (discoveredCamera.brand == "Hikvision") {
                camera.setUsername("admin");
                camera.setPassword("admin");
            } else if (discoveredCamera.brand == "CP Plus") {
                camera.setUsername("admin");
                camera.setPassword("admin");
            } else {
                camera.setUsername("admin");
                camera.setPassword("");
            }
            
            if (m_cameraManager->addCamera(camera)) {
                addedCount++;
                LOG_INFO(QString("Added discovered camera: %1 [%2] at %3")
                         .arg(camera.name(), camera.brand(), camera.ipAddress()), "MainWindow");
            } else {
                LOG_WARNING(QString("Failed to add discovered camera: %1 at %2")
                           .arg(cameraName, discoveredCamera.ipAddress), "MainWindow");
            }
        }
        
        showMessage(QString("Added %1 of %2 discovered cameras").arg(addedCount).arg(selectedCameras.size()));
        
        if (addedCount > 0) {
            // Show a message with RTSP URL information
            QString rtspInfo = "Discovered cameras have been added with suggested RTSP URLs:\n\n";
            for (const DiscoveredCamera& cam : selectedCameras) {
                rtspInfo += QString("• %1: %2\n").arg(cam.brand, cam.rtspUrl);
            }
            rtspInfo += "\nYou may need to adjust usernames, passwords, and RTSP paths for your specific cameras.";
            
            QMessageBox::information(this, "Visco Connect - Camera Discovery", rtspInfo);
        }
    }
}

void MainWindow::editCamera()
{
    int row = m_cameraTable->currentRow();
    if (row < 0) return;
    
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    if (!idItem) return;
      QString cameraId = idItem->data(Qt::UserRole).toString();
    CameraConfig camera = ConfigManager::instance().getCamera(cameraId);
    
    if (camera.id().isEmpty()) {
        QMessageBox::warning(this, "Visco Connect - Error", "Camera not found");
        return;
    }
    
    CameraConfigDialog dialog(camera, this);
    if (dialog.exec() == QDialog::Accepted) {
        CameraConfig updatedCamera = dialog.getCamera();
        if (m_cameraManager->updateCamera(cameraId, updatedCamera)) {
            showMessage(QString("Camera '%1' updated successfully").arg(updatedCamera.name()));
        } else {
            QMessageBox::warning(this, "Visco Connect - Error", "Failed to update camera");
        }
    }
}

void MainWindow::showCameraInfo()
{
    int row = m_cameraTable->currentRow();
    if (row < 0) return;
    
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    if (!idItem) return;
    
    QString cameraId = idItem->data(Qt::UserRole).toString();
    CameraConfig camera = ConfigManager::instance().getCamera(cameraId);
      if (camera.id().isEmpty()) {
        QMessageBox::warning(this, "Visco Connect - Error", "Camera not found");
        return;
    }
    
    CameraInfoDialog dialog(camera, this);
    dialog.exec();
}

void MainWindow::removeCamera()
{
    int row = m_cameraTable->currentRow();
    if (row < 0) return;
    
    QTableWidgetItem* nameItem = m_cameraTable->item(row, 1);
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    if (!nameItem || !idItem) return;
    
    QString cameraName = nameItem->text();
    QString cameraId = idItem->data(Qt::UserRole).toString();
    
    int ret = QMessageBox::question(this, "Visco Connect - Confirm Removal",
                                   QString("Are you sure you want to remove camera '%1'?").arg(cameraName),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // Stop preview if this camera is currently being previewed
        if (m_previewWidget->hasCamera() && m_previewWidget->getCamera().id() == cameraId) {
            m_previewWidget->stop();
            m_previewWidget->clearCamera();
        }
        
        if (m_cameraManager->removeCamera(cameraId)) {
            showMessage(QString("Camera '%1' removed successfully").arg(cameraName));
        } else {
            QMessageBox::warning(this, "Visco Connect - Error", "Failed to remove camera");
        }
    }
}

void MainWindow::toggleCamera()
{
    int row = m_cameraTable->currentRow();
    if (row < 0) return;
    
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    if (!idItem) return;
    
    QString cameraId = idItem->data(Qt::UserRole).toString();
    
    if (m_cameraManager->isCameraRunning(cameraId)) {
        m_cameraManager->stopCamera(cameraId);
    } else {
        m_cameraManager->startCamera(cameraId);
    }
}

void MainWindow::startAllCameras()
{
    m_cameraManager->startAllCameras();
    showMessage("Starting all enabled cameras...");
}

void MainWindow::stopAllCameras()
{
    m_cameraManager->stopAllCameras();
    showMessage("Stopping all cameras...");
}

void MainWindow::toggleAutoStart()
{
    bool enabled = m_autoStartCheckBox->isChecked();
    ConfigManager::instance().setAutoStartEnabled(enabled);
    showMessage(QString("Auto-start %1").arg(enabled ? "enabled" : "disabled"));
}

void MainWindow::showAbout()
{    QMessageBox::about(this, "About Visco Connect",
                      "Visco Connect v2.1.5\n\n"
                      "Demo Build - Verbose Logging Enabled\n\n"
                      "An advanced IP camera port forwarding solution.\n\n"
                      "Features:\n"
                      "• RTSP camera port forwarding\n"
                      "• Auto-discovery of network cameras\n"
                      "• System tray integration\n"
                      "• Windows service support\n"
                      "• Comprehensive logging");
}

void MainWindow::onCameraSelectionChanged()
{
    updateButtons();
}

void MainWindow::onCameraStarted(const QString& id)
{
    updateCameraTable();
    updateButtons();    CameraConfig camera = ConfigManager::instance().getCamera(id);
    showMessage(QString("Camera '%1' started").arg(camera.name()));
}

void MainWindow::onCameraStopped(const QString& id)
{
    updateCameraTable();
    updateButtons();    CameraConfig camera = ConfigManager::instance().getCamera(id);
    showMessage(QString("Camera '%1' stopped").arg(camera.name()));
}

void MainWindow::onCameraError(const QString& id, const QString& error)
{
    CameraConfig camera = ConfigManager::instance().getCamera(id);
    QString message = QString("Camera '%1' error: %2").arg(camera.name()).arg(error);
    showMessage(message);
    
    LOG_ERROR(message, "MainWindow");
}

void MainWindow::onConfigurationChanged()
{
    updateCameraTable();
    updateButtons();
    
    // Restart echo server if configuration changed
    restartEchoServer();
}

void MainWindow::onLogMessage(const QString& message)
{
    appendLog(message);
}

void MainWindow::refreshConnectionStatistics()
{
    // Only refresh if there are cameras and the table is visible
    if (m_cameraTable->rowCount() == 0) {
        return;
    }
    
    // Update connection statistics for each camera row
    for (int i = 0; i < m_cameraTable->rowCount(); ++i) {
        QTableWidgetItem* idItem = m_cameraTable->item(i, 0);
        if (!idItem) continue;
        
        QString cameraId = idItem->data(Qt::UserRole).toString();
        bool isRunning = m_cameraManager->isCameraRunning(cameraId);
        
        // Update data transferred (column 8)
        QString dataTransferred = "0 B";
        if (isRunning) {
            qint64 bytes = m_cameraManager->getPortForwarder()->getBytesTransferred(cameraId);
            if (bytes > 0) {
                if (bytes >= 1024 * 1024 * 1024) {
                    dataTransferred = QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
                } else if (bytes >= 1024 * 1024) {
                    dataTransferred = QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
                } else if (bytes >= 1024) {
                    dataTransferred = QString::number(bytes / 1024.0, 'f', 2) + " KB";
                } else {
                    dataTransferred = QString::number(bytes) + " B";
                }
            }
        }
        
        QTableWidgetItem* dataItem = m_cameraTable->item(i, 8);
        if (dataItem) {
            dataItem->setText(dataTransferred);
        }
        
        // Update action buttons state
        QWidget* actionWidget = m_cameraTable->cellWidget(i, 10);
        if (actionWidget) {
            // Find the start/stop button and update its state
            QPushButton* startStopBtn = actionWidget->findChild<QPushButton*>();
            if (startStopBtn) {
                CameraConfig camera = ConfigManager::instance().getCamera(cameraId);
                startStopBtn->setText(isRunning ? "Stop" : "Start");
                startStopBtn->setEnabled(camera.isEnabled());
            }
            
            // Find the restart button and update its state
            QList<QPushButton*> buttons = actionWidget->findChildren<QPushButton*>();
            if (buttons.size() > 1) {
                QPushButton* restartBtn = buttons[1]; // Second button is restart
                restartBtn->setEnabled(isRunning);
            }
        }
    }
}

void MainWindow::onNetworkInterfacesChanged()
{
    LOG_INFO("Network interfaces changed", "MainWindow");
    showMessage("Network interfaces changed");
    
    // Update status with current interface information
    updateNetworkStatus();
}

void MainWindow::onNetworkInterfaceRemoved(const QString& interfaceName)
{
    LOG_INFO(QString("Network interface removed: %1").arg(interfaceName), "MainWindow");
    showMessage(QString("Network interface removed: %1").arg(interfaceName));
    
    // Update status with current interface information
    updateNetworkStatus();
}

void MainWindow::onWireGuardStateChanged(bool isActive)
{
    QString status = isActive ? "active" : "inactive";
    LOG_INFO(QString("WireGuard state changed: %1").arg(status), "MainWindow");
    showMessage(QString("Network connection is now %1").arg(status));
    
    // Update status bar with network state
    if (isActive) {
        statusBar()->showMessage("Network Connected", 5000);
    } else {
        statusBar()->showMessage("Network Disconnected", 5000);
    }
    
    // Refresh camera table to show new status
    updateCameraTable();
}

void MainWindow::onEchoClientConnected(const QString& clientAddress)
{
    LOG_INFO(QString("Echo server: Client connected from %1").arg(clientAddress), "MainWindow");
    showMessage(QString("Ping client connected: %1").arg(clientAddress));
}

void MainWindow::onEchoClientDisconnected(const QString& clientAddress)
{
    LOG_INFO(QString("Echo server: Client disconnected from %1").arg(clientAddress), "MainWindow");
    showMessage(QString("Ping client disconnected: %1").arg(clientAddress));
}

void MainWindow::onEchoDataReceived(const QString& clientAddress, int bytesEchoed)
{
    // Log ping requests for debugging, but don't spam the UI for every ping
    static QMap<QString, qint64> lastLogTime;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    if (!lastLogTime.contains(clientAddress) || 
        currentTime - lastLogTime[clientAddress] > 30000) { // Log once every 30 seconds per client
        LOG_DEBUG(QString("Echo server: Received %1 bytes from %2").arg(bytesEchoed).arg(clientAddress), "MainWindow");
        lastLogTime[clientAddress] = currentTime;
    }
}

void MainWindow::onPingReceived(const QString& sourceAddress, quint16 identifier, quint16 sequence)
{
    // Log ping requests for debugging, but don't spam the UI for every ping
    static QMap<QString, qint64> lastLogTime;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    if (!lastLogTime.contains(sourceAddress) || 
        currentTime - lastLogTime[sourceAddress] > 10000) { // Log once every 10 seconds per source
        LOG_DEBUG(QString("ICMP ping received from %1 (ID: %2, Seq: %3)")
                  .arg(sourceAddress).arg(identifier).arg(sequence), "MainWindow");
        showMessage(QString("Ping received from %1").arg(sourceAddress));
        lastLogTime[sourceAddress] = currentTime;
    }
}

void MainWindow::onPingReplied(const QString& sourceAddress, quint16 identifier, quint16 sequence, quint32 responseTime)
{
    // Log successful ping replies occasionally
    static QMap<QString, qint64> lastLogTime;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    if (!lastLogTime.contains(sourceAddress) || 
        currentTime - lastLogTime[sourceAddress] > 10000) { // Log once every 10 seconds per source
        LOG_DEBUG(QString("ICMP ping replied to %1 (ID: %2, Seq: %3, Time: %4ms)")
                  .arg(sourceAddress).arg(identifier).arg(sequence).arg(responseTime), "MainWindow");
        lastLogTime[sourceAddress] = currentTime;
    }
}

void MainWindow::onPingResponderError(const QString& error)
{
    LOG_ERROR(QString("Ping responder error: %1").arg(error), "MainWindow");
    showMessage(QString("Ping responder error: %1").arg(error));
}

void MainWindow::updateNetworkStatus()
{
    if (!m_networkManager) return;
    
    auto interfaces = m_networkManager->getActiveInterfaces();
    bool hasWireGuard = m_networkManager->isWireGuardActive();
    
    QString statusText = QString("Interfaces: %1").arg(interfaces.size());
    if (hasWireGuard) {
        statusText += " (WireGuard active)";
    }
    
    // Update a network status label if we have one, or show in status bar
    statusBar()->showMessage(statusText, 3000);
}

void MainWindow::restartEchoServer()
{
    if (!m_echoServer) return;
      ConfigManager& config = ConfigManager::instance();
    
    // Stop the current server
    if (m_echoServer->isRunning()) {
        LOG_INFO("Stopping echo server for configuration change", "MainWindow");
        m_echoServer->stopServer();
    }
    
    // Start with new configuration if enabled
    if (config.isEchoServerEnabled()) {
        LOG_INFO(QString("Starting echo server on port %1").arg(config.getEchoServerPort()), "MainWindow");
        if (m_echoServer->startServer(config.getEchoServerPort())) {
            LOG_INFO(QString("Echo server restarted on port %1").arg(m_echoServer->serverPort()), "MainWindow");
            showMessage(QString("Echo server restarted on port %1").arg(m_echoServer->serverPort()));
        } else {
            LOG_WARNING("Failed to restart echo server", "MainWindow");
            showMessage("Failed to restart echo server");
        }
    } else {
        LOG_INFO("Echo server disabled in configuration", "MainWindow");
        showMessage("Echo server disabled");
    }
}

void MainWindow::createMenuBar()
{
    // File menu
    m_fileMenu = menuBar()->addMenu("&File");
    
    m_exitAction = new QAction("E&xit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    m_fileMenu->addAction(m_exitAction);
    
    // Preview menu
    QMenu* previewMenu = menuBar()->addMenu("&Preview");
    
    m_previewSelectedAction = new QAction("&Preview Selected Camera", this);
    m_previewSelectedAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_previewSelectedAction->setEnabled(false);
    connect(m_previewSelectedAction, &QAction::triggered, this, &MainWindow::previewCamera);
    previewMenu->addAction(m_previewSelectedAction);
    
    m_previewWindowAction = new QAction("Preview in &New Window", this);
    m_previewWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    m_previewWindowAction->setEnabled(false);
    connect(m_previewWindowAction, &QAction::triggered, this, &MainWindow::openCameraPreviewWindow);
    previewMenu->addAction(m_previewWindowAction);
    
    previewMenu->addSeparator();
    
    m_stopPreviewAction = new QAction("&Stop Preview", this);
    m_stopPreviewAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(m_stopPreviewAction, &QAction::triggered, [this]() {
        if (m_previewWidget->hasCamera()) {
            m_previewWidget->stop();
            m_previewWidget->clearCamera();
            updateButtons();
            showMessage("Preview stopped");
        }
    });
    previewMenu->addAction(m_stopPreviewAction);
    
    // Service menu
    m_serviceMenu = menuBar()->addMenu("&Service");
    
    m_installServiceAction = new QAction("&Install Service", this);
    connect(m_installServiceAction, &QAction::triggered, [this]() {
        if (WindowsService::instance().installService()) {
            QMessageBox::information(this, "Visco Connect - Service", "Service installed successfully");
        } else {
            QMessageBox::warning(this, "Visco Connect - Service Error", "Failed to install service");
        }
    });
    m_serviceMenu->addAction(m_installServiceAction);
    
    m_uninstallServiceAction = new QAction("&Uninstall Service", this);
    connect(m_uninstallServiceAction, &QAction::triggered, [this]() {
        if (WindowsService::instance().uninstallService()) {
            QMessageBox::information(this, "Visco Connect - Service", "Service uninstalled successfully");
        } else {
            QMessageBox::warning(this, "Visco Connect - Service Error", "Failed to uninstall service");
        }
    });
    m_serviceMenu->addAction(m_uninstallServiceAction);
    
    // Help menu
    m_helpMenu = menuBar()->addMenu("&Help");
    
    m_aboutAction = new QAction("&About", this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    m_helpMenu->addAction(m_aboutAction);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("Ready");
}

void MainWindow::createCentralWidget()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    // Main splitter
    m_mainSplitter = new QSplitter(Qt::Vertical, m_centralWidget);
    
    // Camera management group
    m_cameraGroupBox = new QGroupBox("Camera Configuration");
    QVBoxLayout* cameraLayout = new QVBoxLayout(m_cameraGroupBox);    // Camera table
    m_cameraTable = new QTableWidget(0, 11);
    QStringList headers = {"#", "Name", "Brand", "Model", "IP Address", "Port", "External Port", "Status", "Data Transferred", "Preview", "Actions"};
    m_cameraTable->setHorizontalHeaderLabels(headers);
    m_cameraTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cameraTable->setAlternatingRowColors(true);
    
    // Set specific column widths for better text display
    m_cameraTable->setColumnWidth(0, 35);   // #
    m_cameraTable->setColumnWidth(1, 120);  // Name
    m_cameraTable->setColumnWidth(2, 80);   // Brand
    m_cameraTable->setColumnWidth(3, 80);   // Model
    m_cameraTable->setColumnWidth(4, 120);  // IP Address
    m_cameraTable->setColumnWidth(5, 60);   // Port
    m_cameraTable->setColumnWidth(6, 90);   // External Port
    m_cameraTable->setColumnWidth(7, 80);   // Status
    m_cameraTable->setColumnWidth(8, 120);  // Data Transferred
    m_cameraTable->setColumnWidth(9, 80);  // Preview
    // Actions column will stretch to fill remaining space
    m_cameraTable->horizontalHeader()->setStretchLastSection(true);
    
    cameraLayout->addWidget(m_cameraTable);      // Camera buttons
    QHBoxLayout* cameraButtonLayout = new QHBoxLayout;
    m_addButton = new QPushButton("Add Camera");
    m_discoverButton = new QPushButton("Discover Cameras");
    m_editButton = new QPushButton("Edit Camera");
    m_removeButton = new QPushButton("Remove Camera");
    m_toggleButton = new QPushButton("Start/Stop");
    m_testButton = new QPushButton("Test Camera");
    m_previewButton = new QPushButton("Preview Camera");
    
    cameraButtonLayout->addWidget(m_addButton);
    cameraButtonLayout->addWidget(m_discoverButton);
    cameraButtonLayout->addWidget(m_editButton);
    cameraButtonLayout->addWidget(m_removeButton);
    cameraButtonLayout->addWidget(m_toggleButton);
    cameraButtonLayout->addWidget(m_testButton);
    cameraButtonLayout->addWidget(m_previewButton);
    cameraButtonLayout->addStretch();
    
    cameraLayout->addLayout(cameraButtonLayout);
    
    // Service control group
    m_serviceGroupBox = new QGroupBox("Service Control");
    QVBoxLayout* serviceLayout = new QVBoxLayout(m_serviceGroupBox);
    
    QHBoxLayout* serviceButtonLayout = new QHBoxLayout;
    m_startAllButton = new QPushButton("Start All Cameras");
    m_stopAllButton = new QPushButton("Stop All Cameras");
    
    serviceButtonLayout->addWidget(m_startAllButton);
    serviceButtonLayout->addWidget(m_stopAllButton);
    serviceButtonLayout->addStretch();
    
    serviceLayout->addLayout(serviceButtonLayout);
    
    // Auto-start checkbox
    m_autoStartCheckBox = new QCheckBox("Auto-start with Windows");
    serviceLayout->addWidget(m_autoStartCheckBox);
    
    // Service status
    m_serviceStatusLabel = new QLabel("Service Status: Ready");
    serviceLayout->addWidget(m_serviceStatusLabel);
    
    // Log viewer group
    m_logGroupBox = new QGroupBox("Application Log");
    QVBoxLayout* logLayout = new QVBoxLayout(m_logGroupBox);    m_logTextEdit = new QTextEdit;
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Consolas", 9));
    m_logTextEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    logLayout->addWidget(m_logTextEdit);
    
    QHBoxLayout* logButtonLayout = new QHBoxLayout;
    m_clearLogButton = new QPushButton("Clear Log");
    connect(m_clearLogButton, &QPushButton::clicked, m_logTextEdit, &QTextEdit::clear);
    
    logButtonLayout->addWidget(m_clearLogButton);
    logButtonLayout->addStretch();
    
    logLayout->addLayout(logButtonLayout);
      // Add to splitter
    QWidget* topWidget = new QWidget;
    QHBoxLayout* topMainLayout = new QHBoxLayout(topWidget);
    
    // Left side - Camera and Service controls
    QWidget* leftWidget = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);    leftLayout->addWidget(m_cameraGroupBox);
    leftLayout->addWidget(m_serviceGroupBox);
    
    // Camera Preview Panel
    m_previewGroupBox = new QGroupBox("Camera Preview");
    QVBoxLayout* previewLayout = new QVBoxLayout(m_previewGroupBox);
    
    m_previewWidget = new CameraPreviewWidget(this);
    m_previewWidget->setCompactMode(false);
    m_previewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    previewLayout->addWidget(m_previewWidget);
    
    // Connect preview widget signals for status updates
    connect(m_previewWidget, &CameraPreviewWidget::connectionEstablished,
            this, [this]() {
                showMessage("Camera preview connected successfully");
                m_previewGroupBox->setTitle(QString("Camera Preview - %1 (Connected)").arg(m_previewWidget->getCamera().name()));
            });
            
    connect(m_previewWidget, &CameraPreviewWidget::connectionLost,
            this, [this]() {
                showMessage("Camera preview connection lost");
                m_previewGroupBox->setTitle(QString("Camera Preview - %1 (Disconnected)").arg(m_previewWidget->getCamera().name()));
            });
            
    connect(m_previewWidget, &CameraPreviewWidget::errorOccurred,
            this, [this](const QString& error) {
                showMessage(QString("Camera preview error: %1").arg(error));
                m_previewGroupBox->setTitle(QString("Camera Preview - %1 (Error)").arg(m_previewWidget->getCamera().name()));
            });
            
    connect(m_previewWidget, &CameraPreviewWidget::snapshotTaken,
            this, [this](const QString& filePath) {
                showMessage(QString("Snapshot saved: %1").arg(filePath));
            });
    
    // Preview window button
    QHBoxLayout* previewButtonLayout = new QHBoxLayout;
    m_previewWindowButton = new QPushButton("Open in Separate Window");
    m_previewWindowButton->setEnabled(false);
    connect(m_previewWindowButton, &QPushButton::clicked, this, &MainWindow::openCameraPreviewWindow);
    previewButtonLayout->addWidget(m_previewWindowButton);
    previewButtonLayout->addStretch();
    previewLayout->addLayout(previewButtonLayout);
    
    leftLayout->addWidget(m_previewGroupBox);    // Right side - Network and User Profile controls in vertical layout
    QWidget* rightWidget = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setSpacing(12);
    
    // VpnWidget already created in initialize()
    m_vpnWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    
    m_userProfileWidget = new UserProfileWidget;
    m_userProfileWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    
    rightLayout->addWidget(m_vpnWidget);
    rightLayout->addWidget(m_userProfileWidget);
    rightLayout->addStretch(); // Add stretch to push widgets to top

    topMainLayout->addWidget(leftWidget, 2);
    topMainLayout->addWidget(rightWidget, 1);
    
    m_mainSplitter->addWidget(topWidget);    m_mainSplitter->addWidget(m_logGroupBox);
    m_mainSplitter->setStretchFactor(0, 3); // Main content gets 3x space
    m_mainSplitter->setStretchFactor(1, 1); // Log gets 1x space
    
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->addWidget(m_mainSplitter);
}

void MainWindow::setupConnections()
{
    // Camera table
    connect(m_cameraTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onCameraSelectionChanged);
    connect(m_cameraTable, &QTableWidget::itemDoubleClicked,
            this, &MainWindow::showCameraInfo);
    connect(m_cameraTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::showCameraContextMenu);
    
    // Enable context menu for camera table
    m_cameraTable->setContextMenuPolicy(Qt::CustomContextMenu);// Camera buttons
    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::addCamera);
    connect(m_discoverButton, &QPushButton::clicked, this, &MainWindow::discoverCameras);
    connect(m_editButton, &QPushButton::clicked, this, &MainWindow::editCamera);
    connect(m_removeButton, &QPushButton::clicked, this, &MainWindow::removeCamera);
    connect(m_toggleButton, &QPushButton::clicked, this, &MainWindow::toggleCamera);
    connect(m_testButton, &QPushButton::clicked, this, &MainWindow::testCamera);
    connect(m_previewButton, &QPushButton::clicked, this, &MainWindow::previewCamera);
    
    // Service buttons
    connect(m_startAllButton, &QPushButton::clicked, this, &MainWindow::startAllCameras);
    connect(m_stopAllButton, &QPushButton::clicked, this, &MainWindow::stopAllCameras);
    connect(m_autoStartCheckBox, &QCheckBox::toggled, this, &MainWindow::toggleAutoStart);
    
    // Camera manager
    connect(m_cameraManager, &CameraManager::cameraStarted,
            this, &MainWindow::onCameraStarted);
    connect(m_cameraManager, &CameraManager::cameraStopped,
            this, &MainWindow::onCameraStopped);
    connect(m_cameraManager, &CameraManager::cameraError,
            this, &MainWindow::onCameraError);
    connect(m_cameraManager, &CameraManager::configurationChanged,
            this, &MainWindow::onConfigurationChanged);    // Logger
    connect(&Logger::instance(), &Logger::logMessage,
            this, &MainWindow::onLogMessage);    // Network Interface Manager
    connect(m_networkManager, &NetworkInterfaceManager::interfacesChanged,
            this, &MainWindow::onNetworkInterfacesChanged);
    connect(m_networkManager, &NetworkInterfaceManager::interfaceRemoved,
            this, &MainWindow::onNetworkInterfaceRemoved);    connect(m_networkManager, &NetworkInterfaceManager::wireGuardInterfaceStateChanged,
            this, &MainWindow::onWireGuardStateChanged);    // Echo Server
    connect(m_echoServer, &EchoServer::clientConnected,
            this, &MainWindow::onEchoClientConnected);
    connect(m_echoServer, &EchoServer::clientDisconnected,
            this, &MainWindow::onEchoClientDisconnected);    connect(m_echoServer, &EchoServer::dataEchoed,
            this, &MainWindow::onEchoDataReceived);
    
    // Ping Responder
    connect(m_pingResponder, &PingResponder::pingReceived,
            this, &MainWindow::onPingReceived);
    connect(m_pingResponder, &PingResponder::pingReplied,
            this, &MainWindow::onPingReplied);
    connect(m_pingResponder, &PingResponder::errorOccurred,
            this, &MainWindow::onPingResponderError);
    
    // Configuration changes
    connect(&ConfigManager::instance(), &ConfigManager::configChanged,
            this, &MainWindow::onConfigurationChanged);
    
    // VPN Widget
    connect(m_vpnWidget, &VpnWidget::statusChanged,
            this, [this](const QString& status) {
                showMessage(QString("VPN Status: %1").arg(status));
            });
    connect(m_vpnWidget, &VpnWidget::logMessage,
            this, &MainWindow::onLogMessage);
}

void MainWindow::updateCameraTable()
{
    m_cameraTable->setRowCount(0);
    
    QList<CameraConfig> cameras = ConfigManager::instance().getAllCameras();
    for (int i = 0; i < cameras.size(); ++i) {
        const CameraConfig& camera = cameras[i];
        
        m_cameraTable->insertRow(i);
        
        // Index (hidden, stores camera ID)
        QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setData(Qt::UserRole, camera.id());
        m_cameraTable->setItem(i, 0, indexItem);
        
        // Name
        m_cameraTable->setItem(i, 1, new QTableWidgetItem(camera.name()));
        
        // Brand
        QTableWidgetItem* brandItem = new QTableWidgetItem(camera.brand());
        if (camera.brand() == "Hikvision") {
            brandItem->setBackground(QColor(230, 250, 230)); // Light green
        } else if (camera.brand() == "CP Plus") {
            brandItem->setBackground(QColor(230, 230, 250)); // Light blue
        } else if (camera.brand() == "Generic") {
            brandItem->setBackground(QColor(250, 250, 230)); // Light yellow
        }
        m_cameraTable->setItem(i, 2, brandItem);
        
        // Model
        m_cameraTable->setItem(i, 3, new QTableWidgetItem(camera.model().isEmpty() ? "Unknown" : camera.model()));
        
        // IP Address
        m_cameraTable->setItem(i, 4, new QTableWidgetItem(camera.ipAddress()));
        
        // Port
        m_cameraTable->setItem(i, 5, new QTableWidgetItem(QString::number(camera.port())));
        
        // External Port
        m_cameraTable->setItem(i, 6, new QTableWidgetItem(QString::number(camera.externalPort())));
        
        // Status
        bool isAnyWireGuardActive = m_networkManager->isWireGuardActive();
        bool isRunning = m_cameraManager->isCameraRunning(camera.id());
        QString status;
        if (!camera.isEnabled()) {
            status = "Disabled";
        } else if (isAnyWireGuardActive) {
            status = "Connected";
        } else {
            status = "Disconnected";
        }
        
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        if (status == "Connected") {
            statusItem->setBackground(QColor(144, 238, 144)); // Light green
        } else if (status == "Disabled") {
            statusItem->setBackground(QColor(211, 211, 211)); // Light gray
        } else {
            statusItem->setBackground(QColor(255, 182, 193)); // Light red
        }
        m_cameraTable->setItem(i, 7, statusItem);
        
        m_cameraTable->setItem(i, 7, statusItem);
        
        // Data Transferred column - shows bytes transferred
        QString dataTransferred = "0 B";
        if (isRunning) {
            qint64 bytes = m_cameraManager->getPortForwarder()->getBytesTransferred(camera.id());
            if (bytes > 0) {
                if (bytes >= 1024 * 1024 * 1024) {
                    dataTransferred = QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
                } else if (bytes >= 1024 * 1024) {
                    dataTransferred = QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
                } else if (bytes >= 1024) {
                    dataTransferred = QString::number(bytes / 1024.0, 'f', 2) + " KB";
                } else {
                    dataTransferred = QString::number(bytes) + " B";
                }
            }
        }
        QTableWidgetItem* dataItem = new QTableWidgetItem(dataTransferred);
        dataItem->setTextAlignment(Qt::AlignCenter);
        m_cameraTable->setItem(i, 8, dataItem);
        
        // Preview column - preview button for each camera
        QWidget* previewWidget = new QWidget();
        QHBoxLayout* previewLayout = new QHBoxLayout(previewWidget);
        previewLayout->setContentsMargins(2, 2, 2, 2);
        previewLayout->setSpacing(2);
        
        bool isPreviewing = m_previewWidget->hasCamera() && m_previewWidget->isPlaying() && m_previewWidget->getCamera().id() == camera.id();
        QPushButton* previewBtn = new QPushButton(isPreviewing ? "Stop" : "👁");
        previewBtn->setToolTip("Preview Camera Stream");
        previewBtn->setMaximumWidth(50);
        previewBtn->setMaximumHeight(25);
        connect(previewBtn, &QPushButton::clicked, [this, camera, previewBtn]() {
            // Toggle preview playback for this camera
            bool isCurrentCamera = m_previewWidget->hasCamera() && m_previewWidget->getCamera().id() == camera.id();
            if (isCurrentCamera && m_previewWidget->isPlaying()) {
                m_previewWidget->stop();
                previewBtn->setText("👁");
                showMessage(QString("Stopped preview for: %1").arg(camera.name()));
            } else {
                m_previewWidget->setCamera(camera);
                m_previewWidget->play();
                previewBtn->setText("Stop");
                showMessage(QString("Started preview for: %1").arg(camera.name()));
            }
            updateButtons();
        });
        
        previewLayout->addWidget(previewBtn);
        previewLayout->addStretch();
        
        m_cameraTable->setCellWidget(i, 9, previewWidget);
        
        // Actions column - control buttons for each camera
        QWidget* actionWidget = new QWidget();
        QHBoxLayout* actionLayout = new QHBoxLayout(actionWidget);
        actionLayout->setContentsMargins(2, 2, 2, 2);
        actionLayout->setSpacing(2);
        
        QPushButton* startStopBtn = new QPushButton(isRunning ? "Stop" : "Start");
        startStopBtn->setMaximumWidth(50);
        startStopBtn->setEnabled(camera.isEnabled());
        connect(startStopBtn, &QPushButton::clicked, [this, camera]() {
            if (m_cameraManager->isCameraRunning(camera.id())) {
                m_cameraManager->stopCamera(camera.id());
            } else {
                m_cameraManager->startCamera(camera.id());
            }
        });
        
        QPushButton* restartBtn = new QPushButton("↻");
        restartBtn->setToolTip("Restart Port Forwarding");
        restartBtn->setMaximumWidth(30);
        restartBtn->setEnabled(isRunning);
        connect(restartBtn, &QPushButton::clicked, [this, camera]() {
            m_cameraManager->getPortForwarder()->restartForwarding(camera.id());
        });
        
        QPushButton* testBtn = new QPushButton("Test");
        testBtn->setMaximumWidth(40);
        connect(testBtn, &QPushButton::clicked, [this, camera]() {
            // Set the current camera ID for testing
            QTableWidgetItem* idItem = m_cameraTable->item(m_cameraTable->currentRow(), 0);
            if (idItem) {
                testCamera();
            }
        });
        
        actionLayout->addWidget(startStopBtn);
        actionLayout->addWidget(restartBtn);
        actionLayout->addWidget(testBtn);
        actionLayout->addStretch();
        
        m_cameraTable->setCellWidget(i, 10, actionWidget);
    }
    
    // Resize columns to content
    m_cameraTable->resizeColumnsToContents();
}

void MainWindow::updateButtons()
{
    bool hasSelection = m_cameraTable->currentRow() >= 0;
    bool hasCamera = m_cameraTable->rowCount() > 0;
      m_editButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
    m_toggleButton->setEnabled(hasSelection);
    m_testButton->setEnabled(hasSelection);
    m_previewButton->setEnabled(hasSelection);
    
    // Update menu actions
    m_previewSelectedAction->setEnabled(hasSelection);
    m_previewWindowAction->setEnabled(hasSelection && m_previewWidget->hasCamera());
    
    m_startAllButton->setEnabled(hasCamera);
    m_stopAllButton->setEnabled(hasCamera);
    
    // Update toggle button text
    if (hasSelection) {
        QTableWidgetItem* idItem = m_cameraTable->item(m_cameraTable->currentRow(), 0);
        if (idItem) {
            QString cameraId = idItem->data(Qt::UserRole).toString();
            bool isRunning = m_cameraManager->isCameraRunning(cameraId);
            m_toggleButton->setText(isRunning ? "Stop Camera" : "Start Camera");
        }
    } else {
        m_toggleButton->setText("Start/Stop");
    }
    
    // Update preview window button
    m_previewWindowButton->setEnabled(hasSelection && m_previewWidget->hasCamera());
}

void MainWindow::loadSettings()
{
    QSettings settings;
    
    // Window geometry with safe fallbacks
    QByteArray geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    } else {
        // First time startup - center on screen with reasonable size
        resize(1200, 800);
        QScreen* screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->availableGeometry();
            int x = (screenGeometry.width() - width()) / 2;
            int y = (screenGeometry.height() - height()) / 2;
            move(x, y);
        }
    }
    
    restoreState(settings.value("windowState").toByteArray());
    
    // Splitter state
    if (m_mainSplitter) {
        QByteArray splitterState = settings.value("splitterState").toByteArray();
        if (!splitterState.isEmpty()) {
            m_mainSplitter->restoreState(splitterState);
        } else {
            // Default splitter sizes if no saved state
            m_mainSplitter->setSizes({600, 200});
        }
    }
    
    // Auto-start setting
    m_autoStartCheckBox->setChecked(ConfigManager::instance().isAutoStartEnabled());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    
    // Window geometry
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    // Splitter state
    if (m_mainSplitter) {
        settings.setValue("splitterState", m_mainSplitter->saveState());
    }
}

void MainWindow::testCamera()
{
    int row = m_cameraTable->currentRow();
    if (row < 0) return;
    
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    QTableWidgetItem* ipItem = m_cameraTable->item(row, 4);  // IP Address column
    QTableWidgetItem* testItem = m_cameraTable->item(row, 7); // Status column (used for feedback)
    
    if (!idItem || !ipItem || !testItem) return;
    
    QString cameraId = idItem->data(Qt::UserRole).toString();
    QString ipAddress = ipItem->text();
    
    // Clean up previous ping process
    if (m_pingProcess) {
        m_pingProcess->kill();
        m_pingProcess->deleteLater();
    }
    
    // Update UI to show testing state
    testItem->setText("Testing...");
    testItem->setBackground(QColor(255, 255, 0)); // Yellow
    m_testButton->setEnabled(false);
    
    // Store current testing camera
    m_currentTestingCameraId = cameraId;
    
    // Create new ping process
    m_pingProcess = new QProcess(this);
    connect(m_pingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onPingFinished);
    
    // Start ping command (Windows)
    QStringList arguments;
    arguments << "-n" << "3" << "-w" << "3000" << ipAddress; // 3 pings, 3 second timeout
    
    LOG_INFO(QString("Testing camera '%1' at IP: %2").arg(cameraId, ipAddress), "MainWindow");
    showMessage(QString("Testing camera at %1...").arg(ipAddress));
    
    m_pingProcess->start("ping", arguments);
    
    // Set a timeout timer in case ping hangs
    QTimer::singleShot(15000, this, [this]() {
        if (m_pingProcess && m_pingProcess->state() == QProcess::Running) {
            m_pingProcess->kill();
            onPingFinished(-1, QProcess::CrashExit);
        }
    });
}

void MainWindow::onPingFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Re-enable test button
    m_testButton->setEnabled(true);
    
    // Find the row for the current testing camera
    int testRow = -1;
    for (int i = 0; i < m_cameraTable->rowCount(); ++i) {
        QTableWidgetItem* idItem = m_cameraTable->item(i, 0);
        if (idItem && idItem->data(Qt::UserRole).toString() == m_currentTestingCameraId) {
            testRow = i;
            break;
        }
    }
      if (testRow >= 0) {
        QTableWidgetItem* testItem = m_cameraTable->item(testRow, 8); // Test column
        QTableWidgetItem* ipItem = m_cameraTable->item(testRow, 4);   // IP Address column
        
        if (testItem && ipItem) {
            QString ipAddress = ipItem->text();
            
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                // Ping successful
                testItem->setText("✓ Online");
                testItem->setBackground(QColor(144, 238, 144)); // Light green
                showMessage(QString("Camera at %1 is online and reachable").arg(ipAddress));
                LOG_INFO(QString("Ping test successful for camera at %1").arg(ipAddress), "MainWindow");
            } else {
                // Ping failed
                testItem->setText("✗ Offline");
                testItem->setBackground(QColor(255, 182, 193)); // Light red
                showMessage(QString("Camera at %1 is not reachable").arg(ipAddress));
                LOG_WARNING(QString("Ping test failed for camera at %1 (exit code: %2)").arg(ipAddress).arg(exitCode), "MainWindow");
            }
        }
    }
    
    // Clean up
    if (m_pingProcess) {
        m_pingProcess->deleteLater();
        m_pingProcess = nullptr;
    }
    
    m_currentTestingCameraId.clear();
}

void MainWindow::onUserLoginSuccessful()
{
    if (m_vpnWidget) {
        LOG_INFO("User login successful - triggering VPN auto-connect", "MainWindow");
        m_vpnWidget->onLoginSuccessful();
    }
}

void MainWindow::disconnectVpnOnLogout()
{
    if (m_vpnWidget) {
        LOG_INFO("User logout - disconnecting VPN and cleaning up configuration", "MainWindow");
        m_vpnWidget->disconnectAndCleanupOnLogout();
    }
}

void MainWindow::previewCamera()
{
    int currentRow = m_cameraTable->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "Visco Connect - No Selection", "Please select a camera to preview.");
        return;
    }
    
    QTableWidgetItem* idItem = m_cameraTable->item(currentRow, 0);
    if (!idItem) {
        QMessageBox::warning(this, "Visco Connect - Error", "Unable to get camera information.");
        return;
    }
    
    QString cameraId = idItem->data(Qt::UserRole).toString();
    QList<CameraConfig> cameras = ConfigManager::instance().getAllCameras();
    
    CameraConfig selectedCamera;
    for (const auto& camera : cameras) {
        if (camera.id() == cameraId) {
            selectedCamera = camera;
            break;
        }
    }
    
    if (!selectedCamera.isValid()) {
        QMessageBox::warning(this, "Visco Connect - Error", "Selected camera configuration is invalid.");
        return;
    }
    
    // Set the camera in the main preview widget
    m_previewWidget->setCamera(selectedCamera);
    m_previewWidget->play();
    
    updateButtons();
    
    LOG_INFO(QString("Started camera preview for: %1").arg(selectedCamera.name()), "MainWindow");
    showMessage(QString("Camera preview started for: %1").arg(selectedCamera.name()));
}

void MainWindow::openCameraPreviewWindow()
{
    if (!m_previewWidget->hasCamera()) {
        QMessageBox::warning(this, "Visco Connect - No Camera", "Please select a camera to preview first.");
        return;
    }
    
    CameraConfig camera = m_previewWidget->getCamera();
    
    // Create new preview window
    CameraPreviewWindow* previewWindow = new CameraPreviewWindow(camera, this);
    
    // Track the window
    m_previewWindows.append(previewWindow);
    
    // Connect window destroyed signal to clean up our list
    connect(previewWindow, &QObject::destroyed, this, [this, previewWindow]() {
        m_previewWindows.removeAll(previewWindow);
    });
    
    // Connect preview widget signals for status updates
    connect(previewWindow->getPreviewWidget(), &CameraPreviewWidget::connectionEstablished,
            this, [this, camera]() {
                showMessage(QString("Preview window connected to: %1").arg(camera.name()));
            });
    
    connect(previewWindow->getPreviewWidget(), &CameraPreviewWidget::errorOccurred,
            this, [this, camera](const QString& error) {
                showMessage(QString("Preview window error for %1: %2").arg(camera.name(), error));
            });
    
    previewWindow->show();
    previewWindow->raise();
    previewWindow->activateWindow();
    
    LOG_INFO(QString("Opened preview window for camera: %1").arg(camera.name()), "MainWindow");
    showMessage(QString("Opened preview window for: %1").arg(camera.name()));
}

void MainWindow::showCameraContextMenu(const QPoint& position)
{
    QTableWidgetItem* item = m_cameraTable->itemAt(position);
    if (!item) {
        return;
    }
    
    int row = item->row();
    QTableWidgetItem* idItem = m_cameraTable->item(row, 0);
    if (!idItem) {
        return;
    }
    
    QString cameraId = idItem->data(Qt::UserRole).toString();
    QList<CameraConfig> cameras = ConfigManager::instance().getAllCameras();
    
    CameraConfig selectedCamera;
    for (const auto& camera : cameras) {
        if (camera.id() == cameraId) {
            selectedCamera = camera;
            break;
        }
    }
    
    if (!selectedCamera.isValid()) {
        return;
    }
    
    QMenu contextMenu(this);
    
    // Preview actions
    QAction* previewAction = contextMenu.addAction("📺 Preview in Panel");
    previewAction->setIcon(QIcon(":/icons/preview.png"));
    connect(previewAction, &QAction::triggered, [this, selectedCamera]() {
        m_previewWidget->setCamera(selectedCamera);
        m_previewWidget->play();
        updateButtons();
        showMessage(QString("Started preview for: %1").arg(selectedCamera.name()));
    });
    
    QAction* previewWindowAction = contextMenu.addAction("🗔 Preview in New Window");
    previewWindowAction->setIcon(QIcon(":/icons/window.png"));
    connect(previewWindowAction, &QAction::triggered, [this, selectedCamera]() {
        CameraPreviewWindow* previewWindow = new CameraPreviewWindow(selectedCamera, this);
        m_previewWindows.append(previewWindow);
        connect(previewWindow, &QObject::destroyed, this, [this, previewWindow]() {
            m_previewWindows.removeAll(previewWindow);
        });
        previewWindow->show();
        previewWindow->raise();
        previewWindow->activateWindow();
    });
    
    contextMenu.addSeparator();
    
    // Camera operations
    bool isRunning = m_cameraManager->isCameraRunning(selectedCamera.id());
    
    QAction* toggleAction = contextMenu.addAction(isRunning ? "⏹ Stop Camera" : "▶ Start Camera");
    connect(toggleAction, &QAction::triggered, [this, selectedCamera, isRunning]() {
        if (isRunning) {
            m_cameraManager->stopCamera(selectedCamera.id());
        } else {
            m_cameraManager->startCamera(selectedCamera.id());
        }
    });
    
    QAction* testAction = contextMenu.addAction("🔍 Test Connection");
    connect(testAction, &QAction::triggered, [this, selectedCamera]() {
        // Select the camera row first
        for (int i = 0; i < m_cameraTable->rowCount(); ++i) {
            QTableWidgetItem* idItem = m_cameraTable->item(i, 0);
            if (idItem && idItem->data(Qt::UserRole).toString() == selectedCamera.id()) {
                m_cameraTable->selectRow(i);
                break;
            }
        }
        testCamera();
    });
    
    contextMenu.addSeparator();
    
    // Configuration actions
    QAction* editAction = contextMenu.addAction("✏ Edit Camera");
    connect(editAction, &QAction::triggered, [this, selectedCamera]() {
        // Select the camera row first
        for (int i = 0; i < m_cameraTable->rowCount(); ++i) {
            QTableWidgetItem* idItem = m_cameraTable->item(i, 0);
            if (idItem && idItem->data(Qt::UserRole).toString() == selectedCamera.id()) {
                m_cameraTable->selectRow(i);
                break;
            }
        }
        editCamera();
    });
    
    QAction* infoAction = contextMenu.addAction("ℹ Camera Information");
    connect(infoAction, &QAction::triggered, [this, selectedCamera]() {
        for (int i = 0; i < m_cameraTable->rowCount(); ++i) {
            QTableWidgetItem* idItem = m_cameraTable->item(i, 0);
            if (idItem && idItem->data(Qt::UserRole).toString() == selectedCamera.id()) {
                m_cameraTable->selectRow(i);
                break;
            }
        }
        showCameraInfo();
    });
    
    contextMenu.addSeparator();
    
    QAction* removeAction = contextMenu.addAction("🗑 Remove Camera");
    removeAction->setIcon(QIcon(":/icons/delete.png"));
    connect(removeAction, &QAction::triggered, [this, selectedCamera]() {
        for (int i = 0; i < m_cameraTable->rowCount(); ++i) {
            QTableWidgetItem* idItem = m_cameraTable->item(i, 0);
            if (idItem && idItem->data(Qt::UserRole).toString() == selectedCamera.id()) {
                m_cameraTable->selectRow(i);
                break;
            }
        }
        removeCamera();
    });
    
    // Show the context menu
    contextMenu.exec(m_cameraTable->mapToGlobal(position));
}

#include "MainWindow.moc" // Include MOC file for Q_OBJECT in CameraConfigDialog