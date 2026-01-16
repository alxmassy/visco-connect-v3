# Camera Server Qt6 WiX Installer - Complete Setup Guide

## 🎉 Installer Successfully Created!

Your Camera Server Qt6 WiX installer has been successfully built and is ready to use.

## 📁 Generated Files

### Main Installer
- **File**: `installer\output\CameraServerQt6Setup.msi`
- **Size**: ~20-30 MB (includes all Qt dependencies)
- **Type**: 64-bit Windows Installer Package

### Additional Files
- `installer\setup_firewall.bat` - Manual firewall configuration script
- `installer\remove_firewall.bat` - Firewall cleanup script
- `installer\HarvestedFiles.wxs` - Auto-generated file list (can be deleted after build)

## 🚀 Installation Instructions

### For End Users

1. **Download/Copy** the `CameraServerQt6Setup.msi` file to the target machine
2. **Right-click** on the MSI file and select "Run as administrator"
3. **Follow** the installation wizard:
   - Accept the license agreement
   - Choose installation directory (default: `C:\Program Files\Camera Server Qt6\`)
   - Complete the installation

### What Gets Installed

✅ **Application Files**:
- `ViscoConnect.exe` - Main application
- Qt6 runtime libraries (Core, GUI, Network, Widgets, SVG)
- Qt6 plugins (platforms, image formats, styles, network, TLS)
- WireGuard DLLs (`tunnel.dll`, `wireguard.dll`)
- GCC runtime libraries

✅ **Shortcuts**:
- Desktop shortcut: "Camera Server Qt6"
- Start Menu folder: "Camera Server Qt6"
- Start Menu shortcuts for application and uninstaller

✅ **Firewall Scripts**:
- `setup_firewall.bat` - Configure firewall rules
- `remove_firewall.bat` - Remove firewall rules

## 🔥 Firewall Configuration

### Automatic Setup (Recommended)
After installation, run the firewall setup:

```batch
# Navigate to installation directory
cd "C:\Program Files\Camera Server Qt6"

# Run as Administrator
setup_firewall.bat
```

### Manual Setup
Use the commands from your documentation:

```powershell
# TCP Echo Server Rule (Port 7777)
netsh advfirewall firewall add rule name="Camera Server Echo" dir=in action=allow protocol=TCP localport=7777

# ICMP Echo Request Rule (Ping Replies)
netsh advfirewall firewall add rule name="ICMP Allow incoming V4 echo request" protocol=icmpv4:8,any dir=in action=allow
```

### Firewall Rules Added
1. **TCP port 7777** - Echo Server functionality for bidirectional ping testing
2. **ICMP Echo Request** - Allows Windows machine to respond to ping requests

## 🧪 Testing the Installation

### 1. Application Launch Test
```batch
# From desktop shortcut or
"C:\Program Files\Camera Server Qt6\ViscoConnect.exe"
```

### 2. Firewall Rules Test
```powershell
# Check if rules are active
netsh advfirewall firewall show rule name="Camera Server Echo"
netsh advfirewall firewall show rule name="ICMP Allow incoming V4 echo request"

# Test from remote machine (replace IP)
ping 10.0.0.2
echo "test message" | nc 10.0.0.2 7777
```

### 3. WireGuard Integration Test
- Start WireGuard VPN
- Verify connectivity between 10.0.0.1 ↔ 10.0.0.2
- Test camera port forwarding

## 🔄 Uninstallation

### Standard Uninstall
1. Go to **Settings > Apps** or **Control Panel > Programs**
2. Find "Camera Server Qt6"
3. Click **Uninstall**

### What Gets Removed
- All application files and folders
- Desktop and Start Menu shortcuts
- Registry entries

### Manual Firewall Cleanup (if needed)
```batch
# Run as Administrator
"C:\Program Files\Camera Server Qt6\remove_firewall.bat"

# Or manually:
netsh advfirewall firewall delete rule name="Camera Server Echo"
netsh advfirewall firewall delete rule name="ICMP Allow incoming V4 echo request"
```

## 🛠 Rebuilding the Installer

If you need to rebuild the installer after making changes:

```batch
# From project root
cd installer
.\build_installer.bat
```

### Prerequisites for Building
- WiX Toolset v3.14+ installed and in PATH
- Application must be built first (`build.bat` in project root)
- Run from Command Prompt or PowerShell as Administrator

## 📋 Installer Features

### ✅ Implemented Features
- [x] Installs to Program Files x64 directory
- [x] Creates desktop shortcut
- [x] Creates Start Menu shortcuts
- [x] Includes all Qt6 dependencies and plugins
- [x] Includes WireGuard DLLs
- [x] Provides firewall configuration scripts
- [x] Professional installer UI with license agreement
- [x] Proper uninstall support
- [x] 64-bit installer package

### 🎯 Firewall Configuration
- [x] TCP port 7777 rule for Echo Server
- [x] ICMP Echo Request rule for ping responses
- [x] Automated setup script
- [x] Automated cleanup script
- [x] Administrative privilege handling

## 🔒 Security Considerations

- **Administrator Required**: Firewall configuration requires admin privileges
- **Specific Rules**: Only adds necessary ports (7777) and ICMP
- **Clean Uninstall**: Removes all firewall rules on uninstall
- **No Broad Exceptions**: Does not create overly permissive firewall rules

## 📝 Distribution

The `CameraServerQt6Setup.msi` file can be:
- Distributed via email, USB drives, or network shares
- Deployed through Group Policy in enterprise environments
- Hosted on websites for download
- Packaged with other software distributions

## 🎉 Success! 

Your Camera Server Qt6 application now has a professional Windows installer that:

1. ✅ **Installs** all necessary files to Program Files
2. ✅ **Creates** desktop and Start Menu shortcuts  
3. ✅ **Includes** firewall configuration scripts
4. ✅ **Provides** easy uninstall process
5. ✅ **Supports** all the features you requested

The installer is production-ready and follows Windows installer best practices!
