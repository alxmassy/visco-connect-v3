@echo off
setlocal

echo ============================================
echo Visco Connect Demo WiX Installer Builder
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

echo Step 1: Copying application icon...
REM Copy logo.ico from project resources into installer directory
if exist "%PROJECT_ROOT%\resources\logo.ico" (
    copy /Y "%PROJECT_ROOT%\resources\logo.ico" "%INSTALLER_DIR%\logo.ico" >nul
    echo Icon file copied: logo.ico
) else (
    echo ERROR: resources\logo.ico not found in project resources.
    echo Please ensure logo.ico exists in the resources directory.
    pause
    exit /b 1
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
    "%INSTALLER_DIR%\CameraServerBasic.wxs" ^
    "%INSTALLER_DIR%\HarvestedFiles.wxs" ^
    -out "%OUTPUT_DIR%\\"

if %errorlevel% neq 0 (
    echo ERROR: Failed to compile WiX source files!
    pause
    exit /b 1
)

echo Step 4: Linking MSI package...
REM Link the compiled objects into an MSI
light.exe -ext WixUIExtension ^
    "%OUTPUT_DIR%\CameraServerBasic.wixobj" ^
    "%OUTPUT_DIR%\HarvestedFiles.wixobj" ^
    -out "%OUTPUT_DIR%\ViscoConnect_v3.1.7_Setup.msi"

if %errorlevel% neq 0 (
    echo ERROR: Failed to link MSI package!
    pause
    exit /b 1
)

echo.
echo ============================================
echo MSI package created successfully!
echo ============================================
echo.
echo MSI Output: %OUTPUT_DIR%\ViscoConnect_v3.1.7_Setup.msi
echo.

REM ===================== BOOTSTRAPPER BUILD =====================
echo Step 5: Building bootstrapper bundle...

REM Check if VC++ Redistributable exists
if not exist "%INSTALLER_DIR%\vc_redist.x64.exe" (
    echo.
    echo WARNING: vc_redist.x64.exe not found in installer directory!
    echo.
    echo To create a standalone installer that works on any PC:
    echo 1. Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe
    echo 2. Save to: %INSTALLER_DIR%\vc_redist.x64.exe
    echo 3. Re-run this script
    echo.
    echo Skipping bootstrapper build. MSI-only installer created.
    goto :cleanup
)

echo Compiling Bundle.wxs...
candle.exe -arch x64 ^
    -ext WixBalExtension ^
    -ext WixUtilExtension ^
    "%INSTALLER_DIR%\Bundle.wxs" ^
    -out "%OUTPUT_DIR%\\"

if %errorlevel% neq 0 (
    echo ERROR: Failed to compile Bundle.wxs!
    pause
    exit /b 1
)

echo Step 6: Linking bootstrapper executable...
light.exe ^
    -ext WixBalExtension ^
    -ext WixUtilExtension ^
    "%OUTPUT_DIR%\Bundle.wixobj" ^
    -out "%OUTPUT_DIR%\ViscoConnect_v3.1.7_Setup.exe"

if %errorlevel% neq 0 (
    echo ERROR: Failed to link bootstrapper!
    pause
    exit /b 1
)

echo.
echo ============================================
echo SUCCESS: Bootstrapper created successfully!
echo ============================================
echo.
echo STANDALONE INSTALLER (recommended):
echo   %OUTPUT_DIR%\ViscoConnect_v3.1.7_Setup.exe
echo   - Includes VC++ Runtime
echo   - Works on any Windows PC
echo.
echo MSI ONLY (requires VC++ Runtime pre-installed):
echo   %OUTPUT_DIR%\ViscoConnect_v3.1.7_Setup.msi
echo.

:cleanup
REM Clean up intermediate files
del "%OUTPUT_DIR%\*.wixobj" 2>nul
del "%OUTPUT_DIR%\*.wixpdb" 2>nul

echo Build complete! Press any key to exit.
pause >nul
