@echo off
setlocal enabledelayedexpansion

echo ================================================================
echo       Visco Connect - Enhanced Build Script v2.0
echo ================================================================
echo.

REM Parse command line arguments
set "BUILD_TYPE=Release"
set "CLEAN_BUILD=0"
set "RUN_AFTER_BUILD=0"
set "VERBOSE=0"

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--debug" set "BUILD_TYPE=Debug"
if /i "%~1"=="--clean" set "CLEAN_BUILD=1"
if /i "%~1"=="--run" set "RUN_AFTER_BUILD=1"
if /i "%~1"=="--verbose" set "VERBOSE=1"
if /i "%~1"=="--help" goto :show_help
shift
goto :parse_args

:show_help
echo Usage: build.bat [options]
echo.
echo Options:
echo   --debug     Build in Debug mode (default: Release)
echo   --clean     Clean build directory before building
echo   --run       Run the application after successful build
echo   --verbose   Enable verbose output
echo   --help      Show this help message
echo.
pause
exit /b 0

:args_done

if "%VERBOSE%"=="1" (
    echo [VERBOSE] Build configuration:
    echo   - Build Type: %BUILD_TYPE%
    echo   - Clean Build: %CLEAN_BUILD%
    echo   - Run After Build: %RUN_AFTER_BUILD%
    echo.
)

REM ================================================================
REM System Requirements Check
REM ================================================================

echo [0/5] Checking system requirements...

REM Check CMake version
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo   [ERROR] CMake not found!
    echo   Please install CMake 3.16 or later from: https://cmake.org/download/
    pause
    exit /b 1
)

REM Get CMake version
for /f "tokens=3" %%i in ('cmake --version ^| findstr /r "cmake version"') do (
    set "CMAKE_VERSION=%%i"
    echo   [OK] CMake version: %%i
)

REM Check Git (optional but recommended)
git --version >nul 2>&1
if %errorlevel% equ 0 (
    for /f "tokens=3" %%i in ('git --version') do (
        echo   [OK] Git version: %%i
    )
) else (
    echo   [INFO] Git not found (optional)
)

REM ================================================================
REM Qt 6.5.3 Detection and Environment Setup
REM ================================================================

echo.
echo [1/5] Detecting Qt 6.5.3 installation...

set "QT_FOUND=0"
set "QT_PATH="

REM Check environment variable first
if defined QT_DIR (
    if exist "%QT_DIR%\bin\qmake.exe" (
        echo   [OK] Found Qt from QT_DIR environment variable: "%QT_DIR%"
        set "QT_PATH=%QT_DIR%"
        set "QT_FOUND=1"
        goto :qt_found
    ) else (
        echo   [WARNING] QT_DIR is set but invalid: "%QT_DIR%"
    )
)

REM Define Qt search paths (add more as needed)
set "QT_SEARCH_PATHS=C:\Qt\6.5.3\mingw_64 C:\Qt\6.5.3\msvc2022_64 C:\Qt\6.5.3\msvc2019_64 C:\Qt\6.8.0\mingw_64 C:\Qt\6.7.0\mingw_64 C:\Qt\6.6.0\mingw_64 C:\Qt6 C:\Qt\Tools\mingw1120_64"

for %%D in (%QT_SEARCH_PATHS%) do (
    if exist "%%~D\bin\qmake.exe" (
        echo   [OK] Found Qt at: "%%~D"
        set "QT_PATH=%%~D"
        set "QT_FOUND=1"
        goto :qt_found
    )
)

REM If not found, try registry lookup
echo   [INFO] Checking Windows registry for Qt installation...
for /f "tokens=2*" %%A in ('reg query "HKEY_LOCAL_MACHINE\SOFTWARE\Classes\Qt6.qmake" /ve 2^>nul') do (
    set "QT_QMAKE_PATH=%%B"
    if exist "!QT_QMAKE_PATH!" (
        for %%P in ("!QT_QMAKE_PATH!") do (
            set "QT_PATH=%%~dpP"
            set "QT_PATH=!QT_PATH:~0,-5!"
            echo   [OK] Found Qt via registry at: "!QT_PATH!"
            set "QT_FOUND=1"
            goto :qt_found
        )
    )
)

