#include "WireGuardConfigDialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QApplication>
#include <QStandardPaths>
#include <QClipboard>
#include <QRegularExpressionValidator>
#include <QRegularExpression>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QDebug>

// Constants
const QStringList WireGuardConfigDialog::DEFAULT_DNS_SERVERS = {
    "",  // Empty DNS (no default DNS servers)
    "8.8.8.8, 8.8.4.4",
    "1.1.1.1, 1.0.0.1",
    "9.9.9.9, 149.112.112.112"
};

const QStringList WireGuardConfigDialog::COMMON_ALLOWED_IPS = {
    "10.0.0.1/24",  // New default
    "0.0.0.0/0, ::/0",
    "0.0.0.0/0",
    "10.0.0.0/8",
    "192.168.0.0/16",
    "172.16.0.0/12"
};

WireGuardConfigDialog::WireGuardConfigDialog(WireGuardManager* wgManager, QWidget *parent)
    : QDialog(parent)
    , m_wireGuardManager(wgManager)
{
    setupUI();
    connectSignals();
    
    // Initialize with default values
    resetToDefaults();    setWindowTitle("Visco Connect - Network Configuration");
    setMinimumSize(700, 650);
    setMaximumSize(1000, 900);
    resize(750, 700);
}

WireGuardConfigDialog::WireGuardConfigDialog(WireGuardManager* wgManager, const WireGuardConfig& config, QWidget *parent)
    : QDialog(parent)
    , m_wireGuardManager(wgManager)
    , m_config(config)
{
    setupUI();
    connectSignals();
    
    setConfiguration(config);    setWindowTitle(QString("Visco Connect - Edit Network Configuration - %1").arg(config.interfaceConfig.name));
    setMinimumSize(700, 650);
    setMaximumSize(1000, 900);
    resize(750, 700);
}

WireGuardConfigDialog::~WireGuardConfigDialog()
{
}

WireGuardConfig WireGuardConfigDialog::getConfiguration() const
{
    return m_config;
}

void WireGuardConfigDialog::setConfiguration(const WireGuardConfig& config)
{
    m_config = config;
    
    // Debug logging
    qDebug() << "Setting configuration:" << config.interfaceConfig.name;
    qDebug() << "Private key length:" << config.interfaceConfig.privateKey.length();
    qDebug() << "Public key length:" << config.interfaceConfig.publicKey.length();
    if (!config.interfaceConfig.publicKey.isEmpty()) {
        qDebug() << "Public key preview:" << config.interfaceConfig.publicKey.left(16) + "...";
    }
      // Update interface configuration
    m_nameEdit->setText(config.interfaceConfig.name);
    m_privateKeyEdit->setText(config.interfaceConfig.privateKey);
    
    // Handle public key: For existing configs loaded from file, the public key should be 
    // derived from the private key. However, our generatePublicKey method doesn't support this.
    // For now, if we don't have a stored public key but have a private key, 
    // we'll leave the public key field empty and let the user regenerate if needed.
    if (!config.interfaceConfig.publicKey.isEmpty()) {
        m_publicKeyEdit->setText(config.interfaceConfig.publicKey);
    } else if (!config.interfaceConfig.privateKey.isEmpty()) {
        // We have private key but no public key - this can happen for configs loaded from files
        // since standard WireGuard configs don't store public keys in [Interface] section
        m_publicKeyEdit->setPlaceholderText("Click 'Generate New Keypair' to get the public key for this private key");
        m_publicKeyEdit->setText("");
    } else {
        m_publicKeyEdit->setText("");
    }
    m_addressEdit->setText(config.interfaceConfig.addresses.join(", "));
    m_dnsEdit->setText(config.interfaceConfig.dns.join(", "));
    m_listenPortSpin->setValue(config.interfaceConfig.listenPort);
    m_mtuSpin->setValue(config.interfaceConfig.mtu > 0 ? config.interfaceConfig.mtu : 1280);
      // Debug: Check what was actually set in the field
    qDebug() << "Public key field after setting:" << m_publicKeyEdit->text().left(16) + "..." << "Length:" << m_publicKeyEdit->text().length();
    
    // Important: Don't try to generate a public key from private key!
    // This was causing different public keys to appear each time.
    // WireGuard configurations should always have both keys stored together.
    if (!config.interfaceConfig.privateKey.isEmpty() && config.interfaceConfig.publicKey.isEmpty()) {
        qDebug() << "Warning: Configuration has private key but no public key - this should not happen";
        // Don't try to generate - this would create a wrong key!
        // Instead, user should regenerate the keypair if needed
    }
    
    // Enable/disable copy button based on public key availability
    m_copyPublicKeyButton->setEnabled(!m_publicKeyEdit->text().trimmed().isEmpty());
    
    // Final debug check
    QString finalPublicKey = m_publicKeyEdit->text().trimmed();
    qDebug() << "Final public key in field:" << finalPublicKey.left(16) + "..." << "Length:" << finalPublicKey.length();
    qDebug() << "Copy button enabled:" << m_copyPublicKeyButton->isEnabled();
    
    // Update peer configuration (use first peer if available)
    if (!config.interfaceConfig.peers.isEmpty()) {
        const WireGuardPeer& firstPeer = config.interfaceConfig.peers.first();
        m_peerPublicKeyEdit->setText(firstPeer.publicKey);
        m_peerEndpointEdit->setText(firstPeer.endpoint);
        m_peerAllowedIPsEdit->setText(firstPeer.allowedIPs.join(", "));
        m_peerPresharedKeyEdit->setText(firstPeer.presharedKey);
        m_peerKeepaliveSpin->setValue(firstPeer.persistentKeepalive);    } else {
        // Clear peer fields if no peers
        m_peerPublicKeyEdit->clear();
        m_peerEndpointEdit->clear();
        m_peerAllowedIPsEdit->setText("10.0.0.1/24"); // New default
        m_peerPresharedKeyEdit->clear();
        m_peerKeepaliveSpin->setValue(25);
    }
}

