#ifndef WINDOWSSERVICE_H
#define WINDOWSSERVICE_H

#include <QObject>
#include <QCoreApplication>
#include <windows.h>

class WindowsService : public QObject
{
    Q_OBJECT

public:
    static WindowsService& instance();
    
    bool isRunningAsService() const;
    bool installService();
    bool uninstallService();
    bool startServiceMode();
    bool isServiceInstalled() const;
    
    void setServiceName(const QString& name);
    void setServiceDisplayName(const QString& displayName);
    void setServiceDescription(const QString& description);

signals:
    void serviceStarted();
    void serviceStopped();
    void serviceError(const QString& error);

private:
    WindowsService();
    ~WindowsService();
    
    static void WINAPI serviceMain(DWORD argc, LPTSTR* argv);
    static void WINAPI serviceControlHandler(DWORD controlCode);
    static DWORD WINAPI serviceWorkerThread(LPVOID lpParam);
    
    void reportServiceStatus(DWORD currentState, DWORD exitCode, DWORD waitHint);
    
    QString m_serviceName;
    QString m_serviceDisplayName;
    QString m_serviceDescription;
    
    static SERVICE_STATUS_HANDLE s_serviceStatusHandle;
    static SERVICE_STATUS s_serviceStatus;
    static WindowsService* s_instance;
    
    bool m_isService;
    HANDLE m_serviceStopEvent;
};

#endif // WINDOWSSERVICE_H
