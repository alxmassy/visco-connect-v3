#include "SystemTrayManager.h"
#include "MainWindow.h"
#include "VpnWidget.h"
#include "Logger.h"
#include <QApplication>
#include <QMessageBox>
#include <QIcon>

SystemTrayManager::SystemTrayManager(MainWindow* mainWindow, VpnWidget* vpnWidget, QObject *parent)
    : QObject(parent)
    , m_trayIcon(nullptr)
    , m_contextMenu(nullptr)
    , m_mainWindow(mainWindow)
    , m_vpnWidget(vpnWidget)
{
}

SystemTrayManager::~SystemTrayManager()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

void SystemTrayManager::initialize()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        LOG_WARNING("System tray is not available", "SystemTrayManager");
        return;
    }
    
    createTrayIcon();
    createContextMenu();
    
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayManager::handleTrayIconActivated);
      updateTrayIconToolTip();
    updateVpnStatus();
    
    // Show the tray icon immediately
    m_trayIcon->show();
    
    // Show initial notification
    showNotification("Visco Connect", "Application started and running in system tray");
    
    LOG_INFO("System tray manager initialized", "SystemTrayManager");
}

void SystemTrayManager::show()
{
    if (m_trayIcon) {
        m_trayIcon->show();
    }
}

void SystemTrayManager::hide()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

bool SystemTrayManager::isVisible() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

void SystemTrayManager::handleTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::DoubleClick:
            handleShowMainWindow();
            break;
        case QSystemTrayIcon::Trigger:
            // Single click - do nothing for now
            break;
        case QSystemTrayIcon::MiddleClick:
            // Middle click - do nothing for now
            break;
        case QSystemTrayIcon::Context:
            // Context menu will be shown automatically
            break;
        default:
            break;
    }
}

void SystemTrayManager::handleShowMainWindow()
{
    emit showMainWindow();
}

void SystemTrayManager::handleJoinNetwork()
{
    if (m_vpnWidget) {
        m_vpnWidget->connectToNetwork();
        // No notification when joining network
        LOG_INFO("Join network requested via system tray", "SystemTrayManager");
    }
}

void SystemTrayManager::handleLeaveNetwork()
{
    if (m_vpnWidget) {
        m_vpnWidget->disconnectFromNetwork();
        // No notification when leaving network
        LOG_INFO("Leave network requested via system tray", "SystemTrayManager");
    }
}

void SystemTrayManager::handleQuitApplication()
{
    LOG_INFO("Quit requested via system tray", "SystemTrayManager");
    emit quitApplication();
}

void SystemTrayManager::updateVpnStatus()
{
    if (!m_vpnWidget) {
        return;
    }
    
    bool isConnected = m_vpnWidget->isConnected();
    
    // Update action states
    if (m_joinNetworkAction) {
        m_joinNetworkAction->setEnabled(!isConnected);
        m_joinNetworkAction->setText(isConnected ? "Connected to Network" : "Join Network");
    }
    
    if (m_leaveNetworkAction) {
        m_leaveNetworkAction->setEnabled(isConnected);
    }
    
    if (m_networkStatusAction) {
        QString statusText = isConnected ? "Status: Connected" : "Status: Disconnected";
        m_networkStatusAction->setText(statusText);
    }
    
    updateTrayIconToolTip();
}

void SystemTrayManager::createTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    
    // Try to load custom icon first (use new logo), fallback to original and then system icon
    QIcon icon(":/icons/logo.ico");
    if (icon.isNull()) {
        icon = QIcon(":/icons/logo.png");
    }
    if (icon.isNull()) {
        icon = QIcon(":/icons/camera_server_icon.svg");
    }
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    m_trayIcon->setIcon(icon);
}

void SystemTrayManager::createContextMenu()
{
    m_contextMenu = new QMenu();
    
    // Show main window action
    m_showAction = new QAction("Show Main Window", this);
    connect(m_showAction, &QAction::triggered, this, &SystemTrayManager::handleShowMainWindow);
    m_contextMenu->addAction(m_showAction);
    
    m_contextMenu->addSeparator();
    
    // Network status (read-only)
    m_networkStatusAction = new QAction("Status: Checking...", this);
    m_networkStatusAction->setEnabled(false);
    m_contextMenu->addAction(m_networkStatusAction);
    
    m_contextMenu->addSeparator();
    
    // Join network action
    m_joinNetworkAction = new QAction("Join Network", this);
    connect(m_joinNetworkAction, &QAction::triggered, this, &SystemTrayManager::handleJoinNetwork);
    m_contextMenu->addAction(m_joinNetworkAction);
    
    // Leave network action
    m_leaveNetworkAction = new QAction("Leave Network", this);
    connect(m_leaveNetworkAction, &QAction::triggered, this, &SystemTrayManager::handleLeaveNetwork);
    m_contextMenu->addAction(m_leaveNetworkAction);
    
    m_contextMenu->addSeparator();
    
    // Exit action
    m_quitAction = new QAction("Exit", this);
    connect(m_quitAction, &QAction::triggered, this, &SystemTrayManager::handleQuitApplication);
    m_contextMenu->addAction(m_quitAction);
    
    m_trayIcon->setContextMenu(m_contextMenu);
}

void SystemTrayManager::updateTrayIconToolTip()
{
    if (!m_trayIcon || !m_vpnWidget) {
        return;
    }
    
    bool isConnected = m_vpnWidget->isConnected();
      QString toolTip = QString("Visco Connect\nNetwork Status: %1")
                      .arg(isConnected ? "Connected" : "Disconnected");
    
    m_trayIcon->setToolTip(toolTip);
}

void SystemTrayManager::showNotification(const QString& title, const QString& message, QSystemTrayIcon::MessageIcon icon)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(title, message, icon, 3000); // Show for 3 seconds
    }
}

void SystemTrayManager::notifyVpnStatusChange(const QString& status, bool connected)
{
    // No notifications for network status changes
    // Users can check status via system tray menu
}