void WireGuardConfigDialog::generateKeypair()
{
    // Check if we already have keys and this is an existing configuration
    bool hasExistingKeys = !m_privateKeyEdit->text().isEmpty() && !m_publicKeyEdit->text().isEmpty();
    bool isExistingConfig = !m_config.interfaceConfig.name.isEmpty();
    
    if (hasExistingKeys && isExistingConfig) {        QMessageBox::StandardButton reply = QMessageBox::question(this, 
            "Visco Connect - Replace Existing Keys?",
            QString("This configuration '%1' already has a keypair.\n\n"
                   "Current Public Key: %2...\n\n"
                   "Generating new keys will make this configuration incompatible with servers "
                   "that have the current public key registered.\n\n"
                   "Are you sure you want to generate new keys?")
                   .arg(m_config.interfaceConfig.name)
                   .arg(m_publicKeyEdit->text().left(16)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        
        if (reply != QMessageBox::Yes) {
            return; // User cancelled
        }
    }
    
    WireGuardKeypair keypair = m_wireGuardManager->generateKeypair();
    if (keypair.isValid()) {
        m_privateKeyEdit->setText(keypair.privateKey);
        m_publicKeyEdit->setText(keypair.publicKey);
          // Update config with new keys
        m_config.interfaceConfig.privateKey = keypair.privateKey;
        m_config.interfaceConfig.publicKey = keypair.publicKey;
        
        // Enable copy button now that we have a public key
        m_copyPublicKeyButton->setEnabled(true);
        
        // Only auto-save if this is a configuration that already has a name and is being edited
        // For new configurations, we'll save when the user clicks OK
        bool shouldAutoSave = !m_config.interfaceConfig.name.isEmpty() && 
                             !m_config.configFilePath.isEmpty();
        
        QString messageText;
        if (shouldAutoSave) {
            // This is an existing configuration being edited - update other fields and save
            QString configName = m_config.interfaceConfig.name;
            m_config.interfaceConfig.addresses = m_addressEdit->text().split(',');
            m_config.interfaceConfig.dns = m_dnsEdit->text().split(',');
            m_config.interfaceConfig.listenPort = m_listenPortSpin->value();
            m_config.interfaceConfig.mtu = m_mtuSpin->value();
            
            // Clean up addresses and DNS
            for (QString& addr : m_config.interfaceConfig.addresses) {
                addr = addr.trimmed();
            }
            for (QString& dns : m_config.interfaceConfig.dns) {
                dns = dns.trimmed();
            }
            
            // Save to ensure persistence
            m_wireGuardManager->saveConfig(m_config);
            
            messageText = QString("New WireGuard keypair has been generated and saved!\n\n"
                           "Public Key: %1...\n"
                           "Private Key: %2...\n\n"
                           "IMPORTANT: You must provide this new public key to your VPN server "
                           "administrator to update their configuration, otherwise connections will fail.")
                           .arg(keypair.publicKey.left(16))
                           .arg(keypair.privateKey.left(16));
        } else {
            // This is a new configuration - keys generated but not saved yet
            messageText = QString("New WireGuard keypair has been generated!\n\n"
                           "Public Key: %1...\n"
                           "Private Key: %2...\n\n"
                           "Remember to provide the public key to your VPN server administrator.\n"
                           "Keys will be saved when you save the configuration.")
                           .arg(keypair.publicKey.left(16))
                           .arg(keypair.privateKey.left(16));
        }
        
        QMessageBox::information(this, "Visco Connect - Keys Generated", messageText);
    } else {
        QMessageBox::warning(this, "Visco Connect - Key Generation Failed", 
            "Failed to generate WireGuard keypair. Please check that:\n"
            "1. tunnel.dll and wireguard.dll are in the application directory\n"
            "2. The application is running with appropriate permissions\n\n"            "Check the application logs for more details.");
    }
}

void WireGuardConfigDialog::copyPublicKeyToClipboard()
{
    QString publicKey = m_publicKeyEdit->text().trimmed();
    
    if (publicKey.isEmpty()) {
        QMessageBox::information(this, "Visco Connect - No Public Key", 
            "No public key available to copy. Please generate a keypair first.");
        return;
    }
    
    // Copy to clipboard
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(publicKey);
    
    // Show confirmation with truncated key for security
    QString truncatedKey = publicKey.length() > 16 ? 
        publicKey.left(16) + "..." : publicKey;
    
    QMessageBox::information(this, "Visco Connect - Public Key Copied", 
        QString("Public key copied to clipboard!\n\n"
                "Key: %1\n\n"
                "You can now paste this key into your VPN server configuration "
                "or share it with your VPN administrator.")
                .arg(truncatedKey));
}

void WireGuardConfigDialog::loadFromFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Load WireGuard Configuration",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "WireGuard Config (*.conf);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        WireGuardConfig config = m_wireGuardManager->parseConfigFile(fileName);
        if (!config.interfaceConfig.name.isEmpty()) {
            setConfiguration(config);
            QMessageBox::information(this, "Visco Connect - Configuration Loaded", 
                QString("Configuration loaded from %1").arg(fileName));
        } else {
            QMessageBox::warning(this, "Visco Connect - Load Error", 
                "Failed to load configuration from the selected file.");
        }
    }
}