:qt_not_found
echo.
echo   [ERROR] Qt 6.5.3 (or compatible version) not found!
echo.
echo   Please install Qt 6.5.3 with one of the following:
echo   1. Qt Online Installer: https://www.qt.io/download-qt-installer
echo   2. Select Qt 6.5.3 with MinGW 11.2.0 64-bit or MSVC 2019/2022 64-bit
echo   3. Make sure Qt is installed in one of these locations:
for %%D in (%QT_SEARCH_PATHS%) do echo      - %%D
echo.
echo   Or set QT_DIR environment variable to your Qt installation path.
echo.
pause
exit /b 1

:qt_found
echo   [OK] Qt installation found at: %QT_PATH%

REM Verify Qt modules
echo   [INFO] Verifying Qt modules...
set "QT_MODULES_OK=1"

if not exist "%QT_PATH%\lib\cmake\Qt6Core" (
    echo   [ERROR] Qt6Core module not found
    set "QT_MODULES_OK=0"
)
if not exist "%QT_PATH%\lib\cmake\Qt6Widgets" (
    echo   [ERROR] Qt6Widgets module not found
    set "QT_MODULES_OK=0"
)
if not exist "%QT_PATH%\lib\cmake\Qt6Network" (
    echo   [ERROR] Qt6Network module not found
    set "QT_MODULES_OK=0"
)

if "%QT_MODULES_OK%"=="0" (
    echo   [ERROR] Required Qt modules are missing!
    echo   Please install Qt 6.5.3 with Core, Widgets, and Network modules.
    pause
    exit /b 1
)

echo   [OK] All required Qt modules found

REM Get Qt version
if exist "%QT_PATH%\bin\qmake.exe" (
    "%QT_PATH%\bin\qmake.exe" --version 2>nul | findstr /r "Qt version" > temp_qt_version.txt 2>nul
    if exist temp_qt_version.txt (
        for /f "tokens=4" %%i in (temp_qt_version.txt) do (
            echo   [OK] Qt version: %%i
        )
        del temp_qt_version.txt
    )
)

REM ================================================================
REM Environment Setup
REM ================================================================

echo.
echo [2/5] Setting up build environment...

REM Set Qt environment variables
set "PATH=%QT_PATH%\bin;%PATH%"
set "CMAKE_PREFIX_PATH=%QT_PATH%"
set "Qt6_DIR=%QT_PATH%\lib\cmake\Qt6"

REM Detect and set up compiler environment
set "COMPILER_FOUND=0"

REM Check for MinGW (comes with Qt)
if exist "%QT_PATH%\..\..\..\Tools\mingw1120_64\bin\gcc.exe" (
    set "MINGW_PATH=%QT_PATH%\..\..\..\Tools\mingw1120_64\bin"
    set "PATH=!MINGW_PATH!;%PATH%"
    set "COMPILER_TYPE=MinGW"
    set "CMAKE_GENERATOR=MinGW Makefiles"
    set "COMPILER_FOUND=1"
    echo   [OK] Using MinGW compiler from Qt Tools
    goto :compiler_found
)

REM Check for MinGW in Qt directory
if exist "%QT_PATH%\..\..\Tools\mingw1120_64\bin\gcc.exe" (
    set "MINGW_PATH=%QT_PATH%\..\..\Tools\mingw1120_64\bin"
    set "PATH=!MINGW_PATH!;%PATH%"
    set "COMPILER_TYPE=MinGW"
    set "CMAKE_GENERATOR=MinGW Makefiles"
    set "COMPILER_FOUND=1"
    echo   [OK] Using MinGW compiler from Qt Tools
    goto :compiler_found
)

REM Check for system MinGW
where gcc >nul 2>&1
if %errorlevel% equ 0 (
    set "COMPILER_TYPE=MinGW"
    set "CMAKE_GENERATOR=MinGW Makefiles"
    set "COMPILER_FOUND=1"
    echo   [OK] Using system MinGW compiler
    goto :compiler_found
)

