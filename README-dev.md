# MagicScan developer guide

MagicScan targets **Windows 10/11 (x64)** using C++17 and native Win32/GDI+ APIs. No Python, Qt, FLTK or .NET runtime is required.

Current capabilities:
- `magicscan --probe`: read-only SCSI identification (expected `NOVA`); needs a connected scanner.
- `magicscan --list-disks`: print storage device descriptors and open/query errors **without issuing any vendor SCSI commands**. Use to diagnose scanner discovery without administrator privileges. Redact disk serial numbers before sharing output.
- `magicscan --preview scan.png`: native image preview with selection, zoom, pan, clipboard copy and PNG export.
- `magicscan --capture-band out.png`: validated **read-only** capture of one fresh 2592×192 RGB strip (requires elevated PowerShell on currently tested Windows setup).
- `magicscan --capture-page out.png`: validated single-page acquisition. Start the command first, press SCAN to start and again to stop; the image strips are assembled into one PNG.
- `magicscan --monitor DIR`: experimental console-based continuous capture. Wait for button-triggered scans, save each PNG in DIR with a unique timestamped filename, and copy the saved image to the Windows clipboard. `--no-clipboard` disables copying while retaining PNG export. Keep the console running; use Ctrl+C to exit.

**Not yet implemented in C++:** hidden system-tray process, automatic start at Windows login, automatic preview after scan, and permission-free SCSI device access. Monitoring currently runs in an open console and requires elevated PowerShell on the tested setup. Repeated scans and clipboard behavior require hardware testing. The previously established USB acquisition protocol is specified in `docs/usb-protocol.md`.

## Set up VS Code

Install Git, VS Code, and Visual Studio 2022 Build Tools with the **Desktop development with C++** workload, including the MSVC compiler and Windows SDK. VS Code extensions: **C/C++** and **CMake Tools**.

```powershell
git clone https://github.com/pdias79/magicscan.git
cd magicscan
git switch feature/continuous-monitor
code .
```

In VS Code run **CMake: Select a Kit** (Visual Studio x64), **CMake: Configure**, then **CMake: Build**.

In a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan.exe --help
.\build\Release\magicscan.exe --list-disks
.\build\Release\magicscan.exe --probe
.\build\Release\magicscan.exe --capture-band "C:\Scans\band.png"
.\build\Release\magicscan.exe --capture-page "C:\Scans\page.png"
.\build\Release\magicscan.exe --monitor "C:\Scans"
.\build\Release\magicscan.exe --preview "C:\Scans\example.png"
```

### Preview controls

- Left mouse drag: draw a rectangular selection.
- Right mouse drag: pan image.
- Mouse wheel: zoom around cursor; `+` / `-`: zoom around window center.
- Ctrl+C: copy selected image, or whole image if selection is cleared, to Windows clipboard.
- Ctrl+S: save selection/whole image as PNG.
- Ctrl+A: clear selection (use the whole image).

On the tested Windows system, the scanner requires an elevated terminal and a `GENERIC_READ | GENERIC_WRITE` handle for SCSI pass-through. The application sends **only DATA_IN SCSI commands**, not writes to scanner memory or its microSD filesystem. Avoid changing scanner drivers.

## Build status

GitHub Actions builds the Windows x64 executable and uploads it as a build artifact. A successful automated build validates compilation; hardware behavior and graphical interaction still require local testing.

## Privacy

Do not commit USB traces, captured scan images, or logs containing private data. The protocol specification is based on USB traffic reverse engineering only.
