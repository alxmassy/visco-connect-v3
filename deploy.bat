@echo off
setlocal

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
"%QT_PATH%\bin\windeployqt.exe" --release --no-translations --compiler-runtime "%BUILD_DIR%\Visco Connect.exe"

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
