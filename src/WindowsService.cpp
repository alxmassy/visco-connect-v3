#include "WindowsService.h"
#include "Logger.h"
#include <QCoreApplication>
#include <QDir>
#include <string>

// Static member definitions
SERVICE_STATUS_HANDLE WindowsService::s_serviceStatusHandle = nullptr;
SERVICE_STATUS WindowsService::s_serviceStatus = {};
WindowsService* WindowsService::s_instance = nullptr;

WindowsService::WindowsService()
    : m_serviceName("ViscoConnectService")
    , m_serviceDisplayName("Visco Connect Service")
    , m_serviceDescription("Visco Connect - IP Camera Port Forwarding Service")
    , m_isService(false)
    , m_serviceStopEvent(nullptr)
{
    s_instance = this;
    
    // Initialize service status
    s_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    s_serviceStatus.dwCurrentState = SERVICE_STOPPED;
    s_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    s_serviceStatus.dwWin32ExitCode = 0;
    s_serviceStatus.dwServiceSpecificExitCode = 0;
    s_serviceStatus.dwCheckPoint = 0;
    s_serviceStatus.dwWaitHint = 0;
}

WindowsService::~WindowsService()
{
    if (m_serviceStopEvent) {
        CloseHandle(m_serviceStopEvent);
    }
}

WindowsService& WindowsService::instance()
{
    static WindowsService instance;
    return instance;
}

bool WindowsService::isRunningAsService() const
{
    return m_isService;
}

bool WindowsService::installService()
{
    SC_HANDLE schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!schSCManager) {
        LOG_ERROR("Failed to open Service Control Manager", "WindowsService");
        return false;
    }
      QString exePath = QCoreApplication::applicationFilePath();
    QString servicePath = QString("\"%1\" --service").arg(QDir::toNativeSeparators(exePath));
    
    std::wstring wideServiceName = m_serviceName.toStdWString();
    std::wstring wideDisplayName = m_serviceDisplayName.toStdWString();
    std::wstring wideServicePath = servicePath.toStdWString();
    
    SC_HANDLE schService = CreateService(
        schSCManager,
        wideServiceName.c_str(),
        wideDisplayName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        wideServicePath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
    
    if (!schService) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_EXISTS) {
            LOG_WARNING("Service already exists", "WindowsService");
        } else {
            LOG_ERROR(QString("Failed to create service, error: %1").arg(error), "WindowsService");
        }
        CloseServiceHandle(schSCManager);
        return error == ERROR_SERVICE_EXISTS;
    }
      // Set service description
    SERVICE_DESCRIPTION serviceDesc;
    std::wstring wideDesc = m_serviceDescription.toStdWString();
    serviceDesc.lpDescription = const_cast<LPWSTR>(wideDesc.c_str());
    ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &serviceDesc);
    
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    
    LOG_INFO("Service installed successfully", "WindowsService");
    return true;
}

bool WindowsService::uninstallService()
{
    SC_HANDLE schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!schSCManager) {
        LOG_ERROR("Failed to open Service Control Manager", "WindowsService");        return false;
    }
    
    std::wstring wideServiceName = m_serviceName.toStdWString();
    SC_HANDLE schService = OpenService(schSCManager, wideServiceName.c_str(), DELETE);
    if (!schService) {
        LOG_ERROR("Failed to open service for deletion", "WindowsService");
        CloseServiceHandle(schSCManager);
        return false;
    }
    
    bool success = DeleteService(schService) != 0;
    if (!success) {
        LOG_ERROR(QString("Failed to delete service, error: %1").arg(GetLastError()), "WindowsService");
    } else {
        LOG_INFO("Service uninstalled successfully", "WindowsService");
    }
    
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    
    return success;
}

bool WindowsService::startServiceMode()
{    m_isService = true;
    
    std::wstring wideServiceName = m_serviceName.toStdWString();
    SERVICE_TABLE_ENTRY serviceTable[] = {
        { const_cast<LPWSTR>(wideServiceName.c_str()), serviceMain },
        { nullptr, nullptr }
    };
    
    if (!StartServiceCtrlDispatcher(serviceTable)) {
        DWORD error = GetLastError();
        if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            LOG_ERROR("Not running as a service", "WindowsService");
            return false;
        }
        LOG_ERROR(QString("StartServiceCtrlDispatcher failed, error: %1").arg(error), "WindowsService");
        return false;
    }
    
    return true;
}