REM Check for cl.exe in PATH (If user already set up environment)
where cl.exe >nul 2>&1
if %errorlevel% equ 0 (
    set "COMPILER_TYPE=MSVC (System PATH)"
    set "CMAKE_GENERATOR=Visual Studio 17 2022"
    set "COMPILER_FOUND=1"
    echo   [OK] Found cl.exe in PATH. Assuming environment is ready.
    goto :compiler_found
)

REM Check for Visual Studio "18" (Preview/Newer Build Tools)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    set "COMPILER_TYPE=MSVC (VS18 BuildTools)"
    set "CMAKE_GENERATOR=Visual Studio 17 2022"
    set "VS_INSTALL_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools"
    set "COMPILER_FOUND=1"
    echo   [OK] Using Visual Studio 18 BuildTools
    goto :compiler_found
)

REM Check using vswhere (Recommended method)
set "VSWHERE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
echo   [INFO] Checking for vswhere at: "%VSWHERE_PATH%"
if exist "%VSWHERE_PATH%" (
    echo   [INFO] vswhere found. Querying for Visual Studio...
    
    REM Debug: Show raw output
    "%VSWHERE_PATH%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > vswhere_output.txt
    set /p VSWHERE_DEBUG=<vswhere_output.txt
    echo   [INFO] vswhere output: !VSWHERE_DEBUG!
    del vswhere_output.txt
    
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE_PATH%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_INSTALL_PATH=%%i"
    )
    if defined VS_INSTALL_PATH (
        echo   [INFO] vswhere returned path: "!VS_INSTALL_PATH!"
        if exist "!VS_INSTALL_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
             call "!VS_INSTALL_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
             set "COMPILER_TYPE=MSVC (Detected)"
             set "CMAKE_GENERATOR=Visual Studio 17 2022"
             set "COMPILER_FOUND=1"
             echo   [OK] Found Visual Studio via vswhere at: !VS_INSTALL_PATH!
             goto :compiler_found
        ) else (
             echo   [WARNING] vcvarsall.bat not found at: "!VS_INSTALL_PATH!\VC\Auxiliary\Build\vcvarsall.bat"
        )
    ) else (
        echo   [WARNING] vswhere returned no path (Visual Studio with C++ tools might not be installed correctly)
    )
) else (
    echo   [WARNING] vswhere.exe not found.
)

REM Check for Visual Studio 2022 (Fallback)
for %%V in (Community Professional Enterprise BuildTools) do (
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%V\VC\Auxiliary\Build\vcvarsall.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\%%V\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
        set "COMPILER_TYPE=MSVC2022"
        set "CMAKE_GENERATOR=Visual Studio 17 2022"
        set "COMPILER_FOUND=1"
        echo   [OK] Using Visual Studio 2022 %%V
        goto :compiler_found
    )
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\%%V\VC\Auxiliary\Build\vcvarsall.bat" (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\%%V\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
        set "COMPILER_TYPE=MSVC2022"
        set "CMAKE_GENERATOR=Visual Studio 17 2022"
        set "COMPILER_FOUND=1"
        echo   [OK] Using Visual Studio 2022 %%V
        goto :compiler_found
    )
)

REM Check for Visual Studio 2019 (Fallback)
for %%V in (Community Professional Enterprise BuildTools) do (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\%%V\VC\Auxiliary\Build\vcvarsall.bat" (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\%%V\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
        set "COMPILER_TYPE=MSVC2019"
        set "CMAKE_GENERATOR=Visual Studio 16 2019"
        set "COMPILER_FOUND=1"
        echo   [OK] Using Visual Studio 2019 %%V
        goto :compiler_found
    )
)

REM Check for Visual Studio 2017 (Fallback)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    set "COMPILER_TYPE=MSVC2017"
    set "CMAKE_GENERATOR=Visual Studio 15 2017"
    set "COMPILER_FOUND=1"
    echo   [OK] Using Visual Studio 2017
    goto :compiler_found
)