void WireGuardConfigDialog::saveToFile()
{
    // Update config from UI first
    m_config.interfaceConfig.name = m_nameEdit->text();
    m_config.interfaceConfig.privateKey = m_privateKeyEdit->text();
    m_config.interfaceConfig.publicKey = m_publicKeyEdit->text();
    m_config.interfaceConfig.addresses = m_addressEdit->text().split(',');
    m_config.interfaceConfig.dns = m_dnsEdit->text().split(',');
    m_config.interfaceConfig.listenPort = m_listenPortSpin->value();
    m_config.interfaceConfig.mtu = m_mtuSpin->value();
    
    // Update peer configuration
    WireGuardPeer peer;
    peer.publicKey = m_peerPublicKeyEdit->text().trimmed();
    peer.endpoint = m_peerEndpointEdit->text().trimmed();
    peer.allowedIPs = m_peerAllowedIPsEdit->text().split(',');
    peer.presharedKey = m_peerPresharedKeyEdit->text().trimmed();
    peer.persistentKeepalive = m_peerKeepaliveSpin->value();
    
    // Clean up peer allowed IPs
    for (QString& ip : peer.allowedIPs) {
        ip = ip.trimmed();
    }
    peer.allowedIPs.removeAll("");
    
    // Clear existing peers and add the new one if valid
    m_config.interfaceConfig.peers.clear();
    if (!peer.publicKey.isEmpty()) {
        m_config.interfaceConfig.peers.append(peer);
    }
    
    // Clean up addresses and DNS
    for (QString& addr : m_config.interfaceConfig.addresses) {
        addr = addr.trimmed();
    }
    for (QString& dns : m_config.interfaceConfig.dns) {
        dns = dns.trimmed();
    }
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save WireGuard Configuration",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + m_nameEdit->text() + ".conf",
        "WireGuard Config (*.conf);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        QString configStr = m_wireGuardManager->configToString(m_config);
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << configStr;
            QMessageBox::information(this, "Visco Connect - Configuration Saved", 
                QString("Configuration saved to %1").arg(fileName));        } else {
            QMessageBox::warning(this, "Visco Connect - Save Error", 
                "Failed to save configuration to the selected file.");
        }
    }
}

