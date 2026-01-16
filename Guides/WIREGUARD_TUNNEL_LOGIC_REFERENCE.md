# WireGuard Tunnel Logic Reference

## Overview

This document explains the correct implementation of WireGuard tunnel integration in Qt applications, documenting the issues we encountered and the proper solutions.

## The Problem: What We Were Doing Wrong

### 1. **Incorrect Direct API Call**

**❌ WRONG APPROACH:**
```cpp
// This was our initial incorrect approach
bool WireGuardManager::createTunnelService(const QString& configPath, const QString& serviceName)
{
    // Trying to call WireGuardTunnelService directly from our application
    std::wstring wideConfigPath = configPath.toStdWString();
    bool result = m_tunnelServiceFunc(wideConfigPath.c_str());  // This fails!
    return result;
}
```

**❌ WHY THIS WAS WRONG:**
- `WireGuardTunnelService` is designed to be called by the Windows Service Manager, not directly by applications
- The function expects to run in a service context with proper service lifecycle management
- Direct calls bypass the required Windows service architecture

### 2. **Misunderstanding the WireGuard Service Architecture**

**❌ WRONG UNDERSTANDING:**
We thought WireGuard worked like this:
```
Application → WireGuardTunnelService() → Tunnel Running
```

**✅ CORRECT ARCHITECTURE:**
WireGuard actually works like this:
```
Application → Creates Windows Service → Service Manager starts service → 
Service calls "YourApp.exe /service config.conf" → 
Your app loads tunnel.dll → WireGuardTunnelService() → Tunnel Running
```

### 3. **Missing Service Management**

**❌ WHAT WAS MISSING:**
- No Windows service creation
- No service lifecycle management (start/stop/remove)
- No proper cleanup when disconnecting

### 4. **UI Thread Blocking**

**❌ WRONG APPROACH:**
```cpp
// This blocked the UI thread
void VpnWidget::onConnectClicked()
{
    if (m_wireGuardManager->connectTunnel(selectedConfig)) {
        // UI would freeze here
    }
}
```

### 5. **Missing Command Line Handling**

**❌ WHAT WAS MISSING:**
Our main() function didn't handle the `/service` parameter that Windows services pass.

## The Solution: Correct WireGuard Integration

### 1. **Proper Service Creation**

**✅ CORRECT APPROACH:**
```cpp
bool WireGuardManager::createTunnelService(const QString& configPath, const QString& serviceName)
{
    // Create a Windows service that will call our executable
    QString exePath = QCoreApplication::applicationFilePath();
    QString serviceCmd = QString("\"%1\" /service \"%2\"")
        .arg(QDir::toNativeSeparators(exePath), QDir::toNativeSeparators(configPath));
    
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    SC_HANDLE service = CreateService(
        scManager,
        serviceName.toStdWString().c_str(),
        displayName.toStdWString().c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        serviceCmd.toStdWString().c_str(),
        nullptr, nullptr,
        L"Nsi\0TcpIp\0",  // Required dependencies
        nullptr, nullptr
    );
    
    // Set required service configuration
    SERVICE_SID_INFO sidInfo;
    sidInfo.dwServiceSidType = SERVICE_SID_TYPE_UNRESTRICTED;  // CRITICAL!
    ChangeServiceConfig2(service, SERVICE_CONFIG_SERVICE_SID_INFO, &sidInfo);
    
    return true;
}
```

### 2. **Proper Service Management**

**✅ COMPLETE SERVICE LIFECYCLE:**
```cpp
// Create service
bool createTunnelService(const QString& configPath, const QString& serviceName);

// Start service  
bool startTunnelService(const QString& serviceName);

// Stop service
bool stopTunnelService(const QString& serviceName);

// Remove service
bool removeTunnelService(const QString& serviceName);
```

### 3. **Command Line Handling**

