#include "UserProfileWidget.h"
#include "ConfigManager.h"

#include <QApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcess>

#include "AuthDialog.h"
#include "MainWindow.h"
#include "Logger.h"

UserProfileWidget::UserProfileWidget(QWidget *parent)
    : QWidget(parent)
    , m_profileGroup(nullptr)
    , m_fullNameLabel(nullptr)
    , m_emailLabel(nullptr)
    , m_logoutButton(nullptr)
    , m_avatarLabel(nullptr)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_profileReply(nullptr)
    , m_logoutReply(nullptr)
{
    setupUI();
    connectSignals();
    
    // Fetch user profile on initialization
    fetchUserProfile();
}

UserProfileWidget::~UserProfileWidget()
{
    if (m_profileReply) {
        m_profileReply->abort();
        m_profileReply->deleteLater();
    }
    if (m_logoutReply) {
        m_logoutReply->abort();
        m_logoutReply->deleteLater();
    }
}

void UserProfileWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);    // Create modern profile card with gradient background
    m_profileGroup = new QGroupBox();    m_profileGroup->setStyleSheet(
        "QGroupBox {"
        "    border: none;"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "        stop:0 #f8f9fa, stop:1 #e9ecef);"
        "    border-radius: 6px;"
        "    margin: 2px;"
        "    padding: 0px;"
        "}"
    );

    // Main card container
    QWidget *profileCard = new QWidget();
    profileCard->setStyleSheet(
        "QWidget {"
        "    background-color: transparent;"
        "    border: none;"
        "}"
    );    QVBoxLayout *cardLayout = new QVBoxLayout(profileCard);
    cardLayout->setSpacing(6);
    cardLayout->setContentsMargins(8, 8, 8, 8);    // Header section with avatar and main info
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);// Modern circular avatar (compact)
    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(32, 32);
    m_avatarLabel->setStyleSheet(
        "QLabel {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "        stop:0 #4CAF50, stop:1 #45a049);"
        "    border-radius: 16px;"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    border: 2px solid white;"
        "    box-shadow: 0 1px 4px rgba(0,0,0,0.1);"
        "}"
    );
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setText("U");    // User info container
    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);
    infoLayout->setContentsMargins(0, 0, 0, 0);

    // Name section with label
    QVBoxLayout *nameSection = new QVBoxLayout();
    nameSection->setSpacing(1);
      QLabel *nameLabel = new QLabel(tr("NAME"));
    nameLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 8px;"
        "    font-weight: 600;"
        "    color: #6c757d;"
        "    text-transform: uppercase;"
        "    letter-spacing: 0.3px;"
        "}"
    );

    m_fullNameLabel = new QLabel(tr("Loading..."));
    m_fullNameLabel->setStyleSheet(
        "QLabel {"
        "    font-weight: 600;"
        "    font-size: 12px;"
        "    color: #212529;"
        "    margin: 0px;"
        "}"
    );

    nameSection->addWidget(nameLabel);
    nameSection->addWidget(m_fullNameLabel);

    // Email section with label
    QVBoxLayout *emailSection = new QVBoxLayout();
    emailSection->setSpacing(1);
      QLabel *emailLabel = new QLabel(tr("EMAIL"));
    emailLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 8px;"
        "    font-weight: 600;"
        "    color: #6c757d;"
        "    text-transform: uppercase;"
        "    letter-spacing: 0.3px;"
        "}"
    );

    m_emailLabel = new QLabel(tr("Loading..."));
    m_emailLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 10px;"
        "    color: #495057;"
        "    margin: 0px;"
        "}"
    );

    emailSection->addWidget(emailLabel);
    emailSection->addWidget(m_emailLabel);

    infoLayout->addLayout(nameSection);
    infoLayout->addLayout(emailSection);
    infoLayout->addStretch();

    headerLayout->addWidget(m_avatarLabel);
    headerLayout->addLayout(infoLayout, 1);    // Action section with modern logout button
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setContentsMargins(0, 4, 0, 0);

    // Add some spacing on the left
    actionLayout->addStretch();

    m_logoutButton = new QPushButton(tr("Logout"));
    m_logoutButton->setMinimumSize(80, 28);
    m_logoutButton->setMaximumHeight(28);
    m_logoutButton->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "        stop:0 #e74c3c, stop:1 #c0392b);"
        "    color: white;"
        "    font-weight: 600;"
        "    font-size: 11px;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 6px 16px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "        stop:0 #ec7063, stop:1 #e74c3c);"
        "    transform: translateY(-1px);"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "        stop:0 #c0392b, stop:1 #a93226);"
        "    transform: translateY(0px);"
        "}"
    );

    actionLayout->addWidget(m_logoutButton);

    // Assemble the card
    cardLayout->addLayout(headerLayout);
    
    // Add a subtle separator line
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(
        "QFrame {"
        "    background-color: #dee2e6;"
        "    border: none;"
        "    height: 1px;"
        "    margin: 0px 0px;"
        "}"
    );
    cardLayout->addWidget(separator);
    
    cardLayout->addLayout(actionLayout);

    // Add card to group
    QVBoxLayout *groupLayout = new QVBoxLayout(m_profileGroup);
    groupLayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->addWidget(profileCard);

    mainLayout->addWidget(m_profileGroup);
}