bool WindowsService::isServiceInstalled() const
{
    SC_HANDLE schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!schSCManager) {        return false;
    }
    
    std::wstring wideServiceName = m_serviceName.toStdWString();
    SC_HANDLE schService = OpenService(schSCManager, wideServiceName.c_str(), SERVICE_QUERY_STATUS);
    bool installed = (schService != nullptr);
    
    if (schService) {
        CloseServiceHandle(schService);
    }
    CloseServiceHandle(schSCManager);
    
    return installed;
}

void WindowsService::setServiceName(const QString& name)
{
    m_serviceName = name;
}

void WindowsService::setServiceDisplayName(const QString& displayName)
{
    m_serviceDisplayName = displayName;
}

void WindowsService::setServiceDescription(const QString& description)
{
    m_serviceDescription = description;
}

void WINAPI WindowsService::serviceMain(DWORD argc, LPTSTR* argv)
{
    if (!s_instance) {
        return;
    }
      s_serviceStatusHandle = RegisterServiceCtrlHandler(
        reinterpret_cast<LPCWSTR>(s_instance->m_serviceName.utf16()),
        serviceControlHandler
    );
    
    if (!s_serviceStatusHandle) {
        return;
    }
    
    // Report initial status
    s_instance->reportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
    
    // Create stop event
    s_instance->m_serviceStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!s_instance->m_serviceStopEvent) {
        s_instance->reportServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }
    
    // Report running status
    s_instance->reportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);
    
    LOG_INFO("Service started", "WindowsService");
    emit s_instance->serviceStarted();
    
    // Create worker thread
    HANDLE workerThread = CreateThread(nullptr, 0, serviceWorkerThread, nullptr, 0, nullptr);
    if (workerThread) {
        WaitForSingleObject(workerThread, INFINITE);
        CloseHandle(workerThread);
    }
    
    // Clean up
    CloseHandle(s_instance->m_serviceStopEvent);
    s_instance->m_serviceStopEvent = nullptr;
    
    s_instance->reportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
    
    LOG_INFO("Service stopped", "WindowsService");
    emit s_instance->serviceStopped();
}

void WINAPI WindowsService::serviceControlHandler(DWORD controlCode)
{
    if (!s_instance) {
        return;
    }
    
    switch (controlCode) {
        case SERVICE_CONTROL_STOP:
            s_instance->reportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
            SetEvent(s_instance->m_serviceStopEvent);
            break;
            
        case SERVICE_CONTROL_INTERROGATE:
            break;
            
        default:
            break;
    }
    
    SetServiceStatus(s_serviceStatusHandle, &s_serviceStatus);
}

DWORD WINAPI WindowsService::serviceWorkerThread(LPVOID lpParam)
{
    if (!s_instance) {
        return 1;
    }
    
    // Wait for stop event
    WaitForSingleObject(s_instance->m_serviceStopEvent, INFINITE);
    
    return 0;
}

void WindowsService::reportServiceStatus(DWORD currentState, DWORD exitCode, DWORD waitHint)
{
    static DWORD checkPoint = 1;
    
    s_serviceStatus.dwCurrentState = currentState;
    s_serviceStatus.dwWin32ExitCode = exitCode;
    s_serviceStatus.dwWaitHint = waitHint;
    
    if (currentState == SERVICE_START_PENDING) {
        s_serviceStatus.dwControlsAccepted = 0;
    } else {
        s_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }
    
    if ((currentState == SERVICE_RUNNING) || (currentState == SERVICE_STOPPED)) {
        s_serviceStatus.dwCheckPoint = 0;
    } else {
        s_serviceStatus.dwCheckPoint = checkPoint++;
    }
    
    SetServiceStatus(s_serviceStatusHandle, &s_serviceStatus);
}
