#include "AuthDialog.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <QtWidgets>
#include <QtNetwork>
#include <QSettings>
#include <QUrlQuery>

AuthDialog::AuthDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Visco Connect - Authentication");
    setFixedSize(380, 300);
    setModal(true);

    /* ----------- Branding section ----------- */
    m_logoLbl  = new QLabel(this);
    QPixmap logo(":/icons/logo.png");          // add your logo to resources
    if (!logo.isNull())
        m_logoLbl->setPixmap(logo.scaled(48,48,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    else
        m_logoLbl->setText("🔐");              // fallback

    m_titleLbl = new QLabel("Visco Connect", this);
    m_titleLbl->setStyleSheet("font-size:18px; font-weight:600;");

    m_subLbl   = new QLabel("Secure Authentication Portal", this);
    m_subLbl->setStyleSheet("color:gray;");

    QVBoxLayout *brandLay = new QVBoxLayout;
    brandLay->addWidget(m_logoLbl, 0, Qt::AlignHCenter);
    brandLay->addWidget(m_titleLbl,0, Qt::AlignHCenter);
    brandLay->addWidget(m_subLbl, 0, Qt::AlignHCenter);

    /* ----------- Credentials section ----------- */
    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText("Email Address");

    m_passEdit = new QLineEdit(this);
    m_passEdit->setPlaceholderText("Password");
    m_passEdit->setEchoMode(QLineEdit::Password);

    m_statusLbl = new QLabel(this);
    m_statusLbl->setAlignment(Qt::AlignCenter);
    m_statusLbl->setWordWrap(true);

    m_loginBtn = new QPushButton("Sign In", this);
    m_loginBtn->setEnabled(false);

    /* ----------- Layout root ----------- */
    QVBoxLayout *root = new QVBoxLayout(this);
    root->addLayout(brandLay);
    root->addSpacing(10);
    root->addWidget(m_userEdit);
    root->addWidget(m_passEdit);
    root->addWidget(m_statusLbl);
    root->addWidget(m_loginBtn);

    /* ----------- Connections ----------- */
    connect(m_userEdit, &QLineEdit::textChanged, this, &AuthDialog::updateButtonState);
    connect(m_passEdit, &QLineEdit::textChanged, this, &AuthDialog::updateButtonState);
    connect(m_passEdit, &QLineEdit::returnPressed, this, &AuthDialog::onLoginClicked);
    connect(m_loginBtn,&QPushButton::clicked, this,&AuthDialog::onLoginClicked);

    m_netMgr = new QNetworkAccessManager(this);
    showStatus("Enter your credentials.", Qt::darkGray);
}

AuthDialog::~AuthDialog()
{
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); }
}

/* ---------- helpers ---------- */
void AuthDialog::updateButtonState()
{
    bool ok = !m_userEdit->text().trimmed().isEmpty() && !m_passEdit->text().isEmpty();
    m_loginBtn->setEnabled(ok);
}

void AuthDialog::showStatus(const QString &text,const QColor &col)
{
    m_statusLbl->setText(text);
    m_statusLbl->setStyleSheet(QString("color:%1;").arg(col.name()));
}

/* ---------- login ---------- */
void AuthDialog::onLoginClicked()
{
    if (!m_loginBtn->isEnabled()) return;
    performAuthentication(m_userEdit->text().trimmed(), m_passEdit->text());
}

void AuthDialog::performAuthentication(const QString &user,const QString &pass)
{
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); }

    showStatus("Authenticating…", Qt::darkGray);
    m_loginBtn->setEnabled(false);

    QString apiBaseUrl = ConfigManager::instance().getApiBaseUrl();
    QNetworkRequest req(QUrl(apiBaseUrl + "/login"));
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");

    // Prepare form-encoded data according to new API requirements
    QUrlQuery formData;
    formData.addQueryItem("username", user);      // email address
    formData.addQueryItem("password", pass);      // password
    formData.addQueryItem("grant_type", "password"); // must always be "password"
    formData.addQueryItem("client_id", "");       // empty value
    formData.addQueryItem("client_secret", "");   // empty value
    formData.addQueryItem("scope", "");           // empty value

    m_reply = m_netMgr->post(req, formData.toString(QUrl::FullyEncoded).toUtf8());

    connect(m_reply, &QNetworkReply::finished, this,&AuthDialog::onNetworkFinished);
    connect(m_reply, qOverload<QNetworkReply::NetworkError>(&QNetworkReply::errorOccurred),
            this,&AuthDialog::onNetworkError);
}