void WireGuardConfigDialog::validateAndAccept()
{
    // Update configuration from UI
    m_config.interfaceConfig.name = m_nameEdit->text().trimmed();
    m_config.interfaceConfig.privateKey = m_privateKeyEdit->text().trimmed();
    m_config.interfaceConfig.publicKey = m_publicKeyEdit->text().trimmed();
    m_config.interfaceConfig.addresses = m_addressEdit->text().split(',');
    m_config.interfaceConfig.dns = m_dnsEdit->text().split(',');
    m_config.interfaceConfig.listenPort = m_listenPortSpin->value();
    m_config.interfaceConfig.mtu = m_mtuSpin->value();
    
    // Clean up addresses and DNS
    for (QString& addr : m_config.interfaceConfig.addresses) {
        addr = addr.trimmed();
    }
    m_config.interfaceConfig.addresses.removeAll("");
    
    for (QString& dns : m_config.interfaceConfig.dns) {
        dns = dns.trimmed();
    }
    m_config.interfaceConfig.dns.removeAll("");
    
    // Create/update single peer from UI
    WireGuardPeer peer;
    peer.publicKey = m_peerPublicKeyEdit->text().trimmed();
    peer.endpoint = m_peerEndpointEdit->text().trimmed();
    peer.allowedIPs = m_peerAllowedIPsEdit->text().split(',');
    peer.presharedKey = m_peerPresharedKeyEdit->text().trimmed();
    peer.persistentKeepalive = m_peerKeepaliveSpin->value();
    
    // Clean up allowed IPs
    for (QString& ip : peer.allowedIPs) {
        ip = ip.trimmed();
    }
    peer.allowedIPs.removeAll("");
    
    // Clear existing peers and add the new one
    m_config.interfaceConfig.peers.clear();
    if (!peer.publicKey.isEmpty()) {
        m_config.interfaceConfig.peers.append(peer);
    }
    
    // Set timestamps
    m_config.createdAt = QDateTime::currentDateTime();
    m_config.lastConnectedAt = QDateTime();
    
    if (validateConfiguration()) {
        // Auto-save the configuration
        if (m_wireGuardManager->saveConfig(m_config)) {
            accept();
        } else {            QMessageBox::warning(this, "Visco Connect - Save Error", 
                "Configuration is valid but failed to save. Please check permissions and try again.");
        }
    }
}

void WireGuardConfigDialog::resetToDefaults()
{
    m_config = WireGuardConfig();
    m_config.interfaceConfig.name = "wg0";
    m_config.interfaceConfig.addresses = QStringList() << "10.0.0.2/24";
    m_config.interfaceConfig.dns = QStringList(); // Empty DNS servers by default
    
    // Create default peer with PersistentKeepalive enabled
    WireGuardPeer defaultPeer;
    defaultPeer.persistentKeepalive = 25;  // Enabled by default to keep NAT hole open
    m_config.interfaceConfig.peers.clear();
    m_config.interfaceConfig.peers.append(defaultPeer);
    
    setConfiguration(m_config);
}

bool WireGuardConfigDialog::validateConfiguration()
{
    // Validate configuration name
    if (m_config.interfaceConfig.name.isEmpty()) {
        QMessageBox::warning(this, "Visco Connect - Invalid Configuration", "Configuration name cannot be empty.");
        return false;
    }
    
    // Validate private key
    if (m_config.interfaceConfig.privateKey.isEmpty()) {
        QMessageBox::warning(this, "Visco Connect - Invalid Configuration", "Private key is required.");
        return false;
    }
    
    // Validate addresses
    if (m_config.interfaceConfig.addresses.isEmpty()) {
        QMessageBox::warning(this, "Visco Connect - Invalid Configuration", "At least one IP address is required.");
        return false;
    }
    
    // Validate peer configuration if provided
    if (!m_config.interfaceConfig.peers.isEmpty()) {
        const WireGuardPeer& peer = m_config.interfaceConfig.peers.first();
        if (peer.publicKey.isEmpty()) {
            QMessageBox::warning(this, "Visco Connect - Invalid Configuration", "Peer public key is required when peer is configured.");
            return false;
        }
        
        if (peer.endpoint.isEmpty()) {
            QMessageBox::warning(this, "Visco Connect - Invalid Configuration", "Peer endpoint is required when peer is configured.");
            return false;
        }
        
        if (peer.allowedIPs.isEmpty()) {
            QMessageBox::warning(this, "Visco Connect - Invalid Configuration", "Peer allowed IPs are required when peer is configured.");
            return false;
        }
    }
    
    return true;
}

