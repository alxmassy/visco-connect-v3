cmake_minimum_required(VERSION 3.16)
project(ViscoConnect VERSION 3.1.7 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific definitions to prevent WinSock conflicts
if(WIN32)
    # Set Windows SDK to use x64 libraries (not x86)
    if(NOT DEFINED CMAKE_VS_PLATFORM_NAME)
        set(CMAKE_VS_PLATFORM_NAME x64)
    endif()
    
    # Force 64-bit library paths for Windows SDK (UM libs)
    set(WINDOWS_SDK_PATH "C:/Program Files (x86)/Windows Kits/10/lib/10.0.26100.0/um/x64")
    if(EXISTS "${WINDOWS_SDK_PATH}")
        link_directories("${WINDOWS_SDK_PATH}")
    endif()
    
    # Force 64-bit UCRT (Universal C Runtime) library path
    set(WINDOWS_UCRT_PATH "C:/Program Files (x86)/Windows Kits/10/lib/10.0.26100.0/ucrt/x64")
    if(EXISTS "${WINDOWS_UCRT_PATH}")
        link_directories("${WINDOWS_UCRT_PATH}")
    endif()
    
    # Force 64-bit MSVC runtime libs
    set(MSVC_RUNTIME_LIB_PATH "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.50.35717/lib/x64")
    if(EXISTS "${MSVC_RUNTIME_LIB_PATH}")
        link_directories("${MSVC_RUNTIME_LIB_PATH}")
    endif()
    
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN  # Exclude rarely-used Windows headers (including old winsock.h)
        NOMINMAX             # Prevent Windows.h from defining min/max macros
        _WIN32_WINNT=0x0601  # Target Windows 7+ for modern APIs
    )
endif()

# --- Find Qt6 package (modified for MinGW) ---

# This makes CMake automatically handle MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Honor an explicit Qt6_DIR from environment first
if(DEFINED ENV{Qt6_DIR} AND EXISTS "$ENV{Qt6_DIR}")
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{Qt6_DIR}")
endif()

# Attempt to find Qt6 quietly; fallback logic below will emit messages
find_package(Qt6 COMPONENTS Core Widgets Network Multimedia MultimediaWidgets QUIET)

# If Qt6 was not found automatically, search for it in common MinGW locations
if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found automatically. Searching common MinGW directories...")

    # List of potential installation paths for MinGW/MSVC versions of Qt6
    # You can add your specific path here if it's not listed.
    set(QT6_MINGW_SEARCH_PATHS
        "C:/Qt/6.8.0/mingw_64"
        "C:/Qt/6.7.0/mingw_64"
        "C:/Qt/6.6.0/mingw_64"
        "C:/Qt/6.5.3/mingw_64"
        "C:/Qt/6.5.0/mingw_64"
        "C:/Qt/6.8.0/msvc2019_64"
        "C:/Qt/6.7.0/msvc2019_64"
        "C:/Qt/6.6.0/msvc2019_64"
        "C:/Qt/6.5.3/msvc2019_64"
        "C:/Qt/6.5.0/msvc2019_64"
        "C:/Qt6"
    )

    # Search the paths
    foreach(QT_PATH ${QT6_MINGW_SEARCH_PATHS})
        if(EXISTS "${QT_PATH}/lib/cmake/Qt6")
            message(STATUS "Found potential Qt6 MinGW installation at: ${QT_PATH}")
            # Prepend the path to CMAKE_PREFIX_PATH to give it priority
            set(CMAKE_PREFIX_PATH "${QT_PATH}" ${CMAKE_PREFIX_PATH})
            
            # Try to find the package again with the new hint
            find_package(Qt6 COMPONENTS Core Widgets Network Multimedia MultimediaWidgets)
            
            if(Qt6_FOUND)
                break() # Exit the loop if Qt is found
            endif()
        endif()
    endforeach()
endif()

# If Qt6 is still not found after searching, fail with a clear error
if(NOT Qt6_FOUND)
    message(FATAL_ERROR 
        "Qt6 for MinGW was not found.\n"
        "Please ensure Qt for MinGW is installed and that the CMAKE_PREFIX_PATH variable is set correctly.\n"
        "Example of how to run CMake with the correct path:\n"
        "cmake -S . -B build -G \"MinGW Makefiles\" -DCMAKE_PREFIX_PATH=C:/Qt/6.5.3/mingw_64"
    )