void UserProfileWidget::connectSignals()
{
    connect(m_logoutButton, &QPushButton::clicked, this, &UserProfileWidget::onLogoutClicked);
}

void UserProfileWidget::fetchUserProfile()
{
    if (m_profileReply) {
        m_profileReply->abort();
        m_profileReply->deleteLater();
    }

    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        updateProfileDisplay(tr("Not authenticated"), tr("Please login"));
        return;
    }

    showLoadingState();

    QString apiBaseUrl = ConfigManager::instance().getApiBaseUrl();
    QNetworkRequest request(QUrl(apiBaseUrl + "/me/profile"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    m_profileReply = m_networkManager->get(request);

    connect(m_profileReply, &QNetworkReply::finished, this, &UserProfileWidget::onProfileFetchFinished);
    connect(m_profileReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &UserProfileWidget::onProfileFetchError);

    LOG_INFO("Fetching user profile from server", "UserProfileWidget");
}

void UserProfileWidget::onProfileFetchFinished()
{
    if (!m_profileReply) return;

    int statusCode = m_profileReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = m_profileReply->readAll();
    m_profileReply->deleteLater();
    m_profileReply = nullptr;

    if (statusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        // Parse the new response structure
        QJsonObject userObj = obj.value("user").toObject();
        QString organizationName = obj.value("organization_name").toString();
        
        QString name = userObj.value("name").toString();
        QString email = userObj.value("email").toString();
        QString role = userObj.value("role").toString();
        
        // Use name as full name, fallback to email if name is empty
        QString fullName;
        if (!name.isEmpty()) {
            fullName = name;
        } else if (!email.isEmpty()) {
            fullName = email.split('@').first(); // Use email username part
        } else {
            fullName = tr("Unknown User");
        }

        updateProfileDisplay(fullName, email);
        
        // Update avatar with first letter of name
        if (!fullName.isEmpty()) {
            m_avatarLabel->setText(fullName.at(0).toUpper());
        }

        LOG_INFO(QString("User profile loaded: %1 (%2) - Organization: %3, Role: %4")
                 .arg(fullName, email, organizationName, role), "UserProfileWidget");

    } else if (statusCode == 401) {
        updateProfileDisplay(tr("Authentication Failed"), tr("Please login again"));
        LOG_WARNING("User profile fetch failed: Authentication token invalid", "UserProfileWidget");
    } else {
        updateProfileDisplay(tr("Profile Error"), tr("Failed to load profile"));
        LOG_WARNING(QString("User profile fetch failed with status code: %1").arg(statusCode), "UserProfileWidget");
    }
}

void UserProfileWidget::onProfileFetchError(QNetworkReply::NetworkError error)
{
    if (!m_profileReply) return;

    QString errorString = m_profileReply->errorString();
    m_profileReply->deleteLater();
    m_profileReply = nullptr;

    updateProfileDisplay(tr("Network Error"), tr("Unable to load profile"));
    LOG_ERROR(QString("User profile fetch network error: %1").arg(errorString), "UserProfileWidget");
}

void UserProfileWidget::updateProfileDisplay(const QString &fullName, const QString &email)
{
    m_currentFullName = fullName;
    m_currentEmail = email;

    m_fullNameLabel->setText(fullName);
    m_emailLabel->setText(email);
    
    // Generate initials for avatar
    QString initials = generateInitials(fullName);
    m_avatarLabel->setText(initials);
    
    // Update avatar color based on name (for variety)
    QString avatarColor = generateAvatarColor(fullName);
    m_avatarLabel->setStyleSheet(
        QString("QLabel {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "        stop:0 %1, stop:1 %2);"
        "    border-radius: 16px;"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    border: 2px solid white;"
        "    box-shadow: 0 1px 4px rgba(0,0,0,0.1);"
        "}").arg(avatarColor).arg(darkenColor(avatarColor))
    );
}

QString UserProfileWidget::generateInitials(const QString &fullName)
{
    if (fullName.isEmpty() || fullName == tr("Loading...") || fullName == tr("Not authenticated")) {
        return "U";
    }
    
    QStringList nameParts = fullName.split(' ', Qt::SkipEmptyParts);    QString initials;
    
    if (nameParts.size() >= 2) {
        // First and last name initials
        initials = QString(nameParts.first().at(0).toUpper()) + QString(nameParts.last().at(0).toUpper());
    } else if (nameParts.size() == 1) {
        // Just first letter if single name
        initials = nameParts.first().left(2).toUpper();
    } else {
        initials = "U";
    }
    
    return initials;
}

QString UserProfileWidget::generateAvatarColor(const QString &fullName)
{
    // Generate a consistent color based on the name
    if (fullName.isEmpty() || fullName == tr("Loading...") || fullName == tr("Not authenticated")) {
        return "#4CAF50"; // Default green
    }
    
    // Simple hash-based color generation
    uint hash = qHash(fullName);
    
    QStringList colors = {
        "#4CAF50", // Green
        "#2196F3", // Blue  
        "#FF9800", // Orange
        "#9C27B0", // Purple
        "#F44336", // Red
        "#009688", // Teal
        "#3F51B5", // Indigo
        "#E91E63", // Pink
        "#795548", // Brown
        "#607D8B"  // Blue Grey
    };
    
    return colors[hash % colors.size()];
}

