<!-- Use this file to provide workspace-specific custom instructions to Copilot. For more details, visit https://code.visualstudio.com/docs/copilot/copilot-customization#_use-a-githubcopilotinstructionsmd-file -->

# Camera Server Qt6 Project Instructions

This is a C++ Qt 6.5.3 application for IP Camera port forwarding on Windows.

## Project Structure
- `src/`: Source files (.cpp)
- `include/`: Header files (.h)
- `ui/`: Qt UI files (.ui) 
- `resources/`: Resource files and icons
- Uses CMake build system

## Key Components
- **CameraConfig**: Camera configuration data structure
- **CameraManager**: Core camera management and control
- **PortForwarder**: TCP port forwarding implementation
- **ConfigManager**: JSON-based configuration management
- **Logger**: Logging system with file output
- **WindowsService**: Windows service integration
- **SystemTrayManager**: System tray functionality
- **MainWindow**: Main GUI interface

## Development Guidelines
- Follow Qt naming conventions (camelCase for methods, PascalCase for classes)
- Use Qt's signal/slot mechanism for communication between components
- Implement proper error handling and logging
- Use Qt's JSON classes for configuration serialization
- Follow RAII principles for resource management
- Use Qt's network classes for TCP operations

## Key Features
- RTSP camera port forwarding
- Windows service support
- System tray integration
- Auto-start with Windows
- JSON configuration storage
- Comprehensive logging
- Camera enable/disable functionality
- Automatic port assignment (starting from 8551)

## Build Requirements
- Qt 6.5.3 with Core, Widgets, and Network modules
- CMake 3.16+
- Windows SDK for service functionality
- C++17 standard