endif()

message(STATUS "Successfully found Qt version: ${Qt6_VERSION}")
message(STATUS "Using Qt from path: ${Qt6_DIR}")

# --- Project Sources and Executable ---

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CameraConfig.cpp
    src/CameraManager.cpp
    src/CameraApiService.cpp
    src/CameraPreviewWidget.cpp
    src/PortForwarder.cpp
    src/WindowsService.cpp
    src/SystemTrayManager.cpp
    src/Logger.cpp
    src/ConfigManager.cpp
    src/CameraDiscovery.cpp
    src/WireGuardManager.cpp    src/WireGuardConfigDialog.cpp
    src/AuthDialog.cpp
    src/VpnWidget.cpp
    src/UserProfileWidget.cpp
    src/NetworkInterfaceManager.cpp
    src/EchoServer.cpp
    src/PingResponder.cpp
    src/FirewallManager.cpp
)

# Header files
set(HEADERS
    include/MainWindow.h
    include/CameraConfig.h
    include/CameraManager.h
    include/CameraApiService.h
    include/CameraPreviewWidget.h
    include/PortForwarder.h
    include/WindowsService.h
    include/SystemTrayManager.h
    include/Logger.h
    include/ConfigManager.h
    include/CameraDiscovery.h
    include/WireGuardManager.h    include/WireGuardConfigDialog.h
    include/AuthDialog.h
    include/VpnWidget.h
    include/UserProfileWidget.h
    include/NetworkInterfaceManager.h
    include/EchoServer.h
    include/PingResponder.h
    include/FirewallManager.h
)

# Resource files
set(RESOURCES
    resources/resources.qrc
)

# Windows-specific resources (manifest, icons, etc.)
if(WIN32)
    set(WIN32_RESOURCES
        resources/app.rc
    )
endif()

# Include directories
include_directories(include)

# Create executable (WIN32 suppresses console window)
add_executable(ViscoConnect WIN32 ${SOURCES} ${HEADERS} ${RESOURCES} ${WIN32_RESOURCES})

# Set the manifest file for Visual Studio generator
set_property(TARGET ViscoConnect PROPERTY VS_USER_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.manifest")

# Link Qt6 libraries
target_link_libraries(ViscoConnect PRIVATE Qt6::Core Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::MultimediaWidgets)

# Link Windows system libraries for WireGuard integration
if(WIN32)
    # Link to all required Windows libraries
    target_link_libraries(ViscoConnect PRIVATE 
        advapi32 ws2_32 kernel32 user32 gdi32 shell32
        ole32 oleaut32 uuid comdlg32
        iphlpapi mpr userenv d3d11 dxgi dxguid winspool
    )
    
    # Force Administrator privileges for the executable
    target_link_options(ViscoConnect PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()

# Copy Qt6 DLLs to the output directory using windeployqt (Corrected Version)
if(WIN32)
    # Find the windeployqt executable reliably
    find_program(
        WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS ${Qt6_DIR}/../../../bin # Qt6_DIR is set by find_package(Qt6)
        REQUIRED
    )

    message(STATUS "Found windeployqt at: ${WINDEPLOYQT_EXECUTABLE}")    # Add the post-build command to run windeployqt
    add_custom_command(TARGET ViscoConnect POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:ViscoConnect>
        COMMENT "Deploying Qt DLLs to the output directory..."
        VERBATIM
    )
    
    # Copy WireGuard DLLs to output directory
    add_custom_command(TARGET ViscoConnect POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/tunnel.dll"
            "${CMAKE_BINARY_DIR}/bin/tunnel.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/wireguard.dll"
            "${CMAKE_BINARY_DIR}/bin/wireguard.dll"
        COMMENT "Copying WireGuard DLLs to output directory..."
        VERBATIM
    )
endif()


# Set output directory
set_target_properties(ViscoConnect PROPERTIES
    OUTPUT_NAME "Visco Connect"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
