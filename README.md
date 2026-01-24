# Visco Connect v2.1.5 -> v3.1.7

IP Camera Port Forwarding Application built with Qt 6.5.3 and C++.

## Overview

This application enables IP camera access across VPN networks by providing TCP port forwarding from external ports to camera RTSP streams. Designed for P2P VPN environments where Device A needs to access IP cameras connected to Device B's local network.

## Features

- **RTSP Port Forwarding**: TCP port forwarding from external ports to IP camera RTSP streams (default port 554)
- **Camera Management**: Add, edit, remove, and configure IP cameras with authentication support
- **WireGuard VPN Integration**: Built-in VPN functionality using WireGuard technology for secure camera access
- **VPN Configuration Management**: Create, edit, and manage multiple VPN profiles with GUI interface
- **Real-time VPN Monitoring**: Connection status, transfer statistics, and uptime tracking
- **Auto-start Support**: Option to start with Windows automatically
- **System Tray Integration**: Minimize to system tray with quick access controls
- **Windows Service**: Can run as a background Windows service
- **Comprehensive Logging**: File-based logging system for debugging and monitoring
- **P2P VPN Support**: Designed for VPN environments with P2P connectivity
- **Auto Port Assignment**: External ports auto-assigned starting from 8551 (non-editable)

## Network Architecture

```
Device A (Client) --> VPN --> Device B (Server) --> Local IP Cameras

Device A: 10.0.0.1 (VPN IP)
Device B: 10.0.0.2 (VPN IP)

Camera Access:
Device A -> 10.0.0.2:8551 -> Device B -> 192.168.1.200:554 (Camera 1)
Device A -> 10.0.0.2:8552 -> Device B -> 192.168.1.201:554 (Camera 2)
Device A -> 10.0.0.2:8553 -> Device B -> 192.168.1.202:554 (Camera 3)
```

## Requirements

- **Operating System**: Windows 10/11
- **Qt Framework**: Qt 6.5.3 or later
- **Build Tools**: CMake 3.16 or later
- **Compiler**: Visual Studio 2022 or MinGW-w64
- **Network**: VPN connection with P2P capability
- **For VPN Features**: Administrator privileges and WireGuard DLLs (tunnel.dll, wireguard.dll)

## Installation & Setup

### Quick Build (Recommended)

The easiest way to build the project is using the comprehensive build script:

```batch
build.bat
```

This single command will:
1. **Detect Qt 6.5.3** installation automatically from common locations
2. **Set up environment** variables and compiler paths
3. **Configure CMake** with the appropriate generator
4. **Build the project** in Release mode
5. **Provide detailed feedback** and troubleshooting information

The script supports:
- Multiple Qt installation paths
- Both MinGW and Visual Studio compilers  
- Automatic environment detection
- Registry-based Qt discovery
- Comprehensive error reporting

### Manual Build (Alternative)

If you prefer step-by-step control:

#### Step 1: Install Qt6

