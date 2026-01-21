# MTU Configuration for Video Streaming Over VPN

## Overview

**Packet Fragmentation** is the most common cause of stuttering video over a VPN. When video packets are too large for the tunnel, they get split into smaller pieces. If even one fragment is lost during transmission, the entire video frame fails to decode, resulting in stuttering, freezing, or dropped frames.

## Solution: Set MTU to 1280

Setting the Maximum Transmission Unit (MTU) to 1280 bytes prevents large video packets from being fragmented when transmitted through the VPN tunnel.

## MTU Implementation in Visco PC

The application now includes automatic MTU configuration support:

### Default MTU Setting
- **Default value**: 1280 bytes
- **Minimum value**: 1280 bytes
- **Maximum value**: 65535 bytes

### How MTU is Applied

1. **WireGuard Configuration Interface Section**
   ```
   [Interface]
   PrivateKey = xxx...
   Address = 10.0.0.2/24
   DNS = 1.1.1.1
   MTU = 1280
   ```

2. **Configuration Flow**
   - When you create or edit a WireGuard configuration in the GUI, you'll see an "MTU Size" field
   - The MTU value is stored in the WireGuard configuration file
   - WireGuard automatically applies this MTU setting to the tunnel interface

### Setting MTU via GUI

1. Open Visco Connect
2. Navigate to VPN/WireGuard Configuration
3. In the "Network Configuration" section, you'll see the "MTU Size" field
4. Default is set to **1280** (recommended for video)
5. Adjust if needed, then save the configuration

### Setting MTU via Configuration File

Edit your WireGuard `.conf` file and add the MTU line:

```
[Interface]
PrivateKey = sMNSaBBpJ...
Address = 10.0.0.2/24
DNS = 1.1.1.1
MTU = 1280

[Peer]
PublicKey = HIJ...
Endpoint = vpn.example.com:51820
AllowedIPs = 0.0.0.0/0
```

## Why 1280?

- **1280 bytes** is a safe default that prevents fragmentation over most VPN tunnels
- **Typical video packet**: 1024-2048 bytes
- **WireGuard overhead**: ~80 bytes per packet
- **1280 - 80 = 1200 bytes** available for video data, preventing fragmentation

## Automatic MTU Calculation

The recommended MTU depends on your network setup:

```
Optimal MTU = Physical Network MTU - VPN Overhead
            = 1500 (standard Ethernet) - 80 (WireGuard) - 20-40 (IP headers)
            = 1360-1400 bytes (approximately)
```

However, **1280 is universally safe** and recommended for video streaming.

## Troubleshooting

### If Video Still Stutters with MTU 1280

1. **Check tunnel is active**
   - Verify VPN connection is established
   - Check WireGuard logs for errors

2. **Try smaller MTU values**
   - Try 1200, 1000, or 512 bytes
   - Lower MTU may reduce packet loss at cost of bandwidth efficiency

3. **Monitor packet loss**
   - Use network monitoring tools to check for dropped packets
   - High loss indicates other network issues beyond MTU

4. **Check camera bandwidth**
   - Ensure camera isn't sending unreasonably large packets
   - Some cameras support quality/resolution adjustment

### How to Verify MTU is Applied

**Windows (PowerShell):**
```powershell
# List network interfaces and their MTU
Get-NetAdapter | Select-Object Name, InterfaceDescription, MTUSize

# For WireGuard interface specifically:
Get-NetAdapter | Where-Object {$_.Name -like "*WireGuard*"} | Select-Object MTUSize
```

**Linux:**
```bash
# Check WireGuard interface MTU
ip link show wg0 | grep mtu
```

## Additional Performance Tips

1. **Reduce video resolution** - Lower bandwidth requirements
2. **Use hardware encoding** - Reduce CPU load on camera
3. **Enable persistent keepalive** - Maintain tunnel stability (set to 25 seconds)
4. **Use dedicated network** - Separate video traffic from other applications
5. **Monitor buffer levels** - Check for encoder/decoder buffer overflows

## References

- [WireGuard Documentation](https://www.wireguard.com/)
- [RFC 791 - Internet Protocol](https://tools.ietf.org/html/rfc791)
- [MTU and Path Discovery](https://en.wikipedia.org/wiki/Path_MTU_Discovery)
