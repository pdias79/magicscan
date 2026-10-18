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

## Large-icon live preview toolbar (next PR)

The live preview has a native 90-pixel command strip with large glyph icons and readable labels for **Copy**, **Save**, **Clear**, **Fit**, **Zoom +** and **Zoom -**. Actions operate on the same selection and zoom state as the keyboard shortcuts, without changing the USB backend. The image is displayed below the toolbar. Selecting and panning remain flicker-free through double-buffered painting.

Run on the usual elevated Windows desktop:

```powershell
cd "C:\Users\Philou\Documents\repos\magicscan"
git fetch origin
git switch feature/large-icon-toolbar
git pull origin feature/large-icon-toolbar
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan-tray.exe "C:\Scans"
```

Scan a page; use the Copy and Save buttons with a selection, Clear to restore full-image operations, Fit to recenter, and Zoom +/- to preserve image-space selection. Confirm the toolbar remains visible during acquisition and the clipboard plus PNG exports are unaffected.

## French Windows interface, application icon and About dialog (PR #9)

The approved handheld-scanner artwork is stored as a small text-encoded PNG asset under `assets/` and converted at CMake configure time into a multi-resolution Windows `.ico` file (16, 24, 32, 48, 64, 128 and 256 pixels). The generated resource is embedded in both Windows executables, used as the live preview/window icon, and loaded for the notification area. Icon generation uses built-in Windows PowerShell and System.Drawing, without a third-party icon package.

All user-facing **Windows GUI** labels, menus, titles, dialogs and errors are now in French. The tray menu and the live preview's window system menu provide **À propos**, showing the CMake project version and a clickable button opening https://github.com/pdias79/magicscan. Command-line diagnostic logs remain English for existing automation compatibility.

### Test on Windows

Run in an **elevated interactive PowerShell** session:

```powershell
cd "C:\Users\Philou\Documents\repos\magicscan"
git fetch origin
git switch feature/app-icon-about-fr
git pull origin feature/app-icon-about-fr
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan-tray.exe "C:\Scans"
```

Verify Explorer shows the scanner icon on both executables, the notification icon matches it, and the live preview's titlebar/taskbar displays it. From the notification area open **À propos**: ensure version and GitHub address are shown and **Ouvrir GitHub** works. Start a scan to verify existing progressive rendering, French toolbar commands and PNG/clipboard output. Test the live preview's window system menu > **À propos de MagicScan**.

### PR #9 refinements: About dialog background and gamma

- The native About dialog uses a white background consistently for its static text and icon controls (transparent text backgrounds via `WM_CTLCOLORSTATIC`).
- The live preview has a **Correction gamma** slider from 0.20 to 3.00 (default 1.00). A gamma lookup table updates the rendered pixels without modifying original USB capture data. Cropped clipboard and manual PNG exports use corrected pixels; the automatically saved full-page PNG remains the unmodified acquisition. The slider resets to 1.00 for each new scan.
- This control is implemented solely in `src/gui/` using the Windows common-controls trackbar; `src/usb/` is unchanged.

After launching, open **À propos** and verify the icon and text have backgrounds matching the white dialog. Then scan a colorful page, adjust gamma in the live window (including darkening and brightening), select a region and verify **Copier** and **Enregistrer** preserve the displayed correction. The automatic PNG in the scan folder must still be unchanged.

## PR #10: automatic USB scanner reconnection

When the scanner is removed while MagicScan is running, SCSI calls fail and the old drive handle is closed as `capture_page` returns. The monitor **does not terminate**: it logs the interruption, marks the device as disconnected, waits 1.5 seconds in stop-aware increments, and re-runs scanner detection. A reconnected scanner can have a different PhysicalDrive identifier. The notification-area tooltip displays whether monitoring is paused, waiting for a scanner, or connected. No USB writes are added.

An interrupted page is discarded rather than exported as a valid PNG. On the next SCAN press, the live preview resets and resumes incremental rendering normally. The stop flag is checked between retries so Pause and Quit remain responsive.

### Hardware test (administrator PowerShell)

```powershell
cd "C:\Users\Philou\Documents\repos\magicscan"
git fetch origin
git switch feature/scanner-auto-reconnect
git pull origin feature/scanner-auto-reconnect
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan-tray.exe "C:\Scans"
```

