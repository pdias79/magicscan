# MagicScan developer guide

MagicScan targets **Windows 10/11 (x64)** using C++17 and native Win32/GDI+ APIs. No Python, Qt, FLTK or .NET runtime is required.

Current capabilities:
- `magicscan --probe`: read-only SCSI identification (expected `NOVA`); needs a connected scanner.
- `magicscan --list-disks`: print storage device descriptors and open/query errors **without issuing any vendor SCSI commands**. Use to diagnose scanner discovery without administrator privileges. Redact disk serial numbers before sharing output.
- `magicscan --preview scan.png`: native image preview with selection, zoom, pan, clipboard copy and PNG export.
- `magicscan --capture-band out.png`: validated **read-only** capture of one fresh 2592×192 RGB strip (requires elevated PowerShell on currently tested Windows setup).
- `magicscan --capture-page out.png`: validated single-page acquisition. Start the command first, press SCAN to start and again to stop; the image strips are assembled into one PNG.
- `magicscan --monitor DIR`: experimental console-based continuous capture. Wait for button-triggered scans, save each PNG in DIR with a unique timestamped filename, and copy the saved image to the Windows clipboard as CF_DIB (24-bit DIB). `--no-clipboard` disables copying while retaining PNG export. Keep the console running; use Ctrl+C to exit. Test clipboard functionality by pasting into Paint or Word from the same interactive Windows desktop. Console monitoring still requires elevation on the tested system.

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

## Windows tray prototype (PR #5)

The Win32 notification-area prototype is built separately from the diagnostic CLI, with GUI code in `src/gui/`, acquisition in `src/usb/`, and scan orchestration in `src/core/`.

From an elevated interactive Windows desktop session, run:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan-tray.exe "C:\Scans"
```

The tray icon starts continuous scan monitoring. Right-click for **Pause monitoring**, **Resume monitoring**, **Open scans folder** and **Quit**. Double-click the icon to open the scan folder. Both PNG save and CF_DIB clipboard export are enabled. Without an explicit folder argument the application uses `%USERPROFILE%\Pictures\MagicScan`.

This first tray implementation still needs Windows hardware validation, elevated permissions, and UI tests. **PR #6 will add a live, band-by-band scan preview**: the scanner acquisition backend will publish decoded strip events to a separate core/controller, and the GUI thread will render the accumulating image without blocking USB polling. No GUI code should be added to the USB transport module.

## Live band-by-band preview (PR #6)

The tray program opens a Win32 live preview window on the first SCAN press. Each received 2592x192 RGB strip is decoded within the USB acquisition backend and delivered as a GUI-neutral callback to `core/monitor`, then queued to the GUI thread for painting. Each band is displayed as it arrives; the window remains visible after the scan stops. The existing PNG and CF_DIB clipboard outputs are retained.

The three responsibilities remain separated: `src/usb/` handles SCSI only; `src/core/` controls scan sessions and file output; `src/gui/` owns Windows windows and painting. Do not perform UI work or Win32 window calls in the SCSI thread.

### Hardware test (elevated interactive PowerShell)

```powershell
git fetch origin
git switch feature/live-band-preview
git pull origin feature/live-band-preview
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan-tray.exe "C:\Scans"
```

Begin a scan using the physical SCAN button: the preview should appear immediately and grow by one image band every time a strip arrives. Press SCAN again to finish, then verify the saved PNG and clipboard. For this first revision, zoom/crop editing remains in the existing `magicscan --preview` tool; direct editor integration will be refined after hardware validation.

## Editable live preview (PR #7)

The live scan window now supports rectangular selection (left drag), pan (right drag), zoom (wheel or +/-), Ctrl+C (copy selected region as CF_DIB), Ctrl+S (save selected region to a PNG via Save As), and Ctrl+A (clear selection). Selection defaults to the entire accumulated page. The preview remains visible after each scan and resets for the next one. PNG saving and automatic clipboard export by the monitor continue unchanged.

USB protocol and band decoding remain in `src/usb/`; scan orchestration in `src/core/`; Win32 rendering, user editing and clipboard operation in `src/gui/`. On test Windows systems launch in elevated interactive PowerShell:

```powershell
cd "C:\Users\Philou\Documents\repos\magicscan"
git fetch origin
git switch feature/live-preview-editing
git pull origin feature/live-preview-editing
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan-tray.exe "C:\Scans"
```

Scan a page; confirm the display grows during the scan. Draw a rectangle over the finished image; press Ctrl+C and paste into Paint, and press Ctrl+S to save a cropped PNG. Zoom/pan and scan again without closing the window. The Windows clipboard is still updated with each new full-page scan as previously established.

### Live preview refinement

- **Escape** cancels the active rectangular selection. Ctrl+C/Ctrl+S then target the complete image.
- The rectangle is anchored in **image pixel coordinates**. Wheel/+/- zoom and right-drag pan transform the displayed outline without changing the selected pixel region.
- The live window draws each frame to an off-screen GDI bitmap and blits once to suppress selection/pan flicker.

Test with a scan: select a distinctive small area, zoom several times, pan, then copy it to Paint; copied pixels and PNG dimensions must remain unchanged. Press Escape and copy again; the full page should be used.
