# MagicScan development

Native C++17 scanner utility for Windows and Linux. This repository is a work in progress.

The current milestone is **a read-only USB identification probe**, not an image capture application. The Linux backend and FLTK interface are planned, not yet implemented.

## Windows setup

Install [Visual Studio Community](https://visualstudio.microsoft.com/) or Visual Studio Build Tools with the **Desktop development with C++** workload (MSVC and Windows SDK), [CMake](https://cmake.org/download/), [Git](https://git-scm.com/downloads), and [VS Code](https://code.visualstudio.com/).

In VS Code install **C/C++** and **CMake Tools**. Open the cloned repository folder. Use the command palette to choose **CMake: Select a Kit** (Visual Studio x64) and **CMake: Configure**, then **CMake: Build**.

Alternatively, in the *Developer PowerShell for VS 2022* terminal:

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\magicscan.exe --probe
```

Run `magicscan --probe` first from a **normal, non-elevated** terminal. If the device is not accessible, report the complete console output. Do not run the entire desktop application as administrator as a workaround.

## Linux

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/magicscan --help
```

Linux build currently provides a clear "not implemented" message for the SCSI probe. A future backend will use SG_IO; no `sudo` workflow is provided yet.

## Privacy

Do not commit captured images, USB packet traces, or private scan logs. See `docs/usb-protocol.md` for the protocol specification.
