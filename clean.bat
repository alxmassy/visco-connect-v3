@echo off
echo Cleaning Visco Connect Build...

if not exist "build" (
    echo Build directory does not exist. Nothing to clean.
    pause
    exit /b 0
)

echo Removing build files...
cd build
del /Q /S * >nul 2>&1
for /d %%p in (*) do rmdir "%%p" /s /q >nul 2>&1
cd ..

echo.
echo Build directory cleaned successfully!
echo.
echo You can now run configure.bat to reconfigure the project.
echo.
pause
