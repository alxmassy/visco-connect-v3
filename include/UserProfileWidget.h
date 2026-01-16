#ifndef USERPROFILEWIDGET_H
#define USERPROFILEWIDGET_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
class QGroupBox;
class QLabel;
class QPushButton;
class QHBoxLayout;
class QVBoxLayout;
QT_END_NAMESPACE

class UserProfileWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UserProfileWidget(QWidget *parent = nullptr);
    ~UserProfileWidget();

private slots:
    void onLogoutClicked();
    void onProfileFetchFinished();
    void onProfileFetchError(QNetworkReply::NetworkError error);
    void onLogoutFinished();
    void onLogoutError(QNetworkReply::NetworkError error);

private:
    void setupUI();
    void fetchUserProfile();
    void updateProfileDisplay(const QString &fullName, const QString &email);
    void showLoadingState();
    void connectSignals();
    void performLogoutApiCall();
    void completeLogoutProcess();
    
    // Helper methods for avatar generation
    QString generateInitials(const QString &fullName);
    QString generateAvatarColor(const QString &fullName);
    QString darkenColor(const QString &color);

    // UI Components
    QGroupBox *m_profileGroup;
    QLabel *m_fullNameLabel;
    QLabel *m_emailLabel;
    QPushButton *m_logoutButton;
    QLabel *m_avatarLabel;

    // Network
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_profileReply;
    QNetworkReply *m_logoutReply;

    // User data
    QString m_currentFullName;
    QString m_currentEmail;
};

#endif // USERPROFILEWIDGET_H