void WireGuardConfigDialog::setupUI()
{
    setModal(true);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Header with title
    QLabel* titleLabel = new QLabel("WireGuard Configuration");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; margin: 10px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // Create scroll area for the form
    QScrollArea* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
      QWidget* formWidget = new QWidget;
    QVBoxLayout* formLayout = new QVBoxLayout(formWidget);
    formLayout->setSpacing(15); // Add more spacing between groups
    formLayout->setContentsMargins(10, 10, 10, 10);
      // Basic Configuration Group
    QGroupBox* basicGroup = new QGroupBox("Basic Configuration");
    QFormLayout* basicForm = new QFormLayout(basicGroup);
    basicForm->setVerticalSpacing(10);
    basicForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("e.g., HomeVPN, OfficeVPN");
    basicForm->addRow("Configuration Name*:", m_nameEdit);
      // Key Management Group
    QGroupBox* keyGroup = new QGroupBox("Keys");
    QFormLayout* keyForm = new QFormLayout(keyGroup);
    keyForm->setVerticalSpacing(10);
    keyForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
      QHBoxLayout* privateKeyLayout = new QHBoxLayout;
    m_privateKeyEdit = new QLineEdit;
    m_privateKeyEdit->setPlaceholderText("Private key (will be generated automatically)");
    m_generateKeyButton = new QPushButton("Generate New Keypair");
    m_generateKeyButton->setMaximumWidth(150);
    m_generateKeyButton->setToolTip("Generate a new public/private keypair. Warning: This will replace existing keys!");
    privateKeyLayout->addWidget(m_privateKeyEdit);    privateKeyLayout->addWidget(m_generateKeyButton);
    keyForm->addRow("Private Key*:", privateKeyLayout);
      // Public Key with Copy Button
    QHBoxLayout* publicKeyLayout = new QHBoxLayout;
    m_publicKeyEdit = new QLineEdit;
    m_publicKeyEdit->setReadOnly(true);
    m_publicKeyEdit->setPlaceholderText("Your public key will appear here - share this with your VPN server administrator");
    m_publicKeyEdit->setStyleSheet("background-color: #f0f0f0;");
    m_copyPublicKeyButton = new QPushButton("Copy");
    m_copyPublicKeyButton->setMaximumWidth(80);
    m_copyPublicKeyButton->setToolTip("Copy public key to clipboard for sharing with VPN server administrator");
    m_copyPublicKeyButton->setEnabled(false); // Initially disabled until key is available
    publicKeyLayout->addWidget(m_publicKeyEdit);
    publicKeyLayout->addWidget(m_copyPublicKeyButton);
    keyForm->addRow("Public Key:", publicKeyLayout);
      // Network Configuration Group
    QGroupBox* networkGroup = new QGroupBox("Network Settings");
    QFormLayout* networkForm = new QFormLayout(networkGroup);
    networkForm->setVerticalSpacing(10);
    networkForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    m_addressEdit = new QLineEdit;
    m_addressEdit->setPlaceholderText("e.g., 10.0.0.2/24");
    networkForm->addRow("IP Address*:", m_addressEdit);
      m_dnsEdit = new QLineEdit;
    m_dnsEdit->setPlaceholderText("e.g., 8.8.8.8, 8.8.4.4 (optional)");
    m_dnsEdit->setText(""); // Empty by default
    networkForm->addRow("DNS Servers:", m_dnsEdit);
    
    m_listenPortSpin = new QSpinBox;
    m_listenPortSpin->setRange(0, 65535);
    m_listenPortSpin->setSpecialValueText("Auto");
    m_listenPortSpin->setValue(0); // Auto by default
    networkForm->addRow("Listen Port:", m_listenPortSpin);
    
    m_mtuSpin = new QSpinBox;
    m_mtuSpin->setRange(1280, 65535);
    m_mtuSpin->setValue(1280);  // Default to 1280 to prevent video packet fragmentation
    m_mtuSpin->setToolTip("MTU size in bytes (default: 1280 for video over VPN). Smaller values prevent packet fragmentation.");
    networkForm->addRow("MTU Size:", m_mtuSpin);
    
    // Simple Peer Configuration Group
    QGroupBox* peerGroup = new QGroupBox("Peer Configuration");
    QFormLayout* peerForm = new QFormLayout(peerGroup);
    peerForm->setVerticalSpacing(10);
    peerForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    m_peerPublicKeyEdit = new QLineEdit;
    m_peerPublicKeyEdit->setPlaceholderText("Server's public key");
    peerForm->addRow("Peer Public Key*:", m_peerPublicKeyEdit);
    
    m_peerEndpointEdit = new QLineEdit;
    m_peerEndpointEdit->setPlaceholderText("e.g., vpn.example.com:51820");
    peerForm->addRow("Server Endpoint*:", m_peerEndpointEdit);
      m_peerAllowedIPsEdit = new QLineEdit;
    m_peerAllowedIPsEdit->setPlaceholderText("e.g., 10.0.0.1/24 or 0.0.0.0/0 (route all traffic)");
    m_peerAllowedIPsEdit->setText("10.0.0.1/24"); // New default
    peerForm->addRow("Allowed IPs:", m_peerAllowedIPsEdit);
    
    m_peerPresharedKeyEdit = new QLineEdit;
    m_peerPresharedKeyEdit->setPlaceholderText("Optional preshared key for extra security");
    peerForm->addRow("Preshared Key:", m_peerPresharedKeyEdit);
    
    m_peerKeepaliveSpin = new QSpinBox;
    m_peerKeepaliveSpin->setRange(0, 65535);
    m_peerKeepaliveSpin->setSpecialValueText("Disabled");
    m_peerKeepaliveSpin->setSuffix(" seconds");
    m_peerKeepaliveSpin->setValue(25); // Set reasonable default
    m_peerKeepaliveSpin->setToolTip("Keepalive interval to prevent NAT timeout (default: 25 seconds). Sends heartbeat packets every N seconds to keep the tunnel alive through routers. Set to 0 to disable.");
    peerForm->addRow("Keep Alive:", m_peerKeepaliveSpin);
    
    // Add all groups to form layout
    formLayout->addWidget(basicGroup);
    formLayout->addWidget(keyGroup);
    formLayout->addWidget(networkGroup);
    formLayout->addWidget(peerGroup);
    formLayout->addStretch();
    
    scrollArea->setWidget(formWidget);
    mainLayout->addWidget(scrollArea);
    
    // Dialog buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    m_loadFileButton = new QPushButton("Load Config File...");
    m_saveFileButton = new QPushButton("Save Config File...");
    m_resetButton = new QPushButton("Reset to Defaults");
    
    buttonLayout->addWidget(m_loadFileButton);
    buttonLayout->addWidget(m_saveFileButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch();
    
    m_okButton = new QPushButton("OK");
    m_cancelButton = new QPushButton("Cancel");
    m_okButton->setDefault(true);
    
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
      mainLayout->addLayout(buttonLayout);
    
    // Only generate keys automatically for NEW configurations (not when editing existing ones)
    // We can determine this by checking if we have a valid configuration passed in the constructor
    bool isNewConfiguration = m_config.interfaceConfig.name.isEmpty() && 
                              m_config.interfaceConfig.privateKey.isEmpty() && 
                              m_config.interfaceConfig.publicKey.isEmpty();
    
    if (isNewConfiguration && m_privateKeyEdit->text().isEmpty()) {
        QTimer::singleShot(100, this, &WireGuardConfigDialog::generateKeypair);
    }
}

void WireGuardConfigDialog::connectSignals()
{    // Key generation
    connect(m_generateKeyButton, &QPushButton::clicked, this, &WireGuardConfigDialog::generateKeypair);
    connect(m_copyPublicKeyButton, &QPushButton::clicked, this, &WireGuardConfigDialog::copyPublicKeyToClipboard);
    
    // Enable/disable copy button based on public key availability
    connect(m_publicKeyEdit, &QLineEdit::textChanged, [this](const QString& text) {
        m_copyPublicKeyButton->setEnabled(!text.trimmed().isEmpty());
    });
    
    // Dialog buttons
    connect(m_okButton, &QPushButton::clicked, this, &WireGuardConfigDialog::validateAndAccept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_resetButton, &QPushButton::clicked, this, &WireGuardConfigDialog::resetToDefaults);
    connect(m_loadFileButton, &QPushButton::clicked, this, &WireGuardConfigDialog::loadFromFile);
    connect(m_saveFileButton, &QPushButton::clicked, this, &WireGuardConfigDialog::saveToFile);
}



