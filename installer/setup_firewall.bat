@echo off
echo =========================================
echo Visco Connect Demo - Post Install Setup
echo =========================================

REM Check if running as administrator
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    echo Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

echo.
echo Adding Windows Firewall Rules...

REM Add TCP firewall rule for port 7777
echo Adding TCP rule for Echo Server (port 7777)...
netsh advfirewall firewall add rule name="Visco Connect Demo Echo" dir=in action=allow protocol=TCP localport=7777
if %errorlevel% equ 0 (
    echo ✓ TCP rule added successfully
) else (
    echo ✗ Failed to add TCP rule
)

echo.
REM Add ICMP firewall rule
echo Adding ICMP rule for ping responses...
netsh advfirewall firewall add rule name="ICMP Allow incoming V4 echo request" protocol=icmpv4:8,any dir=in action=allow
if %errorlevel% equ 0 (
    echo ✓ ICMP rule added successfully
) else (
    echo ✗ Failed to add ICMP rule
)

echo.
echo =========================================
echo Firewall Setup Complete!
echo =========================================
echo.
echo The following firewall rules have been added:
echo 1. TCP port 7777 - For Echo Server functionality
echo 2. ICMP Echo Request - For ping responses
echo.
echo Your Visco Connect Demo application is now ready to use with
echo bidirectional network connectivity through WireGuard VPN.
echo.
echo To test connectivity:
echo - From remote server: ping [this-machine-ip]
echo - From remote server: echo "test" ^| nc [this-machine-ip] 7777
echo.
pause
