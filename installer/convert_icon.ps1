# PowerShell script to convert SVG to ICO using ImageMagick
# You can install ImageMagick from: https://imagemagick.org/script/download.php#windows

param(
    [string]$SvgPath = ".\resources\camera_server_icon.svg",
    [string]$IcoPath = ".\resources\camera_server_icon.ico"
)

if (Get-Command "magick" -ErrorAction SilentlyContinue) {
    Write-Host "Converting SVG to ICO using ImageMagick..."
    magick $SvgPath -resize 256x256 -define icon:auto-resize=256,128,64,48,32,16 $IcoPath
    Write-Host "Icon converted successfully: $IcoPath"
} else {
    Write-Host "ImageMagick not found. Please install ImageMagick or manually convert the SVG to ICO format."
    Write-Host "You can also use online converters like: https://convertio.co/svg-ico/"
    Write-Host "Place the resulting icon at: $IcoPath"
}
