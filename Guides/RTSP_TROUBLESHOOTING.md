# RTSP Port Forwarding Troubleshooting Guide

## Overview
This guide helps diagnose and fix RTSP streaming issues with the Camera Server Qt6 port forwarding application.

## Common Issues and Solutions

### 1. **Connection Established but No Video Stream**

**Symptoms:**
- TCP connection to port 8551 (or other external port) succeeds
- No video stream appears in RTSP client (VLC, etc.)
- Port forwarding shows data transfer but stream doesn't work

**Possible Causes & Fixes:**

#### A. Incorrect RTSP Path
```bash
# Test different RTSP paths based on camera brand:

# Hikvision cameras
rtsp://username:password@10.0.0.2:8551/Streaming/Channels/101
rtsp://username:password@10.0.0.2:8551/Streaming/Channels/102

# Dahua/CP Plus cameras  
rtsp://username:password@10.0.0.2:8551/cam/realmonitor?channel=1&subtype=0
rtsp://username:password@10.0.0.2:8551/cam/realmonitor?channel=1&subtype=1

# Generic cameras
rtsp://username:password@10.0.0.2:8551/stream1
rtsp://username:password@10.0.0.2:8551/live
rtsp://username:password@10.0.0.2:8551/video1
```

#### B. Authentication Issues
```bash
# Test without credentials first
rtsp://10.0.0.2:8551/stream1

# Then with credentials
rtsp://admin:password@10.0.0.2:8551/stream1

# URL encode special characters in password
# @ becomes %40, : becomes %3A, etc.
```

#### C. Camera Not Accessible from Server
```powershell
# From the Camera Server machine, test direct camera access:
ping 192.168.88.200
telnet 192.168.88.200 554

# Test RTSP directly (if VLC installed)
vlc rtsp://admin:password@192.168.88.200:554/stream1
```

### 2. **TCP Connection Fails**

**Symptoms:**
- Cannot connect to external port (8551, etc.)
- "Connection refused" or "Connection timeout" errors

**Fixes:**

#### A. Check Port Forwarding Status
```cpp
// In application logs, look for:
"Successfully bound to all IPv4 interfaces (0.0.0.0:8551)"
// OR
"Successfully bound to all IPv6 interfaces ([::]:8551)"
```

#### B. Firewall Issues
```powershell
# Add Windows Firewall rule for external port
netsh advfirewall firewall add rule name="Camera Server Port 8551" dir=in action=allow protocol=TCP localport=8551

# Check if port is listening
netstat -an | findstr :8551
```

#### C. Interface Binding Issues
The updated code now tries both IPv4 (0.0.0.0) and IPv6 (::) binding.

### 3. **Data Transfer but Stream Corruption**

**Symptoms:**
- Connection works, data is being transferred
- Video is choppy, freezes, or has artifacts

**Fixes:**

#### A. Socket Optimization (Now Implemented)
The code now includes:
- `TCP_NODELAY` for real-time streaming
- `SO_KEEPALIVE` for connection stability  
- Larger buffer sizes (128KB) for streaming
- Proper data flushing

#### B. Network Issues
```bash
# Test network stability between client and server
ping -t 10.0.0.2  # Continuous ping test
iperf3 -c 10.0.0.2 -p 5201  # Bandwidth test
```

### 4. **Multiple Clients Issues**

**Symptoms:**
- First client works, additional clients fail
- Stream quality degrades with multiple viewers

**Notes:**
- Each client gets its own TCP connection to the camera
- Camera may have connection limits
- Network bandwidth may be insufficient

## Testing Commands

### Basic Connectivity Test
```powershell
# Test TCP connection
Test-NetConnection -ComputerName 10.0.0.2 -Port 8551

# Test with telnet
telnet 10.0.0.2 8551
```

### RTSP Client Testing
```bash
# VLC command line
vlc rtsp://admin:password@10.0.0.2:8551/stream1

# FFmpeg test
ffmpeg -i rtsp://admin:password@10.0.0.2:8551/stream1 -f null -

# Python test script (included in project)
python test_rtsp_forwarding.py 10.0.0.2 8551 /stream1
```

## Debugging Tips

### 1. Enable Verbose Logging
- Check application logs in `%LOCALAPPDATA%\ViscoConnect\visco-connect.log`
- Look for RTSP protocol messages in logs

### 2. Network Packet Capture
```powershell
# Use Wireshark to capture traffic on port 8551
# Look for:
# - TCP connection establishment
# - RTSP protocol messages
# - RTP media packets
```

### 3. Camera Direct Testing
Always test the camera directly from the server machine first:
```bash
# Direct camera access (from Camera Server machine)
rtsp://admin:password@192.168.88.200:554/stream1

# Port forwarded access (from client machine)  
rtsp://admin:password@10.0.0.2:8551/stream1
```

## Improvements Made

### Socket Optimization
- Added `TCP_NODELAY` for low latency
- Added `SO_KEEPALIVE` for connection stability
- Increased buffer sizes to 128KB
- Added proper data flushing

### Better Data Handling
- Improved partial write handling
- Added RTSP protocol detection in logs
- Better error handling for streaming data
- Connection timeout handling (10 seconds)

### Enhanced Binding
- Now tries both IPv4 (0.0.0.0) and IPv6 (::)
- Better interface selection fallback
- Improved logging for binding status

### Connection Management
- Better connection timeout handling
- Improved state checking
- Enhanced error reporting

## Still Not Working?

If issues persist after trying the above:

1. **Check Camera Compatibility**
   - Some cameras use proprietary protocols
   - Try different RTSP clients (VLC, FFmpeg, etc.)

2. **Network Configuration**  
   - Ensure VPN routing is correct
   - Check MTU sizes for large video packets
   - Verify no QoS restrictions

3. **Camera Settings**
   - Check camera's network settings
   - Verify RTSP is enabled
   - Try different video codecs/resolutions

4. **Contact Support**
   - Provide full logs from `%LOCALAPPDATA%\ViscoConnect\`
   - Include camera model and network topology
   - Test results from troubleshooting steps
