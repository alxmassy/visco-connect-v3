@echo off
setlocal

echo ============================================
echo Visco Connect - Qt Deployment Script
echo ============================================

set PROJECT_ROOT=%~dp0
set BUILD_DIR=%PROJECT_ROOT%\build\bin
set QT_PATH=C:\Qt\6.5.3\mingw_64

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

REM Create plugins directory
if not exist "%BUILD_DIR%\platforms" mkdir "%BUILD_DIR%\platforms"
if not exist "%BUILD_DIR%\styles" mkdir "%BUILD_DIR%\styles"
if not exist "%BUILD_DIR%\imageformats" mkdir "%BUILD_DIR%\imageformats"

REM Copy platform plugin manually
echo Copying platform plugins...
copy /Y "%QT_PATH%\plugins\platforms\qwindows.dll" "%BUILD_DIR%\platforms\" >nul
copy /Y "%QT_PATH%\plugins\platforms\qminimal.dll" "%BUILD_DIR%\platforms\" >nul

REM Copy styles
echo Copying styles...
copy /Y "%QT_PATH%\plugins\styles\qwindowsvistastyle.dll" "%BUILD_DIR%\styles\" >nul

REM Copy image format plugins
echo Copying image format plugins...
copy /Y "%QT_PATH%\plugins\imageformats\*.dll" "%BUILD_DIR%\imageformats\" >nul

REM Run windeployqt
echo Running windeployqt...
"%QT_PATH%\bin\windeployqt.exe" --release --no-translations "%BUILD_DIR%\ViscoConnect.exe" >nul 2>&1

REM Copy MinGW runtime DLLs
echo Copying MinGW runtime DLLs...
copy /Y "%QT_PATH%\bin\libgcc_s_seh-1.dll" "%BUILD_DIR%\" >nul 2>nul
copy /Y "%QT_PATH%\bin\libstdc++-6.dll" "%BUILD_DIR%\" >nul 2>nul
copy /Y "%QT_PATH%\bin\libwinpthread-1.dll" "%BUILD_DIR%\" >nul 2>nul

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