1. Download Qt6 from [Qt Official Website](https://www.qt.io/download-qt-installer)
2. Install Qt 6.5.3 or later with the following components:
   - Qt6 Core, Widgets, and Network modules
   - CMake support
   - Your preferred compiler (MSVC 2019/2022 or MinGW recommended)

#### Step 2: Build Project

```batch
build.bat
```

### VPN Setup (Optional but Recommended)

For WireGuard VPN functionality, you need the required DLLs:

```batch
setup_wireguard_dlls.bat
```

This script will:
- Search for existing WireGuard DLLs
- Help you locate and copy the required files
- Provide download instructions if needed

Required files:
- `tunnel.dll` - WireGuard tunnel service
- `wireguard.dll` - WireGuard driver interface

**Note**: VPN features require Administrator privileges.

## Usage

### GUI Mode (Recommended)

Start the application with the graphical interface:
```batch
cd build\Release
ViscoConnect.exe
```

### Service Mode

Install and run as Windows service:
1. Start the GUI application
2. Go to **Service** menu → **Install Service**
3. The service will auto-start with Windows (if enabled)

Or install manually:
```batch
ViscoConnect.exe --service
```

## Camera Configuration

### Adding Cameras

1. Click **"Add Camera"** in the main window
2. Configure camera settings:
   - **Camera Name**: Descriptive name (e.g., "Front Door Camera")
   - **IP Address**: Camera's local IP (e.g., 192.168.1.200)
   - **Port**: Camera RTSP port (usually 554)
   - **Username**: Camera authentication username
   - **Password**: Camera authentication password
   - **Enabled**: Enable/disable the camera service

### External Port Assignment

- External ports are **automatically assigned** starting from **8551**
- Each camera receives a unique external port:
  - Camera 1: 8551
  - Camera 2: 8552 
  - Camera 3: 8553
  - And so on...
- External ports are **not editable** by design

### Managing Cameras

- **Edit**: Double-click a camera row or use "Edit Camera" button
- **Remove**: Select camera and click "Remove Camera"
- **Start/Stop**: Use "Start/Stop" button or "Start/Stop All Cameras"

## Accessing Cameras from Device A

Once cameras are configured and running on Device B, access them from Device A using:

```
rtsp://username:password@<Device_B_VPN_IP>:<External_Port>/path
```

Examples:
```
rtsp://admin:password123@10.0.0.2:8551/stream1
rtsp://admin:password123@10.0.0.2:8552/live
rtsp://admin:password123@10.0.0.2:8553/cam/realmonitor
```

**Note**: The exact RTSP path depends on your camera manufacturer and model.

## Configuration Files

- **Config Location**: `%LOCALAPPDATA%\ViscoConnect\config.json`
- **Log Location**: `%LOCALAPPDATA%\ViscoConnect\visco-connect.log`
- **Example Config**: See `config.example.json` in project root

### API Configuration (SSOT)

The application uses a centralized API configuration system that allows you to easily change the backend REST API base URL from a single location.

**Configuration File Format:**
```json
{
    "autoStart": false,
    "echoServerEnabled": true,
    "echoServerPort": 7777,
    "apiBaseUrl": "http://54.225.63.242:8086",
    "cameras": [...]
}
```

**Changing API Server:**
- Edit `config.json` and modify the `apiBaseUrl` field
- Or use the test scripts: `test_api_config.ps1` (Windows) or `test_api_config.py` (Cross-platform)
- Changes are applied immediately without restarting the application

**Common API Configurations:**
- Production: `http://54.225.63.242:8086` (default)
- Development: `http://localhost:3000`
- Secure: `https://api.yourserver.com:8443`

For detailed API configuration documentation, see `API_CONFIGURATION.md`.

## System Tray Features

When minimized to system tray, access these features:
- **Show Visco Connect**: Restore main window
- **Enable All Cameras**: Start all enabled cameras
- **Disable All Cameras**: Stop all running cameras  
- **Quit**: Exit application completely

## Service Management

### Install Service
1. Open main application
2. **Service** menu → **Install Service**
3. Service will be installed as "Visco Connect Service"

### Uninstall Service
1. **Service** menu → **Uninstall Service**
2. Service will be removed from Windows Services

### Auto-start Configuration
- Enable "Auto-start with Windows" checkbox in main window
- This adds application to Windows startup registry

## Logging System

The application provides four logging levels:

- **DEBUG**: Detailed debugging information
- **INFO**: General operational information  
- **WARNING**: Non-critical issues and warnings
- **ERROR**: Critical errors and failures

Log files are automatically rotated and stored in:
`%LOCALAPPDATA%\ViscoConnect\visco-connect.log`

## Troubleshooting

### Common Issues

1. **Qt6 Not Found**
   - Run `setup.bat` to check Qt6 installation
   - Ensure Qt6 bin directory is in PATH
   - Set `Qt6_DIR` environment variable if needed

2. **Camera Connection Failed**
   - Verify camera IP address and port
   - Check camera credentials (username/password)
   - Ensure camera supports RTSP protocol
   - Test camera accessibility from Device B locally

3. **Port Already in Use**
   - External ports are auto-assigned to avoid conflicts
   - Check Windows Firewall settings
   - Ensure no other applications are using the ports

4. **Service Installation Failed**
   - Run application as Administrator
   - Check Windows Event Logs for service errors
   - Verify antivirus is not blocking the service

### Network Testing

Test camera connectivity from Device B:
```batch
# Test camera reachability
ping 192.168.1.200

# Test RTSP connection (if VLC is installed)
vlc rtsp://admin:password@192.168.1.200:554/stream1
```

Test port forwarding from Device A:
```batch
# Test port connectivity
telnet 10.0.0.2 8551

# Test with media player
vlc rtsp://admin:password@10.0.0.2:8551/stream1
```

## Development

### Building from Source

1. Clone/download the project
2. Install Qt 6.5.3+
3. Run build scripts in order:
   ```batch
   setup.bat
   configure.bat
   build.bat
   ```

### Project Structure

```
camera-server-qt6/
├── include/           # Header files
├── src/              # Source files  
├── ui/               # Qt UI files (future)
├── resources/        # Resources and icons
├── build/            # Build output
├── CMakeLists.txt    # CMake configuration
└── *.bat            # Build scripts
```

### Key Components

- **CameraConfig**: Camera configuration data structure
- **CameraManager**: Core camera management and control  
- **PortForwarder**: TCP port forwarding implementation
- **ConfigManager**: JSON-based configuration management
- **Logger**: Logging system with file output
- **WindowsService**: Windows service integration
- **SystemTrayManager**: System tray functionality
- **MainWindow**: Main GUI interface

## Contact me
**Author:** Shiven Saini<br>
**Email:** [shiven.career@proton.me](mailto:shiven.career@proton.me)