QString UserProfileWidget::darkenColor(const QString &color)
{
    // Simple way to darken a hex color for gradient effect
    QColor baseColor(color);
    return baseColor.darker(120).name();
}

void UserProfileWidget::showLoadingState()
{
    m_fullNameLabel->setText(tr("Loading..."));
    m_emailLabel->setText(tr("Fetching profile..."));
    m_avatarLabel->setText("...");
}

void UserProfileWidget::onLogoutClicked()
{
    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        tr("Visco Connect - Logout Confirmation"),
        tr("Are you sure you want to logout? The application will close and you will need to authenticate again when you restart it."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        LOG_INFO("User confirmed logout, initiating logout process", "UserProfileWidget");
        
        // Disable logout button to prevent multiple clicks
        m_logoutButton->setEnabled(false);
        m_logoutButton->setText(tr("Logging out..."));
        
        // Call logout API first
        performLogoutApiCall();
    }
}

void UserProfileWidget::performLogoutApiCall()
{
    if (m_logoutReply) {
        m_logoutReply->abort();
        m_logoutReply->deleteLater();
    }

    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) {
        LOG_WARNING("No authentication token found, proceeding with local logout only", "UserProfileWidget");
        completeLogoutProcess();
        return;
    }

    QString apiBaseUrl = ConfigManager::instance().getApiBaseUrl();
    QNetworkRequest request(QUrl(apiBaseUrl + "/logout"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());

    LOG_INFO("Calling logout API to revoke server-side session and WireGuard IP", "UserProfileWidget");
    
    m_logoutReply = m_networkManager->post(request, QByteArray());

    connect(m_logoutReply, &QNetworkReply::finished, this, &UserProfileWidget::onLogoutFinished);
    connect(m_logoutReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &UserProfileWidget::onLogoutError);
}

void UserProfileWidget::onLogoutFinished()
{
    if (!m_logoutReply) return;

    int statusCode = m_logoutReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = m_logoutReply->readAll();
    m_logoutReply->deleteLater();
    m_logoutReply = nullptr;

    if (statusCode == 200) {
        LOG_INFO("Logout API call successful - server session and WireGuard IP revoked", "UserProfileWidget");
        
        // Parse response to check WireGuard status
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QString wireguardStatus = obj.value("wireguard_status").toString();
        if (!wireguardStatus.isEmpty()) {
            LOG_INFO(QString("WireGuard status: %1").arg(wireguardStatus), "UserProfileWidget");
        }
    } else {
        LOG_WARNING(QString("Logout API call failed with status code: %1").arg(statusCode), "UserProfileWidget");
        // Continue with logout process even if API call failed
    }
    
    // Complete the logout process
    completeLogoutProcess();
}

void UserProfileWidget::onLogoutError(QNetworkReply::NetworkError error)
{
    if (!m_logoutReply) return;
    
    QString errorString = m_logoutReply->errorString();
    m_logoutReply->deleteLater();
    m_logoutReply = nullptr;
    
    LOG_WARNING(QString("Logout API call error: %1").arg(errorString), "UserProfileWidget");
    
    // Continue with logout process even if API call failed
    completeLogoutProcess();
}

void UserProfileWidget::completeLogoutProcess()
{
    LOG_INFO("Completing logout process - clearing local data and restarting application", "UserProfileWidget");
    
    // Find MainWindow and disconnect VPN if needed
    QWidget *parentWidget = this->parentWidget();
    while (parentWidget) {
        if (MainWindow *mainWindow = qobject_cast<MainWindow*>(parentWidget)) {
            LOG_INFO("Disconnecting VPN and cleaning up configuration for logout", "UserProfileWidget");
            mainWindow->disconnectVpnOnLogout();
            break;
        }
        parentWidget = parentWidget->parentWidget();
    }

    // Clear the authentication token
    AuthDialog::clearCurrentAuthToken();
    
    // Switch ConfigManager to no user (clears current user's cameras from memory)
    ConfigManager::instance().switchToUser("");
    LOG_INFO("Cleared user-specific configuration from memory", "UserProfileWidget");

    LOG_INFO("User logged out successfully, restarting application for re-authentication", "UserProfileWidget");

    // Get the current application executable path and arguments
    QString program = QApplication::applicationFilePath();
    QStringList arguments = QApplication::arguments();
    arguments.removeFirst(); // Remove the executable name from arguments

    // Find the main window and set force quit flag before closing
    MainWindow* mainWindow = qobject_cast<MainWindow*>(window());
    if (mainWindow) {
        mainWindow->setForceQuit(true);
        mainWindow->close();
    }

    // Restart the application to show login dialog
    QProcess::startDetached(program, arguments);
    
    // Exit current instance
    QApplication::quit();
}
