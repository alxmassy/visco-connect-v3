# Camera Server Qt6 Installer

This directory contains the WiX Toolset installer configuration for Camera Server Qt6.

## Prerequisites

1. **WiX Toolset v3.11+** must be installed and in your PATH
   - Download from: https://wixtoolset.org/releases/
   - Or install via: `dotnet tool install --global wix`

2. **Camera Server Qt6** must be built first
   - Run `build.bat` in the root directory
   - Ensure `build\bin\ViscoConnect.exe` exists

## Files

- `CameraServerMain.wxs` - Main WiX source file
- `build_installer.bat` - Build script to create the MSI
- `License.rtf` - License agreement text
- `create_icon.ps1` - Helper script for icon conversion
- `convert_icon_manual.bat` - Manual icon conversion helper

## Building the Installer

### Quick Start
```batch
# From the installer directory
build_installer.bat
```

### Manual Steps
1. **Convert Icon (Optional)**:
   ```powershell
   .\create_icon.ps1
   ```
   - Manually convert `resources\camera_server_icon.svg` to `camera_server_icon.ico`
   - Or use the placeholder (default Windows icon will be used)

2. **Build the Installer**:
   ```batch
   build_installer.bat
   ```

3. **Output**: `output\CameraServerQt6Setup.msi`

## Installer Features

### What Gets Installed
- ✅ Main application (`ViscoConnect.exe`)
- ✅ Qt6 runtime libraries (Core, GUI, Network, Widgets, SVG)
- ✅ Qt6 plugins (platforms, image formats, styles, etc.)
- ✅ WireGuard DLLs (`tunnel.dll`, `wireguard.dll`)
- ✅ GCC runtime libraries
- ✅ Example configuration file

### Installation Location
- **Target**: `C:\Program Files\Camera Server Qt6\`
- **Scope**: Per-machine (requires administrator)

### Shortcuts Created
- ✅ Desktop shortcut: "Camera Server Qt6"
- ✅ Start Menu shortcut: "Camera Server Qt6"
- ✅ Start Menu uninstall shortcut

### Firewall Rules Added
The installer automatically adds these Windows Firewall rules:

1. **TCP Echo Server Rule (Port 7777)**
   ```
   Rule Name: Camera Server Echo
   Direction: Inbound
   Protocol: TCP
   Port: 7777
   Purpose: Allows EchoServer functionality for bidirectional ping testing
   ```

2. **ICMP Echo Request Rule**
   ```
   Rule Name: ICMP Allow incoming V4 echo request
   Direction: Inbound
   Protocol: ICMPv4 (Type 8)
   Purpose: Allows Windows machine to respond to ping requests
   ```

### Uninstallation
- Removes all installed files
- Removes shortcuts
- **Automatically removes firewall rules**
- Cleans up registry entries

## Testing the Installer

### Installation Test
1. Run as Administrator: `CameraServerQt6Setup.msi`
2. Follow the installation wizard
3. Verify installation in `C:\Program Files\Camera Server Qt6\`
4. Check desktop shortcut works
5. Verify application launches

### Firewall Rules Test
```powershell
# Check if rules were added
netsh advfirewall firewall show rule name="Camera Server Echo"
netsh advfirewall firewall show rule name="ICMP Allow incoming V4 echo request"

# Test from another machine (replace 10.0.0.2 with actual IP)
ping 10.0.0.2
echo "test" | nc 10.0.0.2 7777
```

### Uninstallation Test
1. Go to Programs and Features
2. Uninstall "Camera Server Qt6"
3. Verify files are removed
4. Verify firewall rules are removed:
   ```powershell
   netsh advfirewall firewall show rule name="Camera Server Echo"
   # Should show: "No rules match the specified criteria."
   ```

## Troubleshooting

### Build Issues

**Error: "WiX Toolset not found in PATH"**
- Install WiX Toolset: https://wixtoolset.org/releases/
- Add WiX bin directory to PATH
- Restart command prompt

**Error: "Build directory not found"**
- Run `build.bat` in the root directory first
- Ensure `build\bin\ViscoConnect.exe` exists

**Error: "Failed to harvest files"**
- Check that `build\bin` directory has files
- Ensure no files are locked (close application)
- Try running as Administrator

### Installation Issues

**Error: "Installation package corrupt"**
- Rebuild the MSI using `build_installer.bat`
- Check if antivirus is interfering

**Firewall rules not added**
- Run installer as Administrator
- Check Windows Event Viewer for errors
- Manually add rules using commands in `FIREWALL_CONFIGURATION.md`

**Application won't start**
- Check if all Qt DLLs are in the installation directory
- Verify Visual C++ Redistributable is installed
- Check application event logs

## Customization

### Changing Installation Directory
Edit `CameraServerMain.wxs`:
```xml
<Directory Id="INSTALLFOLDER" Name="Your App Name" />
```

### Adding More Firewall Rules
Add custom actions in `CameraServerMain.wxs`:
```xml
<CustomAction Id="AddCustomRule" 
              Execute="deferred" 
              Impersonate="no" 
              ExeCommand='cmd.exe /c "netsh advfirewall firewall add rule name=&quot;Custom Rule&quot; dir=in action=allow protocol=TCP localport=8080"' 
              Return="ignore" />
```

### Changing Product Information
Edit these properties in `CameraServerMain.wxs`:
- `Name` - Product name
- `Manufacturer` - Company name
- `Version` - Version number
- `UpgradeCode` - Unique identifier (don't change after first release)

## Advanced Usage

### Command Line Installation
```batch
# Silent installation
msiexec /i CameraServerQt6Setup.msi /quiet

# Installation with logging
msiexec /i CameraServerQt6Setup.msi /L*v install.log

# Custom installation directory
msiexec /i CameraServerQt6Setup.msi INSTALLFOLDER="C:\MyApps\CameraServer"
```

### Creating MST Transform Files
For enterprise deployment with custom settings:
```batch
# Create administrative installation
msiexec /a CameraServerQt6Setup.msi TARGETDIR="C:\AdminInstall"

# Use tools like Orca or InstEd to create MST files
```

## Security Notes

- Installer must run as Administrator (required for firewall rules)
- Firewall rules are automatically removed on uninstall
- Rules are specific to required ports only (7777 and ICMP)
- No broad firewall exceptions are created

## Support

For issues with the installer:
1. Check the build logs in the output directory
2. Review Windows Event Viewer (Application and System logs)
3. Verify WiX Toolset installation
4. Test manual firewall rule creation using `FIREWALL_CONFIGURATION.md`
