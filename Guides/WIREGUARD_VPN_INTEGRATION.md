# WireGuard VPN Integration for Camera Server Qt6

This document describes the WireGuard VPN functionality integrated into the Camera Server Qt6 application.

## Overview

The Camera Server Qt6 now includes built-in VPN capabilities using WireGuard technology. This allows you to route all camera traffic through a secure VPN tunnel, providing enhanced security and privacy for your IP camera deployments.

## Prerequisites

### WireGuard DLLs Required

You need to obtain the following DLL files and place them in the application directory:

1. **tunnel.dll** - WireGuard tunnel service DLL
2. **wireguard.dll** - WireGuard kernel driver interface DLL

These DLLs are available from the official WireGuard Windows implementation:
- Download from: https://git.zx2c4.com/wireguard-windows/about/
- Or build from source following the embeddable-dll-service instructions

### Windows Administrator Rights

The VPN functionality requires administrator privileges to:
- Install and manage Windows services
- Interface with the WireGuard kernel driver
- Create network adapters

## Features

### Key Generation
- Generate WireGuard public/private key pairs
- Cryptographically secure key generation using WireGuard's native functions

### Configuration Management
- Create, edit, and delete VPN configurations
- Support for multiple VPN profiles
- Import/export configuration files in standard WireGuard format
- Configuration validation and syntax checking

### Connection Management
- Connect/disconnect VPN tunnels
- Real-time connection status monitoring
- Automatic reconnection on connection loss
- Connection statistics (data transfer, uptime)

### GUI Integration
- Dedicated VPN widget in the main application window
- Configuration dialog with tabbed interface (Interface, Peers, Advanced)
- Visual connection status indicators
- Real-time transfer statistics display

## GUI Components

### VPN Widget
Located on the right side of the main window, providing:
- Configuration selection dropdown
- Connect/Disconnect buttons
- Real-time connection status
- Data transfer statistics
- Configuration management buttons

### Configuration Dialog
Comprehensive configuration editor with three tabs:

#### Interface Tab
- Configuration name
- Key generation and management
- IP address and DNS settings
- Listen port configuration

#### Peers Tab
- Add/remove peers
- Peer configuration (public key, endpoint, allowed IPs)
- Keep-alive settings
- Preshared key support

#### Advanced Tab
- Raw configuration text editor
- Configuration templates
- Import/export functionality

## Usage Instructions

### Creating a VPN Configuration

1. Click "Create New" in the VPN widget
2. Fill in the Interface tab:
   - Enter a configuration name (e.g., "MyVPN")
   - Click "Generate" to create a new key pair
   - Set your VPN IP address (e.g., "10.0.0.2/24")
   - Configure DNS servers (optional)

3. Add a peer in the Peers tab:
   - Click "Add" to create a new peer
   - Enter the server's public key
   - Set the endpoint (server:port)
   - Configure allowed IPs (e.g., "0.0.0.0/0" for full tunnel)

4. Click "OK" to save the configuration

### Connecting to VPN

1. Select your configuration from the dropdown
2. Click "Connect"
3. Monitor the connection status and statistics
4. The camera traffic will now be routed through the VPN

### Managing Configurations

- **Edit**: Select a configuration and click "Edit"
- **Delete**: Select a configuration and click "Delete"
- **Refresh**: Click "Refresh" to reload the configurations list

## Technical Implementation

### Architecture

The VPN functionality is implemented through several key components:

#### WireGuardManager Class
- Core VPN management and control
- DLL interface and function loading
- Windows Service integration
- Configuration parsing and validation
- Connection monitoring and statistics

#### WireGuardConfigDialog Class
- Comprehensive configuration editing interface
- Multi-tab design for organized settings
- Real-time configuration validation
- Import/export functionality

#### VpnWidget Class
- Main GUI component for VPN control
- Real-time status updates
- Configuration selection and management
- Integration with main application

### Windows Service Integration

The VPN functionality integrates with Windows Service Control Manager (SCM) to:
- Create ephemeral WireGuard tunnel services
- Start/stop tunnel services on demand
- Monitor service status and health
- Automatically clean up services on disconnection

### Security Considerations

- All private keys are handled securely in memory
- Configuration files are stored in the user's AppData directory
- Administrator privileges are required for service operations
- DLL loading includes security validation

## Configuration File Format

The application uses standard WireGuard configuration format:

```ini
[Interface]
PrivateKey = <base64-encoded-private-key>
Address = 10.0.0.2/24
DNS = 8.8.8.8, 8.8.4.4

[Peer]
PublicKey = <base64-encoded-public-key>
Endpoint = server.example.com:51820
AllowedIPs = 0.0.0.0/0
PersistentKeepalive = 25
```

## Troubleshooting

### Common Issues

1. **DLLs Not Found**
   - Ensure tunnel.dll and wireguard.dll are in the application directory
   - Check that DLLs are the correct architecture (x64)

2. **Access Denied**
   - Run the application as Administrator
   - Check Windows UAC settings

3. **Connection Failed**
   - Verify server endpoint is reachable
   - Check firewall settings
   - Validate configuration syntax

4. **Service Creation Failed**
   - Ensure no conflicting WireGuard services exist
   - Check Windows Service Control Manager permissions

### Logging

The application provides detailed logging of VPN operations:
- Connection attempts and results
- Configuration changes
- Error conditions and troubleshooting information
- Service management operations

Check the application log viewer for VPN-related messages prefixed with "VPN:".

## Integration with Camera System

When a VPN connection is established:
- All camera traffic is automatically routed through the VPN tunnel
- Port forwarding continues to work normally
- Camera discovery may need to account for VPN network ranges
- External access URLs will reflect the VPN's public IP

## Best Practices

1. **Key Management**
   - Generate unique key pairs for each configuration
   - Store private keys securely
   - Regularly rotate keys for enhanced security

2. **Network Configuration**
   - Use appropriate IP ranges that don't conflict with local networks
   - Configure DNS servers for proper name resolution
   - Set appropriate MTU values if needed

3. **Performance**
   - Monitor data transfer statistics
   - Use persistent keepalive for NAT traversal
   - Consider bandwidth limitations

## Future Enhancements

Planned improvements include:
- Automatic server discovery
- Configuration templates for common VPN providers
- Advanced routing options
- Traffic analysis and reporting
- Integration with camera-specific VPN settings

## Support

For issues related to the VPN functionality:
1. Check the application logs
2. Verify DLL availability and compatibility
3. Ensure proper administrator privileges
4. Consult WireGuard documentation for protocol-specific issues