Check these cases:
1. Launch with scanner disconnected; tooltip indicates waiting. Plug it in and scan normally without restarting.
2. Unplug during idle surveillance; plug in again and complete a scan.
3. Unplug mid-scan; unfinished capture should not produce a complete PNG. Replug and start a **new** scan; live preview, PNG and clipboard should work.
4. Repeat disconnect/reconnect more than once, possibly with a different physical drive number.
5. Pause and Quit while disconnected: MagicScan should stop in a bounded interval, without a modal error dialog.

## PR #11 — Paramètres persistants et traitement d’image (anciennes PR #11 + #12)

L’historique des scans n’est pas prévu.

Le menu de notification **Paramètres...** permet de modifier les options conservées dans
`%APPDATA%\\MagicScan\\settings.ini` : dossier de destination, sauvegarde automatique
de chaque scan, copie automatique vers le presse-papiers et gamma par défaut.

**Enregistrement automatique désactivé :** le scanner est lu intégralement en mémoire,
la fenêtre reçoit les bandes progressivement et conserve la page après acquisition.
Aucun PNG, y compris temporaire, n’est créé automatiquement ; les actions **Copier**
et **Enregistrer** restent accessibles. Le mode n’empêche pas de sauvegarder
manuellement une sélection ou la page complète.

L’éditeur propose également **Tourner** (rotation de 90° après un scan terminé),
les réglages en direct de luminosité et de contraste en plus du gamma, tout en
préservant les données brutes de l’acquisition et la sélection à l’écran.
L’export PNG manuel et la copie prennent en compte les corrections visibles.
Les originaux enregistrés automatiquement restent sans retouche.

**Remarque :** cette PR se concentre sur les retouches de base et l’export PNG
déjà pris en charge ; les exports JPEG/PDF pourront faire l’objet d’une évolution
distincte si demandés.

### Test matériel — PowerShell administrateur

```powershell
cd "C:\Users\Philou\Documents\repos\magicscan"
git fetch origin
git switch feature/preferences-image-tools
git pull origin feature/preferences-image-tools
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan-tray.exe "C:\Scans"
```

Vérifier : **Paramètres...** dans le systray, changement du dossier, bascule de la
sauvegarde automatique et du presse-papiers, gamma initial réappliqué à chaque
scan et options persistantes après redémarrage.

Pour le mode sans fichier : désactiver **Enregistrer automatiquement les scans
sur le disque**, effectuer un scan, constater qu’aucun nouveau fichier n’est créé,
puis utiliser **Copier** et **Enregistrer** manuellement depuis la prévisualisation.
Tester luminosité/contraste et rotation 90°, suivi d’un nouveau scan et d’une
déconnexion/reconnexion du scanner. Réactiver la sauvegarde et vérifier qu’un
PNG original est à nouveau créé.

### PR #11 — Ergonomie et redimensionnement de la prévisualisation

- Les contrôles texte et cases à cocher de **Paramètres** utilisent le même fond blanc que la fenêtre (messages `WM_CTLCOLORSTATIC` et `WM_CTLCOLORBTN`).
- L'éditeur actualise le dessin sur `WM_SIZE` et `WM_SIZING` pendant le redimensionnement interactif, afin de ne plus afficher une zone vide ou des artefacts avant le relâchement de la souris.
- Un bouton **Réglages** dans la barre supérieure affiche ou masque les trois curseurs. Le panneau est masqué au démarrage ; second clic pour le refermer. Gamma, luminosité et contraste sont trois curseurs de même largeur, alignés verticalement. Ils suivent la largeur de la fenêtre. Les valeurs sont conservées lorsque le panneau est fermé.
- Le panneau replié rend davantage d'espace à la prévisualisation et **Ajuster** tient compte de sa hauteur.

Tests : déplacer lentement puis rapidement les bordures avec une image affichée, ouvrir/fermer Réglages, vérifier l'alignement des trois curseurs et le recadrage/zoom, et vérifier les fonds des champs et cases à cocher dans Paramètres.

## PR #12 — Diagnostics d'accès USB et permissions Windows

