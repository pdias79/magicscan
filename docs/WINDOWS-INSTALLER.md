# PR17 — Installateur Windows MagicScan (brouillon)

## Objectif
Un installateur Inno Setup 6 x64 installe l'application dans Program Files,
un service Windows `MagicScanUsb` sous LocalSystem (mode automatique) et un
raccourci de démarrage commun aux utilisateurs. Les utilisateurs standard
peuvent ensuite lancer MagicScan sans élévation. L'installation/désinstallation
nécessite les droits administrateur (invite UAC).

## Construction (Windows)
Préinstaller **Inno Setup 6** depuis son site officiel. Dans PowerShell :

```powershell
cd "C:\Users\Philou\Documents\repos\magicscan"
git fetch origin
git switch feature/windows-installer
git pull --ff-only origin feature/windows-installer
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" ".\installer\MagicScan.iss"
```

L'installateur est généré sous `dist/MagicScan-0.4.0-setup-win64.exe`.
Son numéro suit provisoirement le CMake `0.4.0`; le bump de version
sera traité avec la publication.

## Installation expérimentale
1. Fermer MagicScan sur toutes les sessions et arrêter le service existant
   avec `sc.exe stop MagicScanUsb` dans PowerShell admin. Vérifier
   `sc.exe query MagicScanUsb` => STOPPED avant de mettre à niveau.
2. Lancer l'installateur, confirmer l'UAC.
3. Dans PowerShell admin : `sc.exe qc MagicScanUsb`,
   `sc.exe query MagicScanUsb`. Attendu : AUTO_START et RUNNING.
4. Se connecter à un **compte Windows standard** ; lancer MagicScan depuis
   le menu Démarrer, ou ouvrir une nouvelle session pour vérifier l'exécution
   automatique au démarrage de la session.
5. Tester un scan complet, un scan vide, la reconnexion du service et
   Pause/Quitter. Les PNG doivent appartenir au compte standard.

## Désinstallation
Par **Applications installées** : désinstaller MagicScan sous un compte admin.
Le programme exécute le script de suppression du service ; vérifier ensuite
`sc.exe query MagicScanUsb` (service introuvable). Les paramètres du profil
utilisateur et ses scans ne sont pas effacés.

## Limitations à vérifier
- Installateur non signé : avertissement SmartScreen possible.
- Inno Setup 6 doit être installé uniquement sur la machine de build, non sur
  le poste destinataire.
- Tester mise à niveau d'une installation manuelle PR14–16 et suppression
  depuis une session standard avec UAC.
- En cas d'échec du service, relever la sortie via une exécution PowerShell
  manuelle de `installer/manage-service.ps1` en tant qu'administrateur.
- Ne pas fusionner avant build, installation, usage standard et désinstallation
  sur Windows réel.
