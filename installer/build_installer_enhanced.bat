@echo off
setlocal

echo ============================================
echo Camera Server Qt6 WiX Installer Builder
echo ============================================

REM Set variables
set PROJECT_ROOT=%~dp0..
set INSTALLER_DIR=%~dp0
set BUILD_DIR=%PROJECT_ROOT%\build\bin
set OUTPUT_DIR=%INSTALLER_DIR%\output
set WIX_PATH=C:\Program Files (x86)\WiX Toolset v3.14\bin

REM Add WiX to PATH for this session
set PATH=%WIX_PATH%;%PATH%

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Check if WiX is installed
where heat.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: WiX Toolset not found in PATH!
    echo Please install WiX Toolset and add it to your PATH.
    pause
    exit /b 1
)

REM Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo ERROR: Build directory not found: %BUILD_DIR%
    echo Please build the application first using build.bat
    pause
    exit /b 1
)

echo.
echo Available installer versions:
echo 1. Basic Installer (no firewall auto-config)
echo 2. Complete Installer (with firewall scripts and UI)
echo.
set /p INSTALLER_TYPE="Choose installer type (1 or 2): "

if "%INSTALLER_TYPE%"=="1" (
    set WXS_FILE=CameraServerBasic.wxs
    set OUTPUT_NAME=CameraServerQt6Setup-Basic.msi
    echo Building BASIC installer...
) else if "%INSTALLER_TYPE%"=="2" (
    set WXS_FILE=CameraServerComplete.wxs
    set OUTPUT_NAME=CameraServerQt6Setup-Complete.msi
    echo Building COMPLETE installer with firewall configuration...
) else (
    echo Invalid choice. Using basic installer.
    set WXS_FILE=CameraServerBasic.wxs
    set OUTPUT_NAME=CameraServerQt6Setup-Basic.msi
)

echo.
echo Step 1: Creating icon file...
REM Check for ICO file
if exist "%INSTALLER_DIR%\camera_server_icon.ico" (
    echo Icon file found: camera_server_icon.ico
) else (
    echo WARNING: Icon file not found. Creating a default reference...
    REM Create a simple default icon reference
    copy NUL "%INSTALLER_DIR%\camera_server_icon.ico" >nul 2>&1
    echo Default icon placeholder created.
    echo You can replace it with a real ICO file for better appearance.
)

echo Step 2: Harvesting files from build directory...
REM Use Heat to harvest all files from the build\bin directory
heat.exe dir "%BUILD_DIR%" ^
    -cg HarvestedFiles ^
    -gg ^
    -scom ^
    -sreg ^
    -sfrag ^
    -srd ^
    -dr INSTALLFOLDER ^
    -var var.BuildDir ^
    -out "%INSTALLER_DIR%\HarvestedFiles.wxs"

if %errorlevel% neq 0 (
    echo ERROR: Failed to harvest files!
    pause
    exit /b 1
)

echo Step 3: Compiling WiX source files...
REM Compile the main WXS file and harvested files
candle.exe -arch x64 ^
    -dSourceDir="%PROJECT_ROOT%" ^
    -dBuildDir="%BUILD_DIR%" ^
    "%INSTALLER_DIR%\%WXS_FILE%" ^
    "%INSTALLER_DIR%\HarvestedFiles.wxs" ^
    -out "%OUTPUT_DIR%\\"

if %errorlevel% neq 0 (
    echo ERROR: Failed to compile WiX source files!
    pause
    exit /b 1
)

echo Step 4: Linking MSI package...
REM Extract WXS filename without extension for wixobj
for %%f in ("%WXS_FILE%") do set WXS_NAME=%%~nf

REM Link the compiled objects into an MSI
light.exe -ext WixUIExtension ^
    "%OUTPUT_DIR%\%WXS_NAME%.wixobj" ^
    "%OUTPUT_DIR%\HarvestedFiles.wixobj" ^
    -out "%OUTPUT_DIR%\%OUTPUT_NAME%"

if %errorlevel% neq 0 (
    echo ERROR: Failed to link MSI package!
    pause
    exit /b 1
)

echo.
echo ============================================
echo SUCCESS: Installer created successfully!
echo ============================================
echo.
echo Output: %OUTPUT_DIR%\%OUTPUT_NAME%
echo.
if "%INSTALLER_TYPE%"=="2" (
    echo COMPLETE INSTALLER FEATURES:
    echo ✓ Application files and Qt dependencies
    echo ✓ Desktop and Start Menu shortcuts
    echo ✓ Firewall configuration scripts included
    echo ✓ Start Menu shortcuts for firewall management
    echo ✓ Automatic firewall prompt during installation
    echo.
    echo FIREWALL RULES:
    echo • TCP port 7777 - Camera Server Echo functionality
    echo • ICMP Echo Request - Ping response capability
) else (
    echo BASIC INSTALLER FEATURES:
    echo ✓ Application files and Qt dependencies
    echo ✓ Desktop and Start Menu shortcuts
    echo.
    echo NOTE: Firewall rules must be configured manually.
    echo See FIREWALL_CONFIGURATION.md for instructions.
)
echo.
echo To test the installer:
echo 1. Run as Administrator: %OUTPUT_DIR%\%OUTPUT_NAME%
echo 2. Follow the installation wizard
if "%INSTALLER_TYPE%"=="2" (
    echo 3. Choose whether to auto-configure firewall
    echo 4. Check that shortcuts and firewall scripts are installed
) else (
    echo 3. Manually configure firewall if needed
)
echo.

REM Clean up intermediate files
del "%OUTPUT_DIR%\*.wixobj" 2>nul
del "%OUTPUT_DIR%\*.wixpdb" 2>nul

echo Build complete! Press any key to exit.
pause >nul
