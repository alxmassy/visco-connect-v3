# Visco Connect - Release Workflow

To create a newer version of the installer after making code changes, follow this disciplined **Build → Deploy → MSI → Bundle** workflow.

Since we are bundling the VC++ Redistributable, this process includes the final step to create the standalone `.exe` Bootstrapper.

## Step 1: Update the Version Number
Before building, you must increment the version so Windows knows it's an update.

1.  **C++ Code**: Update any version constants in your `src/main.cpp` or version headers.
2.  **WiX Config**: Open `installer/CameraServerBasic.wxs` AND `installer/Bundle.wxs`.
    *   Change the `Version` attribute (e.g., from `3.1.7` to `3.1.8`).
    *   **Crucial**: For a "Major Upgrade" to work automatically, ensure `Product Id="*"` in the `.wxs` file (the asterisk allows WiX to generate a new GUID automatically).

## Step 2: Build the Release Executable
Open your **x64 Native Tools Command Prompt for VS 2019/2022** and run the build sequence:

```cmd
cd C:\dev\uct\visco-pc-v3.1.7

REM Clean previous build
rmdir /s /q build
mkdir build
cd build

REM Configure and Build with Ninja
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.5.3/msvc2019_64"
cmake --build . --config Release
```

## Step 3: Deploy Qt Dependencies
Stay in the terminal and run the deployment script. This gathers the DLLs and the VC++ Runtime into `build/bin`.

```cmd
cd C:\dev\uct\visco-pc-v3.1.7
deploy.bat
```

**Verification**: Check that `vcruntime140.dll` is present in `build\bin`.

## Step 4: Generate the MSI and EXE Bundle
Run the updated installer script. This creates the MSI first, then wraps it into the Bootstrapper `.exe`.

```cmd
cd C:\dev\uct\visco-pc-v3.1.7\installer
build_installer.bat
```

## Final Output
You will find the release files in `installer/output`:
*   **`ViscoConnect_v3.1.x_Setup.exe`** (Distribute THIS file)
*   `ViscoConnect_v3.1.x_Setup.msi` (Internal use only)
