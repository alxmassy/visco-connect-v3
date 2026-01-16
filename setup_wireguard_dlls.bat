@echo off
echo ================================================================
echo          WireGuard DLLs Setup for Camera Server Qt6
echo ================================================================
echo.

echo This script helps you set up the required WireGuard DLLs for VPN functionality.
echo.

REM Check if DLLs already exist
set "DLL_DIR=."
if exist "build\Release" set "DLL_DIR=build\Release"
if exist "build\bin" set "DLL_DIR=build\bin"
if exist "build" if not exist "build\Release" if not exist "build\bin" set "DLL_DIR=build"

echo Target directory: %DLL_DIR%
echo.

REM Check current status
echo Current DLL status:
if exist "%DLL_DIR%\tunnel.dll" (
    echo   [OK] tunnel.dll found
) else (
    echo   [MISSING] tunnel.dll not found
)

if exist "%DLL_DIR%\wireguard.dll" (
    echo   [OK] wireguard.dll found
) else (
    echo   [MISSING] wireguard.dll not found
)

echo.

REM Check if DLLs exist in common locations
set "WIREGUARD_FOUND=0"
set "COMMON_PATHS=C:\Program Files\WireGuard C:\WireGuard %USERPROFILE%\Downloads ."

echo Searching for WireGuard DLLs in common locations...

for %%P in (%COMMON_PATHS%) do (
    if exist "%%P\tunnel.dll" if exist "%%P\wireguard.dll" (
        echo   [FOUND] DLLs found in: %%P
        
        echo.
        set /p "COPY_CHOICE=Copy DLLs from %%P to %DLL_DIR%? (y/n): "
        if /i "!COPY_CHOICE!"=="y" (
            copy "%%P\tunnel.dll" "%DLL_DIR%\" >nul 2>&1
            copy "%%P\wireguard.dll" "%DLL_DIR%\" >nul 2>&1
            if exist "%DLL_DIR%\tunnel.dll" if exist "%DLL_DIR%\wireguard.dll" (
                echo   [OK] DLLs copied successfully!
                set "WIREGUARD_FOUND=1"
                goto :dlls_ready
            ) else (
                echo   [ERROR] Failed to copy DLLs
            )
        )
    )
)

:check_manual
if "%WIREGUARD_FOUND%"=="0" (
    echo.
    echo WireGuard DLLs not found automatically.
    echo.
    echo To get the required DLLs:
    echo.
    echo Method 1 - Download from WireGuard Windows:
    echo   1. Visit: https://git.zx2c4.com/wireguard-windows/about/
    echo   2. Look for "embeddable-dll-service" documentation
    echo   3. Download the prebuilt DLLs or build from source
    echo   4. Place tunnel.dll and wireguard.dll in: %DLL_DIR%
    echo.
    echo Method 2 - Extract from WireGuard Installation:
    echo   1. Install WireGuard for Windows from: https://www.wireguard.com/install/
    echo   2. Look for tunnel.dll and wireguard.dll in the installation directory
    echo   3. Copy them to: %DLL_DIR%
    echo.
    echo Method 3 - Use csharp-samples directory:
    echo   If you have the DLLs in the csharp-samples directory, you can:
    
    if exist "csharp-samples\tunnel.dll" if exist "csharp-samples\wireguard.dll" (
        echo   [FOUND] DLLs found in csharp-samples directory!
        echo.
        set /p "COPY_CHOICE=Copy DLLs from csharp-samples to %DLL_DIR%? (y/n): "
        if /i "!COPY_CHOICE!"=="y" (
            copy "csharp-samples\tunnel.dll" "%DLL_DIR%\" >nul 2>&1
            copy "csharp-samples\wireguard.dll" "%DLL_DIR%\" >nul 2>&1
            if exist "%DLL_DIR%\tunnel.dll" if exist "%DLL_DIR%\wireguard.dll" (
                echo   [OK] DLLs copied successfully from csharp-samples!
                set "WIREGUARD_FOUND=1"
                goto :dlls_ready
            )
        )
    ) else (
        echo   Copy tunnel.dll and wireguard.dll to csharp-samples directory
        echo   Then run this script again.
    )
)

:dlls_ready
echo.
if "%WIREGUARD_FOUND%"=="1" (
    echo ================================================================
    echo                   WIREGUARD DLLS READY!
    echo ================================================================
    echo.
    echo The Camera Server Qt6 application is now ready to use VPN features.
    echo.
    echo Important notes:
    echo   - Run the application as Administrator for VPN functionality
    echo   - The first time you connect, Windows may prompt for driver installation
    echo   - Check the application logs for VPN-related messages
    echo.
) else (
    echo ================================================================
    echo                 MANUAL SETUP REQUIRED
    echo ================================================================
    echo.
    echo Please manually obtain tunnel.dll and wireguard.dll and place them in:
    echo   %DLL_DIR%
    echo.
    echo The application will work without these DLLs, but VPN features will be disabled.
    echo.
)

echo To verify DLL setup after manual installation:
echo   1. Check that both tunnel.dll and wireguard.dll exist in %DLL_DIR%
echo   2. Run the Camera Server Qt6 application as Administrator
echo   3. Look for VPN widget on the right side of the main window
echo   4. Check application logs for "WireGuard Manager initialized successfully"
echo.

pause