Cette évolution améliore la compréhension des erreurs d'accès sans changer les droits Windows, les ACL, les pilotes, ni les commandes SCSI en lecture seule. Le systray distingue : scanner absent, accès refusé, périphérique occupé, scanner déconnecté et erreur générique. La surveillance continue ses nouvelles tentatives après chaque échec.

La commande de diagnostic **en lecture seule** fournit le chemin PhysicalDrive détecté, le succès/échec de l'identification NOVA, le code d'erreur Windows et une action conseillée :

```powershell
cd "C:\Users\Philou\Documents\repos\magicscan"
git fetch origin
git switch feature/usb-permission-diagnostics
git pull origin feature/usb-permission-diagnostics
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\magicscan.exe --diagnose-access
```

Lancer d'abord dans **PowerShell normal**, puis dans **PowerShell administrateur**. Comparer le résultat pour vérifier si l'élévation est réellement nécessaire sur ce PC. Une ouverture `PhysicalDrive` avec accès lecture/écriture peut être exigée par Windows pour `IOCTL_SCSI_PASS_THROUGH_DIRECT`, même lorsque toutes les commandes envoyées sont `DATA_IN`. Cette PR ne contourne aucune restriction du système.

Tester ensuite l'application graphique dans PowerShell administrateur :

```powershell
.\build\Release\magicscan-tray.exe "C:\Scans"
```

Vérifier la disparition et le retour du scanner, l'infobulle correspondante et les scans complets après reconnexion. Simuler si possible un refus d'accès sans modifier les permissions système. La confirmation d'une utilisation sans élévation dépend des résultats matériels : ne pas promettre un fonctionnement non administrateur sans preuve.

### Protection contre les instances multiples (PR #12)

L'exécutable graphique `magicscan-tray.exe` utilise un mutex Windows nommé
`Local\\MagicScanTraySingleInstance`. Sa création est atomique, y compris lorsque
deux raccourcis sont lancés presque au même instant. Le premier processus conserve
le mutex pendant toute sa vie. Un deuxième lancement, dans la même session Windows,
quitte **silencieusement, sans boîte de dialogue**, sans initialiser le scanner ni ajouter d'icône dans le systray. Le mutex est libéré
lorsque la première instance s'arrête. Le CLI `magicscan.exe` reste indépendant.

Test : lancer le tray, tenter un second lancement, vérifier un seul systray et
un seul processus actif, quitter la première instance puis relancer normalement.
Répéter avec deux lancements rapprochés.

## PR #13 — Distribution Windows portable (version 0.4.0)

Le processus de build produit une archive `MagicScan-0.4.0-win64.zip` contenant
`magicscan-tray.exe`, `magicscan.exe` et un `LISEZ-MOI.md` en français.
Une vérification de l'archive extraite est exécutée dans GitHub Actions avant
le dépôt de l'artefact CI. L'archive portable ne nécessite pas d'installation.

### Construction locale sous Windows

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\package-windows.ps1 -BuildDir build -OutputDir dist
```

L'archive générée se trouve sous `dist/MagicScan-0.4.0-win64.zip`.
Extrayez-la entièrement avant de lancer `magicscan-tray.exe`. Les droits
administrateur restent nécessaires pour accéder au scanner sur le PC testé ;
voir `docs/WINDOWS-PORTABLE.md`. Aucun pilote ni ACL Windows n'est modifié.

### Publication de version (après validation matérielle et fusion de la PR)

Le workflow `.github/workflows/release.yml` **ne s'exécute pas automatiquement**
lors du merge. Il démarre à la création et au push d'un tag `vX.Y.Z`, vérifie
que celui-ci correspond à la version de `CMakeLists.txt`, compile et teste,
puis publie sur GitHub Releases le ZIP portable avec des notes de version
générées automatiquement.

```powershell
git switch main
git pull origin main
git tag v0.4.0
git push origin v0.4.0
```

Créer le tag **uniquement après** avoir validé le ZIP sur Windows et vérifié
qu'il n'existe pas déjà. Ne pas créer de release en avance : sa publication
est déclenchée par le tag. Les archives ne sont pas signées numériquement ;
aucune prise en charge SmartScreen ou installation en arrière-plan n'est promise.
