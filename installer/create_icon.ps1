# PowerShell script to create a basic ICO file from SVG or create a default one
param(
    [string]$OutputPath = ".\camera_server_icon.ico"
)

Write-Host "Creating icon file for WiX installer..."

# Check if we have the SVG file
$svgPath = "..\resources\camera_server_icon.svg"
if (Test-Path $svgPath) {
    Write-Host "SVG icon found. For best results, please manually convert to ICO format."
    Write-Host "You can use online converters like:"
    Write-Host "- https://convertio.co/svg-ico/"
    Write-Host "- https://cloudconvert.com/svg-to-ico"
    Write-Host ""
    Write-Host "After conversion, save the ICO file as: $OutputPath"
} else {
    Write-Host "SVG icon not found. Creating a placeholder reference..."
}

# Create a simple batch file to help with manual conversion
$batchContent = @"
@echo off
echo.
echo Icon Conversion Helper
echo =====================
echo.
echo Please manually convert the SVG icon to ICO format:
echo 1. Open: ..\resources\camera_server_icon.svg
echo 2. Use an online converter or image editor
echo 3. Save as: camera_server_icon.ico (in this directory)
echo 4. Recommended sizes: 256x256, 128x128, 64x64, 48x48, 32x32, 16x16
echo.
echo Online converters:
echo - https://convertio.co/svg-ico/
echo - https://cloudconvert.com/svg-to-ico
echo.
pause
"@

$batchContent | Out-File -FilePath "convert_icon_manual.bat" -Encoding ASCII

Write-Host "Created convert_icon_manual.bat for manual conversion help."
Write-Host ""
Write-Host "Note: If you don't have an ICO file, the installer will use a default Windows icon."
