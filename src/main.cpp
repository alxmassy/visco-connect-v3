#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QSettings>
#include <windows.h>
#include <string>

#include "MainWindow.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "WindowsService.h"
#include "FirewallManager.h"
#include "AuthDialog.h"

// Forward declaration for WireGuard service function
extern "C" {
    typedef bool (*WireGuardTunnelServiceFunc)(const wchar_t* configFile);
}

int main(int argc, char *argv[])
{    // Check for WireGuard service mode first (before creating QApplication)
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "/service") {
        // This is a WireGuard tunnel service call
        QString configPath = QString::fromLocal8Bit(argv[2]);
        
        // Get the directory where the executable is located
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string exeDir = std::string(exePath);
        size_t lastSlash = exeDir.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            exeDir = exeDir.substr(0, lastSlash);
        }
        
        // Try to load tunnel.dll from the executable directory
        std::string tunnelDllPath = exeDir + "\\tunnel.dll";
        HMODULE tunnelDll = LoadLibraryA(tunnelDllPath.c_str());
        if (!tunnelDll) {
            // Fallback: try loading from current directory
            tunnelDll = LoadLibraryA("tunnel.dll");
            if (!tunnelDll) {
                return 1;  // Failed to load tunnel.dll
            }
        }
        
        WireGuardTunnelServiceFunc tunnelServiceFunc = 
            (WireGuardTunnelServiceFunc)GetProcAddress(tunnelDll, "WireGuardTunnelService");
        
        if (!tunnelServiceFunc) {
            FreeLibrary(tunnelDll);
            return 1;  // Failed to get function
        }
        
        // Convert to wide string and call the function
        std::wstring wideConfigPath = configPath.toStdWString();
        bool result = tunnelServiceFunc(wideConfigPath.c_str());
        
        FreeLibrary(tunnelDll);
        return result ? 0 : 1;
    }
      QApplication app(argc, argv);
      // Set application and window icon
      app.setWindowIcon(QIcon(":/icons/logo.ico"));
    
    // Set application properties
    app.setApplicationName("ViscoConnect");
    app.setApplicationVersion("2.1.5");
    app.setOrganizationName("Visco Connect Team");
    app.setApplicationDisplayName("");
    app.setOrganizationDomain("viscoconnect.local");
    
    // Check if running as service
    bool runAsService = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--service") {
            runAsService = true;
            break;
        }
    }
    
    // Initialize logger
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(appDataPath);
    Logger::instance().setLogFile(appDataPath + "/visco-connect.log");
    Logger::instance().setLogLevel(LogLevel::Info);
    
    LOG_INFO("=== Visco Connect v2.1.5 Starting ===", "Main");
    LOG_INFO(QString("Version: %1").arg(app.applicationVersion()), "Main");
    LOG_INFO(QString("Run as service: %1").arg(runAsService ? "Yes" : "No"), "Main");
      // Load configuration
    if (!ConfigManager::instance().loadConfig()) {
        LOG_ERROR("Failed to load configuration", "Main");
        if (!runAsService) {
            QMessageBox::critical(nullptr, "Visco Connect - Error", "Failed to load configuration file");
        }
        return 1;
    }
    
    // Initialize and check firewall rules (with a small delay to allow system to settle)
    QTimer::singleShot(1000, [&app]() {
        LOG_INFO("Checking firewall rules...", "Main");
        FirewallManager firewallManager;
        
        if (!firewallManager.areFirewallRulesPresent()) {
            LOG_INFO("Firewall rules missing, attempting to add them", "Main");
            
            if (firewallManager.addFirewallRules()) {
                LOG_INFO("Firewall rules added successfully", "Main");
            } else {
                LOG_WARNING("Failed to add firewall rules automatically. Please run the application as Administrator or manually add firewall rules using setup_firewall.bat", "Main");
            }
        } else {
            LOG_INFO("Firewall rules already present", "Main");
        }
    });
    
    if (runAsService) {
        // Running as Windows service
        LOG_INFO("Starting Windows service mode", "Main");
        
        if (!WindowsService::instance().startServiceMode()) {
            LOG_ERROR("Failed to start service mode", "Main");
            return 1;
        }
          // Service mode - create minimal application without UI
        // The service will handle camera management in the background
        
        // TODO: In a real implementation, you would create a service-only version
        // that doesn't require GUI components. For now, we'll just run the console app.
        
        LOG_INFO("Service mode started successfully", "Main");
        return app.exec();    } else {
        // Running as regular application
        LOG_INFO("Starting GUI application", "Main");
        
        // Check if user is already authenticated
        QString currentToken = AuthDialog::getCurrentAuthToken();
        if (currentToken.isEmpty()) {
            // No valid token found, show authentication dialog
            LOG_INFO("No valid authentication token found, showing login dialog", "Main");
            AuthDialog authDialog;
            if (authDialog.exec() != QDialog::Accepted) {
                LOG_INFO("User canceled login, exiting application", "Main");
                return 0;
            }
            LOG_INFO("User authenticated successfully", "Main");
        } else {
            // Valid token found, switch to the authenticated user's configuration
            LOG_INFO("Valid authentication token found, skipping login dialog", "Main");
            QSettings authSettings("ViscoConnect", "Auth");
            QString userEmail = authSettings.value("user_email").toString();
            if (!userEmail.isEmpty()) {
                ConfigManager::instance().switchToUser(userEmail);
                LOG_INFO(QString("Loaded configuration for authenticated user: %1").arg(userEmail), "Main");
            }
        }

        // Check if system tray is available
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            QMessageBox::critical(nullptr, "Visco Connect - System Tray",
                                "System tray is not available on this system.");
            return 1;
        }
        
        // Create main window
        LOG_INFO("Creating main window...", "Main");
        MainWindow window;
        // Set the main window icon
        window.setWindowIcon(QIcon(":/icons/logo.ico"));
        LOG_INFO("Main window created", "Main");
        
        // Show the main window initially
        window.show();
        LOG_INFO("Main window shown", "Main");
        
        // Handle application quit cleanup
        QObject::connect(&app, &QApplication::aboutToQuit, []() {
            LOG_INFO("=== Visco Connect v2.1.5 Shutting Down ===", "Main");
        });
        
        LOG_INFO("GUI application initialized successfully", "Main");
        
        return app.exec();
    }
}
