@echo off
setlocal enabledelayedexpansion

echo ============================================
echo Visco Connect - Qt Deployment Script
echo ============================================

set PROJECT_ROOT=%~dp0
set BUILD_DIR=%PROJECT_ROOT%\build\bin
set QT_PATH=C:\Qt\6.5.3\msvc2019_64

REM Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo ERROR: Build directory not found: %BUILD_DIR%
    echo Please build the application first using build.bat
    pause
    exit /b 1
)

REM Check if executable exists
if not exist "%BUILD_DIR%\Visco Connect.exe" (
    echo ERROR: ViscoConnect.exe not found in %BUILD_DIR%
    echo Please build the application first using build.bat
    pause
    exit /b 1
)

echo Deploying Qt dependencies...

REM Run windeployqt
echo Running windeployqt...
"%QT_PATH%\bin\windeployqt.exe" --release --no-translations --compiler-runtime --force "%BUILD_DIR%\Visco Connect.exe"

REM Check/copy MSVC runtime DLLs for self-contained deployment
echo.
echo Checking MSVC runtime DLLs...
set VCRUNTIME_OK=1

if not exist "%BUILD_DIR%\vcruntime140.dll" set VCRUNTIME_OK=0
if not exist "%BUILD_DIR%\msvcp140.dll" set VCRUNTIME_OK=0

if %VCRUNTIME_OK%==0 (
    echo MSVC runtime not deployed by windeployqt, copying manually...
    REM Try to find MSVC runtime from Visual Studio installation
    set "VCRUNTIME_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC"
    
    REM Find the latest version directory
    for /d %%d in ("%VCRUNTIME_PATH%\*") do set "VCRUNTIME_VER=%%d"
    
    if exist "!VCRUNTIME_VER!\x64\Microsoft.VC143.CRT\vcruntime140.dll" (
        echo Copying from: !VCRUNTIME_VER!\x64\Microsoft.VC143.CRT\
        copy /Y "!VCRUNTIME_VER!\x64\Microsoft.VC143.CRT\vcruntime140.dll" "%BUILD_DIR%\" >nul
        copy /Y "!VCRUNTIME_VER!\x64\Microsoft.VC143.CRT\vcruntime140_1.dll" "%BUILD_DIR%\" >nul 2>&1
        copy /Y "!VCRUNTIME_VER!\x64\Microsoft.VC143.CRT\msvcp140.dll" "%BUILD_DIR%\" >nul
        copy /Y "!VCRUNTIME_VER!\x64\Microsoft.VC143.CRT\msvcp140_1.dll" "%BUILD_DIR%\" >nul 2>&1
        copy /Y "!VCRUNTIME_VER!\x64\Microsoft.VC143.CRT\msvcp140_2.dll" "%BUILD_DIR%\" >nul 2>&1
        echo MSVC runtime DLLs copied successfully.
    ) else (
        echo WARNING: Could not find MSVC runtime DLLs!
        echo Please manually copy vcruntime140.dll and msvcp140.dll to %BUILD_DIR%
    )
)

REM Final verification of MSVC runtime
if exist "%BUILD_DIR%\vcruntime140.dll" (
    echo [OK] vcruntime140.dll present
) else (
    echo [WARN] vcruntime140.dll MISSING - MSI may fail on other PCs!
)
if exist "%BUILD_DIR%\msvcp140.dll" (
    echo [OK] msvcp140.dll present
) else (
    echo [WARN] msvcp140.dll MISSING - MSI may fail on other PCs!
)

REM Verify deployment
echo.
echo Verifying deployment...
if exist "%BUILD_DIR%\platforms\qwindows.dll" (
    echo [OK] Platform plugins deployed
) else (
    echo [FAIL] Platform plugins missing
    pause
    exit /b 1
)

if exist "%BUILD_DIR%\Qt6Core.dll" (
    echo [OK] Qt libraries deployed
) else (
    echo [FAIL] Qt libraries missing
    pause
    exit /b 1
)

echo.
echo ============================================
echo Qt deployment completed successfully!
echo ============================================
echo.
echo Application is ready at: %BUILD_DIR%\ViscoConnect.exe
echo.

pause
