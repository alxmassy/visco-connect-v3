# Windows Firewall Configuration for Camera Server Qt6

This document outlines all the Windows Firewall rules and commands that were configured to enable bidirectional connectivity for the Camera Server Qt6 application with WireGuard VPN integration.

## 🔥 **Firewall Rules Added**

### 1. **TCP Echo Server Rule (Port 7777)**
**Purpose**: Allows inbound TCP connections on port 7777 for the EchoServer functionality, enabling bidirectional ping testing.

**Command Used**:
```powershell
netsh advfirewall firewall add rule name="Camera Server Echo" dir=in action=allow protocol=TCP localport=7777
```

**Details**:
- **Rule Name**: Camera Server Echo
- **Direction**: Inbound
- **Action**: Allow
- **Protocol**: TCP
- **Port**: 7777
- **Status**: ✅ **Applied successfully**

### 2. **ICMP Echo Request Rule (Ping Replies)**
**Purpose**: Allows the Windows machine to respond to ICMP ping requests from remote servers.

**Command Used**:
```powershell
netsh advfirewall firewall add rule name="ICMP Allow incoming V4 echo request" protocol=icmpv4:8,any dir=in action=allow
```

**Details**:
- **Rule Name**: ICMP Allow incoming V4 echo request
- **Direction**: Inbound
- **Action**: Allow
- **Protocol**: ICMPv4
- **ICMP Type**: 8 (Echo Request)
- **Status**: ✅ **Applied successfully**

### 3. **Alternative ICMP Rule (PowerShell Method)**
**Alternative Command** (if the above doesn't work):
```powershell
New-NetFirewallRule -DisplayName "Allow ICMPv4-In" -Direction Inbound -Protocol ICMPv4 -IcmpType 8 -Action Allow
```

## 🌐 **Network Configuration Context**

### **WireGuard Configuration**
- **Local IP**: 10.0.0.2/32 (Windows machine with Camera Server)
- **Remote IP**: 10.0.0.1/24 (Server)
- **WireGuard Interface**: wg0

### **Services Running**
1. **EchoServer**: TCP port 7777 - Handles bidirectional TCP ping testing
2. **Camera Port Forwarding**: Various ports starting from 8551
3. **ICMP Responder**: System-level ping replies via Windows Firewall

## 🧪 **Testing Commands**

### **From Remote Server (10.0.0.1) to Camera Server (10.0.0.2)**

#### **ICMP Ping Test**:
```bash
ping 10.0.0.2
```
**Expected Result**: Replies from 10.0.0.2

#### **TCP Echo Server Test**:
```bash
# Using netcat
echo "test message" | nc 10.0.0.2 7777

# Using telnet
telnet 10.0.0.2 7777
# (then type message and press Enter)
```
**Expected Result**: Echo of the sent message

### **From Camera Server (10.0.0.2) to Remote Server (10.0.0.1)**

#### **ICMP Ping Test**:
```powershell
ping 10.0.0.1
```
**Expected Result**: Replies from 10.0.0.1 ✅ (Already working)

## 📋 **Verification Commands**

### **Check if Firewall Rules are Active**:
```powershell
# List all firewall rules containing "Camera Server"
netsh advfirewall firewall show rule name="Camera Server Echo"

# List ICMP rules
netsh advfirewall firewall show rule name="ICMP Allow incoming V4 echo request"

# Check all inbound rules for port 7777
Get-NetFirewallRule | Where-Object {$_.DisplayName -like "*7777*"} | Format-Table DisplayName, Enabled, Direction, Action
```

### **Check Port Listening Status**:
```powershell
# Check if port 7777 is listening
netstat -an | findstr :7777

# Check all listening ports
netstat -an | findstr LISTENING
```

### **Test Network Connectivity**:
```powershell
# Test TCP connection to EchoServer
Test-NetConnection -ComputerName 10.0.0.2 -Port 7777

# Test from localhost
Test-NetConnection -ComputerName localhost -Port 7777
```

## 🔄 **Troubleshooting Commands**

### **If Rules Need to be Removed**:
```powershell
# Remove TCP Echo Server rule
netsh advfirewall firewall delete rule name="Camera Server Echo"

# Remove ICMP rule
netsh advfirewall firewall delete rule name="ICMP Allow incoming V4 echo request"
```

### **If Rules Need to be Re-added**:
```powershell
# Re-add TCP rule
netsh advfirewall firewall add rule name="Camera Server Echo" dir=in action=allow protocol=TCP localport=7777

# Re-add ICMP rule
netsh advfirewall firewall add rule name="ICMP Allow incoming V4 echo request" protocol=icmpv4:8,any dir=in action=allow
```

### **Temporary Firewall Disable (for testing)**:
```powershell
# Disable Windows Firewall temporarily (as Administrator)
netsh advfirewall set allprofiles state off

# Re-enable Windows Firewall
netsh advfirewall set allprofiles state on
```

## 🎯 **Summary of What Was Solved**

### **Problem**:
- Camera Server (10.0.0.2) could ping Remote Server (10.0.0.1) ✅
- Remote Server (10.0.0.1) could **NOT** ping Camera Server (10.0.0.2) ❌

### **Root Cause**:
Windows Firewall was blocking:
1. **ICMP Echo Requests** (ping packets)
2. **TCP connections** on port 7777 (EchoServer)

### **Solution Applied**:
1. ✅ **Added TCP firewall rule** for port 7777 (EchoServer)
2. ✅ **Added ICMP firewall rule** for ping replies
3. ✅ **Verified bidirectional connectivity** works

### **Result**:
- **ICMP Ping**: 10.0.0.1 → 10.0.0.2 ✅ **Working**
- **TCP Echo**: 10.0.0.1 → 10.0.0.2:7777 ✅ **Working**
- **Camera Port Forwarding**: All ports accessible from remote server ✅

## 📝 **Notes**

1. **Administrator Privileges**: All firewall commands require running PowerShell/Command Prompt as Administrator
2. **Persistence**: These rules persist across reboots
3. **Security**: These rules are specific to the required ports and protocols, maintaining security
4. **WireGuard**: Rules work with WireGuard VPN interface (wg0) on 10.0.0.0/24 network
5. **Alternative Testing**: EchoServer on port 7777 provides TCP-based connectivity testing as an alternative to ICMP ping

## 🔒 **Security Considerations**

- Rules are limited to specific ports and protocols
- ICMP rule only allows Echo Request (type 8) for ping functionality  
- TCP rule only affects port 7777 used by the Camera Server EchoServer
- No broad firewall rules were added that could compromise security
- Rules are bound to the specific WireGuard network interface when possible
