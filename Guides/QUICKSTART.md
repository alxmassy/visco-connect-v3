# Quick Start Guide - Visco Connect v2.1.5

## Prerequisites
1. Windows 10/11
2. Qt 6.5.3+ installed
3. VPN connection between Device A and Device B
4. IP cameras on Device B's local network

## Setup Steps

### 1. First Time Setup
```batch
# Run setup to check Qt6 installation
setup.bat

# Configure build system  
configure.bat

# Build the application
build.bat
```

### 2. Run Application
```batch
# Start the GUI application
run.bat
```

### 3. Configure Cameras

1. **Add Camera**: Click "Add Camera" button
2. **Fill Details**:
   - **Name**: "Front Door Camera"
   - **IP**: 192.168.1.200
   - **Port**: 554
   - **Username**: admin
   - **Password**: your_password
   - **Enabled**: ✓ Checked

3. **Save**: Click OK

### 4. Start Forwarding

1. **Start Single Camera**: Select camera → Click "Start Camera"
2. **Start All**: Click "Start All Cameras"

### 5. Access from Device A

Use any RTSP-compatible player (VLC, etc.):
```
rtsp://admin:password@<Device_B_VPN_IP>:8551/stream_path
```

Example:
```
rtsp://admin:password123@10.0.0.2:8551/live
```

## Port Assignments

| Camera | Local Address | External Port | Access URL |
|--------|---------------|---------------|------------|
| Camera 1 | 192.168.1.200:554 | 8551 | Device_B_IP:8551 |
| Camera 2 | 192.168.1.201:554 | 8552 | Device_B_IP:8552 |
| Camera 3 | 192.168.1.202:554 | 8553 | Device_B_IP:8553 |

## Common RTSP Paths by Manufacturer

- **Hikvision**: `/Streaming/Channels/101`
- **Dahua**: `/cam/realmonitor?channel=1&subtype=0`
- **Axis**: `/axis-media/media.amp`
- **Foscam**: `/videoMain`
- **Generic**: `/stream1` or `/live`

## System Tray Usage

When minimized:
- **Double-click tray icon**: Show main window
- **Right-click tray icon**: Access quick menu
  - Enable All Cameras
  - Disable All Cameras  
  - Quit

## Troubleshooting

### Build Issues
```batch
# Qt6 not found
setup.bat

# Reconfigure if needed
configure.bat
```

### Connection Issues
1. **Test camera locally** on Device B:
   ```
   ping 192.168.1.200
   ```

2. **Test RTSP** (if VLC installed):
   ```
   vlc rtsp://admin:password@192.168.1.200:554/stream1
   ```

3. **Check forwarding** from Device A:
   ```
   telnet Device_B_VPN_IP 8551
   ```

### Service Installation
1. **Install**: Service menu → Install Service
2. **Enable Auto-start**: Check "Auto-start with Windows"
3. **Uninstall**: Service menu → Uninstall Service

## Configuration Locations

- **Config**: `%LOCALAPPDATA%\ViscoConnect\config.json`
- **Logs**: `%LOCALAPPDATA%\ViscoConnect\visco-connect.log`
- **Lock**: `%LOCALAPPDATA%\ViscoConnect\visco-connect.lock`

## Advanced Usage

### Manual Config Edit
Edit `config.json` directly for bulk camera setup:
```json
{
    "autoStart": true,
    "cameras": [
        {
            "name": "Camera 1",
            "ipAddress": "192.168.1.200", 
            "port": 554,
            "username": "admin",
            "password": "password123",
            "enabled": true
        }
    ]
}
```

### Service Mode
```batch
# Install service manually
ViscoConnect.exe --service

# Check service status
sc query ViscoConnectService
```

---

**Need Help?** Check the full README.md for detailed documentation.