:compiler_not_found
echo   [ERROR] No suitable compiler found!
echo.
echo   To fix this, please install one of the following:
echo   1. Visual Studio 2022 Community Edition:
echo      https://visualstudio.microsoft.com/vs/
echo      - Choose "Desktop development with C++" workload during installation
echo.
echo   2. Visual Studio 2019 Community Edition:
echo      https://visualstudio.microsoft.com/vs/older-downloads/
echo      - Choose "Desktop development with C++" workload during installation
echo.
echo   3. Or install MinGW-w64 (usually comes with Qt installation):
echo      - Install through Qt Online Installer
echo      - Select MinGW option during Qt setup
echo.
echo   After installation, close and reopen this command prompt for changes to take effect.
echo.
pause
exit /b 1

:compiler_found
echo   [OK] Compiler: %COMPILER_TYPE%
echo   [OK] CMake Generator: %CMAKE_GENERATOR%

REM Detect CPU cores for parallel building
for /f %%i in ('echo %NUMBER_OF_PROCESSORS%') do set "CPU_CORES=%%i"
echo   [OK] CPU cores available: %CPU_CORES%

REM ================================================================
REM WireGuard DLL Check
REM ================================================================

echo.
echo [3/5] Checking WireGuard dependencies...

set "WIREGUARD_DLLS_FOUND=1"
if not exist "tunnel.dll" (
    echo   [WARNING] tunnel.dll not found in current directory
    set "WIREGUARD_DLLS_FOUND=0"
)
if not exist "wireguard.dll" (
    echo   [WARNING] wireguard.dll not found in current directory
    set "WIREGUARD_DLLS_FOUND=0"
)

if "%WIREGUARD_DLLS_FOUND%"=="1" (
    echo   [OK] WireGuard DLLs found
) else (
    echo   [INFO] WireGuard DLLs not found - VPN features will be disabled
    echo   [INFO] Run setup_wireguard_dlls.bat to set up WireGuard DLLs
)

REM ================================================================
REM Project Configuration
REM ================================================================

echo.
echo [4/5] Configuring project with CMake...

REM Clean build if requested
if "%CLEAN_BUILD%"=="1" (
    echo   [INFO] Cleaning build directory...
    if exist "build" rmdir /s /q build
)

REM Clean previous build if it exists with different configuration
if exist "build\CMakeCache.txt" (
    echo   [INFO] Found existing build configuration...
    findstr /C:"%CMAKE_GENERATOR%" build\CMakeCache.txt >nul 2>&1
    if %errorlevel% neq 0 (
        echo   [INFO] Different generator detected, cleaning build directory...
        if exist "build" rmdir /s /q build
    )
)

REM Create build directory
if not exist "build" (
    echo   [INFO] Creating build directory...
    mkdir build
)

cd build

REM Configure with CMake
echo   [INFO] Running CMake configuration for %BUILD_TYPE% build...
set "CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH%"

REM Add generator instance if we found a path (helps CMake find VS in non-standard locations)
if defined VS_INSTALL_PATH (
    set "CMAKE_ARGS=!CMAKE_ARGS! -DCMAKE_GENERATOR_INSTANCE=\"!VS_INSTALL_PATH!\""
)

if "%COMPILER_TYPE%"=="MinGW" (
    cmake .. -G "%CMAKE_GENERATOR%" %CMAKE_ARGS%
) else (
    cmake .. -G "%CMAKE_GENERATOR%" -A x64 %CMAKE_ARGS%
)

if %errorlevel% neq 0 (
    echo.
    echo   [ERROR] CMake configuration failed with error code %errorlevel%!
    echo.
    echo   Troubleshooting steps:
    echo   1. Check that Qt 6.5.3 is properly installed
    echo   2. Verify all Qt modules are available (Core, Widgets, Network)
    echo   3. Make sure the compiler is properly set up
    echo   4. Check CMake version (3.16+ required)
    echo.
    echo   Current environment:
    echo   - Qt Path: %QT_PATH%
    echo   - CMAKE_PREFIX_PATH: %CMAKE_PREFIX_PATH%
    echo   - Compiler: %COMPILER_TYPE%
    echo   - Generator: %CMAKE_GENERATOR%
    echo.
    cd ..
    pause
    exit /b 1
)

