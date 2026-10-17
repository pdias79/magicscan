# MagicScan developer guide

MagicScan targets **Windows 10/11 (x64)** using C++17 and native Win32/GDI+ APIs. No Python, Qt, FLTK or .NET runtime is required.

Current capabilities:
- `magicscan --probe`: read-only SCSI identification (expected `NOVA`); needs a connected scanner.
- `magicscan --preview scan.png`: native image preview with selection, zoom, pan, clipboard copy and PNG export.
- `magicscan --capture-band out.png`: experimental **read-only** capture of one fresh 2592×192 RGB strip. Start a scan after launching the command; it waits up to 60 seconds. This is a diagnostic milestone, not a full-page scan.

**Not yet implemented in C++:** full-page USB image capture, automatic background monitoring, user-configured output directory, and automatic opening of the preview after scan. The previously established USB acquisition protocol is specified in `docs/usb-protocol.md`.

## Set up VS Code

Install Git, VS Code, and Visual Studio 2022 Build Tools with the **Desktop development with C++** workload, including the MSVC compiler and Windows SDK. VS Code extensions: **C/C++** and **CMake Tools**.

```powershell
git clone https://github.com/pdias79/magicscan.git
cd magicscan
git switch feature/bootstrap-cpp
code .
```

In VS Code run **CMake: Select a Kit** (Visual Studio x64), **CMake: Configure**, then **CMake: Build**.

In a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan.exe --help
.\build\Release\magicscan.exe --probe
.\build\Release\magicscan.exe --capture-band "C:\Scans\band.png"
.\build\Release\magicscan.exe --preview "C:\Scans\example.png"
```

### Preview controls

- Left mouse drag: draw a rectangular selection.
- Right mouse drag: pan image.
- Mouse wheel: zoom around cursor.
- Ctrl+C: copy selected image, or whole image if selection is cleared, to Windows clipboard.
- Ctrl+S: save selection/whole image as PNG.
- Ctrl+A: clear selection (use the whole image).

Run `--probe` from a normal **non-administrator** terminal and report its complete output. Probe access may still be restricted by Windows device permissions. Do not change scanner USB drivers or send write commands.

## Build status

GitHub Actions builds the Windows x64 executable and uploads it as a build artifact. A successful automated build validates compilation; hardware behavior and graphical interaction still require local testing.

## Privacy

Do not commit USB traces, captured scan images, or logs containing private data. The protocol specification is based on USB traffic reverse engineering only.
