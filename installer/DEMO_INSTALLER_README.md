# Visco Connect Demo build v2.1.5 - Installer Documentation

## 🎉 Demo Build Installer Successfully Created!

Your **Visco Connect Demo build v2.1.5** WiX installer has been successfully built and is ready for distribution.

## 📁 Generated Files

### Main Installer
- **File**: `installer\output\ViscoConnectDemo_v2.1.5_Setup.msi`
- **Version**: 2.1.5 (Demo Build)
- **Type**: 64-bit Windows Installer Package
- **Target**: `C:\Program Files\Visco Connect Demo\`

## 🆕 Version 2.1.5 Demo Build Changes

### Updated Branding
- ✅ **Product Name**: "Visco Connect Demo build v2.1.5"
- ✅ **Manufacturer**: "Visco Technologies"
- ✅ **Version**: 2.1.5.0
- ✅ **Installation Directory**: `C:\Program Files\Visco Connect Demo\`

### Demo Build Features
- ✅ **Demo Notice**: Prominent demo build notification in license agreement
- ✅ **Evaluation Purpose**: Clearly marked as evaluation/testing version
- ✅ **Limited Support**: Demo version limitations explained
- ✅ **Demo Branding**: All shortcuts and references updated to "Visco Connect Demo"

## 🚀 Installation Instructions

### For End Users

1. **Download**: `ViscoConnectDemo_v2.1.5_Setup.msi`
2. **Run as Administrator**: Right-click and select "Run as administrator"
3. **Accept License**: Read the demo build notice and license agreement
4. **Install**: Choose installation directory (default recommended)
5. **Configure Firewall**: Run firewall setup after installation (optional)

### What Gets Installed

✅ **Demo Application**:
- `ViscoConnect.exe` - Main application (Demo version)
- Qt6 runtime libraries and dependencies
- WireGuard integration DLLs
- Demo configuration files

✅ **Demo Shortcuts**:
- Desktop shortcut: "Visco Connect Demo"
- Start Menu folder: "Visco Connect Demo"
- Uninstall shortcut: "Uninstall Visco Connect Demo"

✅ **Firewall Scripts**:
- `setup_firewall.bat` - Configure firewall for demo
- `remove_firewall.bat` - Remove firewall rules

## 🔥 Firewall Configuration

### Automatic Setup (Recommended)
```batch
# Navigate to installation directory
cd "C:\Program Files\Visco Connect Demo"

# Run as Administrator
setup_firewall.bat
```

### Demo Firewall Rules
1. **TCP port 7777** - "Visco Connect Demo Echo" rule
2. **ICMP Echo Request** - For ping responses

## 🎯 Demo Build Specific Features

### License Agreement Updates
- **Demo Notice**: Clear indication this is a demonstration version
- **Evaluation Purpose**: Explains intended use for testing/evaluation
- **Limitations**: Notes potential functionality or time restrictions
- **Support Notice**: Limited technical support for demo versions

### Registry Keys
- **Demo Registry Path**: `HKCU\Software\ViscoConnectDemo`
- **Separate from Production**: Won't conflict with full version installs

## 🧪 Testing the Demo Installation

### 1. Demo Application Launch
```batch
# From desktop shortcut or
"C:\Program Files\Visco Connect Demo\ViscoConnect.exe"
```

### 2. Demo Firewall Rules
```powershell
# Check demo-specific rules
netsh advfirewall firewall show rule name="Visco Connect Demo Echo"
netsh advfirewall firewall show rule name="ICMP Allow incoming V4 echo request"
```

### 3. Demo Functionality Test
- Verify demo branding appears in application
- Test camera port forwarding capabilities
- Validate WireGuard VPN integration

## 🔄 Uninstalling Demo Version

### Standard Uninstall
1. **Settings > Apps** → Find "Visco Connect Demo build v2.1.5"
2. Click **Uninstall**
3. Firewall rules automatically removed

### Manual Cleanup (if needed)
```batch
"C:\Program Files\Visco Connect Demo\remove_firewall.bat"
```

## 🛠 Rebuilding the Demo Installer

```batch
# From installer directory
cd c:\Users\shiven\camera-server-qt6\installer
.\build_installer.bat
```

## 📋 Demo Version Summary

### ✅ Demo Build Features
- [x] Clear demo version identification
- [x] Updated branding throughout installer
- [x] Demo-specific license agreement
- [x] Separate installation path
- [x] Demo firewall rule naming
- [x] Evaluation purpose documentation

### 🎯 Distribution Ready
- **File Size**: ~26MB
- **Platform**: Windows x64
- **Requirements**: Administrator privileges for firewall
- **Compatibility**: Windows 10/11 with Qt6 support

## 🔒 Demo Security Considerations

- **Limited Scope**: Demo version clearly identified
- **Firewall Rules**: Same security level as production
- **Evaluation Only**: Not recommended for production use
- **Clean Uninstall**: Complete removal of all demo components

## 🎉 Demo Version Success!

Your **Visco Connect Demo build v2.1.5** installer is now ready for:

1. ✅ **Demo Distribution** - Share with potential customers
2. ✅ **Evaluation Testing** - Allow users to test functionality  
3. ✅ **Proof of Concept** - Demonstrate capabilities
4. ✅ **Sales Support** - Technical demonstrations

The demo installer provides a professional evaluation experience while clearly identifying it as a demonstration version!
