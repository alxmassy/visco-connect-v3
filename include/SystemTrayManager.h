#ifndef SYSTEMTRAYMANAGER_H
#define SYSTEMTRAYMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QStyle>

class MainWindow;
class VpnWidget;

class SystemTrayManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayManager(MainWindow* mainWindow, VpnWidget* vpnWidget, QObject *parent = nullptr);
    ~SystemTrayManager();
      void initialize();
    void show();
    void hide();      bool isVisible() const;
    void updateVpnStatus();
    void showNotification(const QString& title, const QString& message, QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);
    void notifyVpnStatusChange(const QString& status, bool connected);

signals:
    void showMainWindow();
    void joinNetwork();
    void leaveNetwork();
    void quitApplication();

private slots:
    void handleTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void handleShowMainWindow();    void handleJoinNetwork();
    void handleLeaveNetwork();
    void handleQuitApplication();

private:
    void createTrayIcon();
    void createContextMenu();
    void updateTrayIconToolTip();
    
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_contextMenu;
      // Actions
    QAction* m_showAction;
    QAction* m_joinNetworkAction;
    QAction* m_leaveNetworkAction;
    QAction* m_networkStatusAction;
    QAction* m_separatorAction;
    QAction* m_quitAction;
    
    MainWindow* m_mainWindow;
    VpnWidget* m_vpnWidget;
};

#endif // SYSTEMTRAYMANAGER_H
