@echo off
echo ==========================================
echo Visco Connect Demo - Firewall Cleanup
echo ==========================================

REM Check if running as administrator
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    echo Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

echo.
echo Removing Windows Firewall Rules...

REM Remove TCP firewall rule
echo Removing TCP rule for Echo Server...
netsh advfirewall firewall delete rule name="Visco Connect Demo Echo"
if %errorlevel% equ 0 (
    echo ✓ TCP rule removed successfully
) else (
    echo ✗ TCP rule not found or failed to remove
)

echo.
REM Remove ICMP firewall rule
echo Removing ICMP rule...
netsh advfirewall firewall delete rule name="ICMP Allow incoming V4 echo request"
if %errorlevel% equ 0 (
    echo ✓ ICMP rule removed successfully
) else (
    echo ✗ ICMP rule not found or failed to remove
)

echo.
echo ==========================================
echo Firewall Cleanup Complete!
echo ==========================================
echo.
echo All Visco Connect Demo firewall rules have been removed.
echo.
pause
