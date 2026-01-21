# PersistentKeepalive Configuration for NAT Traversal

## Overview

**PersistentKeepalive** is a WireGuard feature that sends periodic "heartbeat" packets to keep the VPN tunnel connection alive through NAT firewalls. This is critical for Windows clients behind home/office routers.

**Note on Protocols:**
- **WireGuard tunnel itself**: Uses UDP for the VPN connection
- **Camera streaming over tunnel**: Uses TCP (RTSP) for video packets  
- **PersistentKeepalive**: Keeps the UDP tunnel active, allowing TCP streams to flow through

## The Problem: NAT Timeout

### What Happens Without PersistentKeepalive

1. Your Windows PC establishes UDP tunnel to VPN server (WireGuard)
2. Your router creates a NAT mapping for this UDP connection
3. TCP video stream flows through the tunnel successfully
4. **Stream pauses briefly** (camera offline, network hiccup, etc.)
5. Router's NAT table forgets the UDP mapping after timeout (typically 30-60 seconds)
6. Next TCP video packet tries to reach the server → router doesn't know where to send it
7. UDP tunnel is effectively dead → Connection fails → "Connection retry" messages in UI
8. User must manually reconnect

### Result
- Stuttering video
- Frequent disconnections
- Poor user experience
- Constant "retry" notifications

## The Solution: PersistentKeepalive = 25

### How It Works

**PersistentKeepalive = 25** means:
- Send a tiny UDP keepalive packet every 25 seconds
- Each packet "pings" the server to confirm the UDP tunnel is active
- Router sees ongoing UDP traffic → keeps NAT hole open
- UDP tunnel never times out
- TCP video streams flow smoothly through the tunnel, even with brief pauses

### Benefits

✅ **Prevents NAT Timeout** - Connection stays alive indefinitely
✅ **Smooth Streaming** - No freezes or disconnections
✅ **Reduced Retries** - No more "connection retry" messages
✅ **Better UX** - Users don't see network instability
✅ **Minimal Overhead** - 25 seconds is optimal (tiny packets)

## Implementation in Visco PC

### Default Settings

- **Default PersistentKeepalive**: 25 seconds (enabled by default)
- **Previous behavior**: 0 seconds (disabled, causing issues)
- **New behavior**: Automatically applied to all new configurations

### Where It's Used

```
[Interface]
PrivateKey = xxx...
Address = 10.0.0.2/24
MTU = 1280

[Peer]
PublicKey = yyy...
Endpoint = vpn.example.com:51820
AllowedIPs = 0.0.0.0/0
PersistentKeepalive = 25  ← NEW DEFAULT
```

### GUI Control

In ViscoConnect settings:
1. Navigate to VPN Configuration
2. "Keep Alive:" field shows **25 seconds** by default
3. Can be adjusted 0-65535 seconds
4. 0 = Disabled
5. Recommended: 25 (optimal balance)

## Why 25 Seconds?

| Value | Behavior | Use Case |
|-------|----------|----------|
| **0** | Disabled (old default) | ❌ Server-only mode (behind firewall) |
| **5-10** | Very aggressive | ❌ Excessive overhead, wastes bandwidth |
| **25** | **OPTIMAL** ✅ | ✅ Perfect for client behind NAT |
| **60** | Relaxed | ⚠️ May timeout on some routers |
| **120+** | Very relaxed | ❌ Risk of NAT table eviction |

### Calculation

WireGuard UDP keepalive packet size: ~20-30 bytes  
Frequency: Every 25 seconds  
Bandwidth cost: ~96 bytes per minute = negligible

Meanwhile NAT table entries typically expire in 30-300 seconds depending on router. 25-second UDP keepalive safely keeps the hole open for TCP video traffic to flow through.

## Common NAT Router Timeouts

| Router Type | Timeout | Recommended | Notes |
|-------------|---------|-------------|-------|
| Home WiFi | 30-60s | 25s | Consumer routers |
| Enterprise | 60-120s | 25s | Business equipment |
| ISP NAT | 300s | 25s | Carrier NAT |
| 4G/LTE | 30-60s | 25s | Mobile networks |

Setting to 25 seconds is universally safe.