REM Validate CMake cache files were created successfully
echo   [INFO] Validating CMake cache files...
if not exist "CMakeCache.txt" (
    echo   [ERROR] CMakeCache.txt was not created!
    cd ..
    pause
    exit /b 1
)

if not exist "cmake_install.cmake" (
    echo   [ERROR] cmake_install.cmake was not created!
    cd ..
    pause
    exit /b 1
)

if not exist "CMakeFiles" (
    echo   [ERROR] CMakeFiles directory was not created!
    cd ..
    pause
    exit /b 1
)

REM Check for proper Makefile generation (MinGW) or solution files (MSVC)
if "%COMPILER_TYPE%"=="MinGW" (
    if not exist "Makefile" (
        echo   [ERROR] Makefile was not generated!
        cd ..
        pause
        exit /b 1
    )    echo   [OK] Makefile generated successfully
) else (
    if not exist "ViscoConnect.sln" (
        echo   [WARNING] Visual Studio solution file not found - checking for project files...
        if not exist "ViscoConnect.vcxproj" (
            echo   [ERROR] No Visual Studio project files were generated!
            cd ..
            pause
            exit /b 1
        )
    )
    echo   [OK] Visual Studio project files generated successfully
)

REM Process and compile CMake cache dependencies
echo   [INFO] Processing CMake cache and dependencies...
cmake --build . --target cmake_check_build_system

if %errorlevel% neq 0 (
    echo   [WARNING] CMake build system check reported issues (error code %errorlevel%)
    echo   [INFO] Attempting to regenerate build files...
    cmake .. -G "%CMAKE_GENERATOR%" %CMAKE_ARGS%
    if %errorlevel% neq 0 (
        echo   [ERROR] Failed to regenerate build files!
        cd ..
        pause
        exit /b 1
    )
)

REM Force CMake to rescan for source file changes and update dependencies
echo   [INFO] Updating CMake dependencies and cache for source changes...
cmake --build . --target cmake_check_build_system --config %BUILD_TYPE%

REM Check if we need to regenerate due to CMakeLists.txt changes
if exist "..\CMakeLists.txt" (
    for %%F in ("CMakeCache.txt") do set "CACHE_TIME=%%~tF"
    for %%F in ("..\CMakeLists.txt") do set "CMAKE_TIME=%%~tF"
    
    REM If CMakeLists.txt is newer than cache, regenerate
    echo   [INFO] Checking if CMakeLists.txt has been modified...
    if "!CMAKE_TIME!" GTR "!CACHE_TIME!" (
        echo   [INFO] CMakeLists.txt is newer than cache - regenerating...
        cmake .. -G "%CMAKE_GENERATOR%" %CMAKE_ARGS%
        if %errorlevel% neq 0 (
            echo   [ERROR] Failed to regenerate after CMakeLists.txt changes!
            cd ..
            pause
            exit /b 1
        )
    )
)

echo   [OK] CMake configuration and cache compilation completed successfully!

REM ================================================================
REM Build Process
REM ================================================================

echo.
echo [5/5] Building project...

REM Pre-compile Qt MOC files and auto-generated sources
echo   [INFO] Pre-compiling Qt MOC and auto-generated files...
if "%COMPILER_TYPE%"=="MinGW" (
    REM For MinGW, compile autogen target first
    cmake --build . --target ViscoConnect_autogen --config %BUILD_TYPE%
    if %errorlevel% neq 0 (
        echo   [WARNING] Auto-generation step had issues (error code %errorlevel%)
        echo   [INFO] Continuing with main build...
    ) else (
        echo   [OK] Qt MOC and auto-generation completed successfully
    )
) else (
    REM For MSVC, ensure autogen target is built
    cmake --build . --target ViscoConnect_autogen --config %BUILD_TYPE%
    if %errorlevel% neq 0 (
        echo   [WARNING] Auto-generation step had issues (error code %errorlevel%)
        echo   [INFO] Continuing with main build...
    ) else (
        echo   [OK] Qt MOC and auto-generation completed successfully
    )
)

REM Build the project
echo   [INFO] Compiling source files with %CPU_CORES% parallel jobs...
set "BUILD_ARGS=--config %BUILD_TYPE% --parallel %CPU_CORES%"