**✅ CORRECT MAIN() FUNCTION:**
```cpp
int main(int argc, char *argv[])
{
    // Handle WireGuard service mode FIRST (before QApplication)
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "/service") {
        QString configPath = QString::fromLocal8Bit(argv[2]);
        
        HMODULE tunnelDll = LoadLibraryA("tunnel.dll");
        WireGuardTunnelServiceFunc tunnelServiceFunc = 
            (WireGuardTunnelServiceFunc)GetProcAddress(tunnelDll, "WireGuardTunnelService");
        
        std::wstring wideConfigPath = configPath.toStdWString();
        bool result = tunnelServiceFunc(wideConfigPath.c_str());
        
        FreeLibrary(tunnelDll);
        return result ? 0 : 1;
    }
    
    // Regular application startup
    QApplication app(argc, argv);
    // ... rest of application
}
```

### 4. **Asynchronous UI Operations**

**✅ NON-BLOCKING UI:**
```cpp
void VpnWidget::onConnectClicked()
{
    setConnectionInProgress(true);
    
    // Use QTimer::singleShot for asynchronous operation
    QTimer::singleShot(10, [this, selectedConfig]() {
        bool success = m_wireGuardManager->connectTunnel(selectedConfig);
        
        // Update UI on main thread
        setConnectionInProgress(false);
        if (success) {
            updateConnectionStatus();
        } else {
            showConnectionError();
        }
    });
}
```

## Key Technical Requirements

### 1. **Required Windows Service Configuration**

```
Service Name:    "WireGuardTunnel$ConfigName"
Display Name:    "WireGuard Tunnel: ConfigName"
Service Type:    SERVICE_WIN32_OWN_PROCESS
Start Type:      SERVICE_DEMAND_START
Error Control:   SERVICE_ERROR_NORMAL
Dependencies:    ["Nsi", "TcpIp"]
SID Type:        SERVICE_SID_TYPE_UNRESTRICTED  // CRITICAL!
Executable:      "C:\path\to\app.exe /service config.conf"
```

### 2. **Required DLLs**

- `tunnel.dll` - Contains WireGuardTunnelService function
- `wireguard.dll` - WireGuard driver interface

### 3. **Administrator Privileges**

WireGuard operations require Administrator privileges for:
- Creating/managing Windows services
- Network adapter operations
- Driver interactions

## Critical Success Factors

### ✅ DO:

1. **Always create Windows services** - Never call WireGuardTunnelService directly
2. **Handle `/service` command line parameter** - Essential for service operation
3. **Set SERVICE_SID_TYPE_UNRESTRICTED** - Required by WireGuard
4. **Use asynchronous operations** - Prevent UI freezing
5. **Proper service dependencies** - "Nsi" and "TcpIp" are required
6. **Run as Administrator** - Required for service creation and VPN operations

### ❌ DON'T:

1. **Call WireGuardTunnelService directly** - It won't work outside service context
2. **Block the UI thread** - Use asynchronous operations
3. **Forget service cleanup** - Always remove services when disconnecting
4. **Skip SID configuration** - SERVICE_SID_TYPE_UNRESTRICTED is mandatory
5. **Ignore error handling** - Service operations can fail in many ways

## Error Debugging Guide

### Common Issues:

1. **"WireGuardTunnelService returns false"**
   - Check if running as Administrator
   - Verify tunnel.dll is available
   - Ensure config file exists and is valid

2. **"Service fails to start"**
   - Check SERVICE_SID_TYPE_UNRESTRICTED is set
   - Verify dependencies (Nsi, TcpIp) are available
   - Check executable path in service configuration

3. **"UI freezes during connection"**
   - Use QTimer::singleShot for asynchronous operations
   - Don't call blocking operations on main thread

4. **"Service creation fails"**
   - Ensure running as Administrator
   - Check if service name already exists
   - Verify Service Control Manager access

## Reference Implementation Files

- `src/WireGuardManager.cpp` - Service management implementation
- `src/main.cpp` - Command line handling for `/service` parameter
- `src/VpnWidget.cpp` - Asynchronous UI operations

## Official Documentation References

- [WireGuard Windows Repository](https://github.com/WireGuard/wireguard-windows)
- [Embeddable DLL Service](https://github.com/WireGuard/wireguard-windows/tree/main/embeddable-dll-service)
- [WireGuard Windows Documentation](https://github.com/WireGuard/wireguard-windows/blob/main/docs/)

---

**Last Updated:** July 27, 2025  
**Created by:** Camera Server Qt6 Development Team