## Testing PersistentKeepalive

### Verify It's Working

**Windows (PowerShell):**
```powershell
# Monitor WireGuard tunnel for UDP keepalive packets
# Use Wireshark to capture traffic on WireGuard port

# Filter: udp.port == 51820
# Look for regular UDP packets every ~25 seconds even when idle
# These are the keepalive packets maintaining the tunnel

# Also monitor for TCP traffic on the tunnel
# Filter: tcp.port == (your external camera port)
# TCP video packets should flow through consistently
```

**Linux:**
```bash
# Monitor WireGuard stats
wg show wg0

# Should show continuous RX/TX even when idle
# (due to keepalive packets)
```

### What to Look For

1. **Before Setting Keepalive (0 seconds)**
   - No UDP traffic on port 51820 when idle
   - UDP connection drops after router timeout (~30-60s)
   - TCP video streams fail when UDP tunnel dies
   - Manual reconnect needed

2. **After Setting Keepalive (25 seconds)**
   - Regular UDP packets every 25 seconds on port 51820
   - UDP tunnel stays alive indefinitely
   - TCP video packets flow through consistently
   - No disconnections

## Troubleshooting

### Video Still Disconnects

**Check current keepalive:**
1. Open ViscoConnect settings
2. Look at "Keep Alive:" value
3. Should show "25 seconds" (not "Disabled")
4. If 0, change to 25 and save

**If still disconnecting:**
1. Reduce to 10 seconds (more aggressive)
2. Check router logs for NAT timeout settings
3. Try connecting from different network
4. Check firewall rules

### High Network Usage

**If data usage seems high:**
1. Check if PersistentKeepalive is too aggressive (e.g., 5 seconds)
2. Increase to 25-30 seconds
3. Monitor data usage again
4. Normal overhead: ~3.5 KB per hour per connection

### Disconnects After Specific Time

**If disconnects after ~60 seconds:**
- Your router timeout is 60 seconds
- Reduce PersistentKeepalive to 20-25 seconds
- Usually solves the problem

## Best Practices

1. **Always enable** for client connections (NAT behind router)
2. **Default to 25** for new configurations
3. **Adjust only if needed** (connection issues)
4. **Never set to 0** unless it's server-only mode
5. **Test with video** before deploying

## Comparison: Before vs After

### Before (PersistentKeepalive = 0)

```
Time 0:00 → UDP tunnel established ✓
Time 0:30 → TCP video playing smoothly ✓
Time 1:00 → 30s pause (camera offline briefly)
        → Router NAT table entry expires ✗
Time 1:05 → TCP video packet arrives
        → Router: "Unknown UDP tunnel" ✗
        → Cannot forward TCP packet ✗
        → Connection fails ✗
        → UI shows "retry" ✗
Time 1:15 → User manually reconnects ✗
```

### After (PersistentKeepalive = 25)

```
Time 0:00 → UDP tunnel established ✓
Time 0:25 → UDP keepalive sent ✓
Time 0:50 → UDP keepalive sent ✓
Time 1:00 → TCP video paused (camera offline)
Time 1:15 → UDP keepalive sent ✓
        → Router NAT entry refreshed ✓
        → UDP tunnel alive ✓
Time 1:20 → TCP video resumes
        → Seamless, no reconnect needed ✓
```

## Integration with Other Settings

### With MTU = 1280
- **MTU (1280 bytes)**: Prevents TCP video packet fragmentation over the UDP tunnel
- **PersistentKeepalive (25s)**: Keeps the UDP tunnel active
- **Together**: Smooth, reliable video streaming ✓

### With Firewall Rules
- **Firewall**: Allows UDP port 51820 (WireGuard tunnel)
- **PersistentKeepalive**: Keeps UDP hole open
- **Result**: TCP traffic can flow through indefinitely ✓

## References

- [WireGuard Documentation - PersistentKeepalive](https://www.wireguard.com/quickstart/)
- [RFC 3235 - Network Address Translation-Protocol Translation](https://tools.ietf.org/html/rfc3235)
- [NAT Timeout Reference](https://www.behavior.org/resources/internet-addresses-nat/)