if "%VERBOSE%"=="1" (
    set "BUILD_ARGS=%BUILD_ARGS% --verbose"
)

cmake --build . %BUILD_ARGS%

if %errorlevel% neq 0 (
    echo.
    echo   [ERROR] Build failed with error code %errorlevel%!
    echo.
    echo   Common solutions:
    echo   1. Check for missing dependencies (Qt modules, Windows SDK)
    echo   2. Verify all source files are present
    echo   3. Check for compilation errors in the output above
    echo   4. Make sure WireGuard DLLs are available (tunnel.dll, wireguard.dll)
    echo   5. Try running with --clean flag to force rebuild
    echo.
    cd ..
    pause
    exit /b 1
)

REM Validate final build artifacts
echo   [INFO] Validating build artifacts and cache files...
if exist "ViscoConnect_autogen" (
    echo   [OK] Qt autogen files compiled successfully
) else (
    echo   [WARNING] Qt autogen directory not found - this may indicate MOC compilation issues
)

if exist "CMakeFiles\ViscoConnect.dir" (
    echo   [OK] CMake object files directory exists
) else (
    echo   [WARNING] CMake object files directory not found
)

echo   [OK] Build completed successfully!

cd ..

REM ================================================================
REM Success and Instructions
REM ================================================================

echo.
echo ================================================================
echo                     BUILD SUCCESSFUL!
echo ================================================================
echo.

REM Find the executable
set "EXE_PATH="
if exist "build\%BUILD_TYPE%\ViscoConnect.exe" (
    set "EXE_PATH=build\%BUILD_TYPE%\ViscoConnect.exe"
) else if exist "build\bin\ViscoConnect.exe" (
    set "EXE_PATH=build\bin\ViscoConnect.exe"
) else if exist "build\ViscoConnect.exe" (
    set "EXE_PATH=build\ViscoConnect.exe"
)

if defined EXE_PATH (
    echo   [OK] Executable created: %EXE_PATH%
    echo.
    
    REM Check file size to verify it's not corrupted
    for %%F in ("%EXE_PATH%") do (
        if %%~zF GTR 1048576 (
            echo   [OK] Executable size: %%~zF bytes (looks good)
        ) else (
            echo   [WARNING] Executable size: %%~zF bytes (seems small, check for issues)
        )
    )
) else (
    echo   [WARNING] Executable not found in expected locations
    echo   [INFO] Searching for executable...
    dir /s /b build\*.exe 2>nul
)

echo.
echo Build Information:
echo   - Qt Version: %QT_PATH%
echo   - Compiler: %COMPILER_TYPE%
echo   - Build Type: %BUILD_TYPE%
echo   - Architecture: x64
echo   - CMake Version: %CMAKE_VERSION%
echo   - Parallel Jobs: %CPU_CORES%
if "%WIREGUARD_DLLS_FOUND%"=="1" (
    echo   - WireGuard DLLs: Available
) else (
    echo   - WireGuard DLLs: Not found
)
echo.

echo Next Steps:
echo   1. Make sure tunnel.dll and wireguard.dll are in the same directory as the executable
echo   2. Run the application as Administrator for VPN functionality
echo   3. For Windows Service installation, use the Service menu in the GUI
echo.

if "%RUN_AFTER_BUILD%"=="1" (
    if defined EXE_PATH (
        echo [INFO] Launching application...
        echo.
        start "" "%EXE_PATH%"
        goto :end
    ) else (
        echo [ERROR] Cannot run application - executable not found
    )
)

echo To run the application:
if defined EXE_PATH (
    echo   %EXE_PATH%
) else (
    echo   Check the build directory for ViscoConnect.exe
)

echo.
echo To run with VPN features (requires Administrator):
echo   Right-click Command Prompt -^> Run as Administrator
if defined EXE_PATH (
    echo   %EXE_PATH%
)

echo.
echo Command line options for this build script:
echo   build.bat --help     Show help
echo   build.bat --debug    Build in Debug mode
echo   build.bat --clean    Clean build directory
echo   build.bat --run      Run after successful build
echo   build.bat --verbose  Enable verbose output

:end
echo.
echo ================================================================
pause
