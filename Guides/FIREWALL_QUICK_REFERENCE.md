# 🔥 Firewall Quick Reference - Camera Server Qt6

## Essential Commands (Run as Administrator)

### Enable Bidirectional Connectivity
```powershell
# 1. Allow TCP EchoServer (Port 7777)
netsh advfirewall firewall add rule name="Camera Server Echo" dir=in action=allow protocol=TCP localport=7777

# 2. Allow ICMP Ping Replies
netsh advfirewall firewall add rule name="ICMP Allow incoming V4 echo request" protocol=icmpv4:8,any dir=in action=allow
```

### Test Connectivity
```bash
# From Remote Server (10.0.0.1) to Camera Server (10.0.0.2)
ping 10.0.0.2                           # ICMP ping test
echo "test" | nc 10.0.0.2 7777         # TCP echo test
```

### Verify Rules
```powershell
# Check if rules are active
netsh advfirewall firewall show rule name="Camera Server Echo"
netsh advfirewall firewall show rule name="ICMP Allow incoming V4 echo request"

# Check port listening
netstat -an | findstr :7777
```

### Remove Rules (if needed)
```powershell
netsh advfirewall firewall delete rule name="Camera Server Echo"
netsh advfirewall firewall delete rule name="ICMP Allow incoming V4 echo request"
```

## ✅ Status: Both rules applied successfully
- **TCP Port 7777**: ✅ Enabled
- **ICMP Ping**: ✅ Enabled  
- **Bidirectional Connectivity**: ✅ Working
