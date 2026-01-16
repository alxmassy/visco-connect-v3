cmake_minimum_required(VERSION 3.16)
project(ViscoConnect VERSION 2.1.5 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

    # List of potential installation paths for MinGW versions of Qt6
    # You can add your specific path here if it's not listed.
    set(QT6_MINGW_SEARCH_PATHS
        "C:/Qt/6.8.0/mingw_64"
        "C:/Qt/6.7.0/mingw_64"
        "C:/Qt/6.6.0/mingw_64"
        "C:/Qt/6.5.3/mingw_64"
        "C:/Qt/6.5.0/mingw_64"
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

# Link Qt6 libraries
target_link_libraries(ViscoConnect PRIVATE Qt6::Core Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::MultimediaWidgets)

# Link Windows system libraries for WireGuard integration
if(WIN32)
    target_link_libraries(ViscoConnect PRIVATE advapi32 ws2_32)
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