void AuthDialog::onNetworkFinished()
{
    if (!m_reply) return;
    int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray data = m_reply->readAll();
    m_reply->deleteLater();
    m_reply=nullptr;
    
    if (code==200) {
        QJsonDocument doc=QJsonDocument::fromJson(data);
        QJsonObject response = doc.object();
        
        // Extract access token from new response format
        QString token = response.value("access_token").toString();
        QString tokenType = response.value("token_type").toString(); // "bearer"
        
        if (!token.isEmpty()) {
            // Save the token to QSettings
            QSettings s("ViscoConnect","Auth");
            s.setValue("access_token", token);
            s.setValue("token_type", tokenType);
            
            // Extract user information if available
            QJsonObject user = response.value("user").toObject();
            if (!user.isEmpty()) {
                s.setValue("user_id", user.value("id").toInt());
                s.setValue("user_name", user.value("name").toString());
                s.setValue("user_email", user.value("email").toString());
                s.setValue("user_role", user.value("role").toString());
                s.setValue("account_created_date", user.value("account_created_date").toString());
            }
            
            // Save additional session info
            s.setValue("ip_address", response.value("ip_address").toString());
            s.setValue("last_login", response.value("last_login").toString());
            
            // Set expiration time (assume 1 hour if not provided by server)
            qint64 expiresAt = QDateTime::currentSecsSinceEpoch() + 3600; // 1 hour
            s.setValue("expires_at", expiresAt);
            
            showStatus("Login successful.", Qt::darkGreen);
            
            // Emit signal for successful login
            emit loginSuccessful();
            
            // Switch ConfigManager to this user's configuration
            QString userEmail = user.value("email").toString();
            if (!userEmail.isEmpty()) {
                ConfigManager::instance().switchToUser(userEmail);
                LOG_INFO(QString("Switched to user configuration for: %1").arg(userEmail), "AuthDialog");
            }
            
            QTimer::singleShot(700, this, &QDialog::accept);
            return;
        }
        showStatus("Unexpected response.", Qt::red);
    } else if (code==401) {
        showStatus("Invalid username or password.", Qt::red);
    } else {
        showStatus(QString("Server error (%1).").arg(code), Qt::red);
    }
    m_loginBtn->setEnabled(true);
    m_passEdit->clear(); m_passEdit->setFocus();
}

void AuthDialog::onNetworkError(QNetworkReply::NetworkError)
{
    if (!m_reply) return;
    QString err=m_reply->errorString();
    m_reply->deleteLater(); m_reply=nullptr;
    showStatus("Network error: "+err, Qt::red);
    m_loginBtn->setEnabled(true);
}

/* ---------- token helpers ---------- */
QString AuthDialog::getCurrentAuthToken()
{
    QSettings s("ViscoConnect","Auth");
    QString tok=s.value("access_token").toString();
    qint64 exp=s.value("expires_at").toLongLong();
    return (!tok.isEmpty() && QDateTime::currentSecsSinceEpoch()<exp)?tok:QString();
}

QString AuthDialog::getBearerToken()
{
    QString token = AuthDialog::getCurrentAuthToken();
    if (token.isEmpty()) return QString();
    
    QSettings s("ViscoConnect","Auth");
    QString tokenType = s.value("token_type", "bearer").toString();
    return QString("%1 %2").arg(tokenType).arg(token);
}

void AuthDialog::clearCurrentAuthToken(){ QSettings("ViscoConnect","Auth").clear(); }
