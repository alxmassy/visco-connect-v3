#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>
#include <QNetworkReply>        // for enum in slot

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QNetworkAccessManager;
QT_END_NAMESPACE

class AuthDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AuthDialog(QWidget *parent = nullptr);
    ~AuthDialog();

    static QString getCurrentAuthToken();
    static int getUserId();
    static QString getBearerToken();
    static void clearCurrentAuthToken();

signals:
    void loginSuccessful();

private slots:
    void onLoginClicked();
    void onNetworkFinished();
    void onNetworkError(QNetworkReply::NetworkError err);

private:
    void updateButtonState();
    void performAuthentication(const QString &user, const QString &pass);
    void showStatus(const QString &text, const QColor &col);

    // widgets
    QLabel     *m_logoLbl;
    QLabel     *m_titleLbl;
    QLabel     *m_subLbl;
    QLabel     *m_statusLbl;
    QLineEdit  *m_userEdit;
    QLineEdit  *m_passEdit;
    QPushButton *m_loginBtn;

    // networking
    QNetworkAccessManager *m_netMgr;
    QNetworkReply         *m_reply{};
};
#endif
