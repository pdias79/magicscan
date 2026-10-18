# MagicScan — Version portable Windows

Cette archive contient **MagicScan pour Windows 10/11 x64**. Elle ne nécessite pas
d'installateur ; extrayez l'archive dans un dossier de votre choix.

## Contenu

- `magicscan-tray.exe` : interface graphique dans la zone de notification.
- `magicscan.exe` : outils de diagnostic en ligne de commande.

## Premier lancement

1. Branchez et allumez le scanner IRIScan Book 5 WiFi en mode USB.
2. Extrayez **tout le ZIP** (ne lancez pas l'exécutable depuis l'archive).
3. Sur les machines testées, Windows refuse l'accès SCSI sans élévation.
   Ouvrez **PowerShell en tant qu'administrateur**, rendez-vous dans le dossier
   extrait puis exécutez :

```powershell
.\magicscan-tray.exe
```

Vous pouvez aussi préciser un dossier lors du premier lancement :

```powershell
.\magicscan-tray.exe "C:\Scans"
```

Les paramètres (dossier, enregistrement automatique, presse-papiers, gamma)
sont conservés dans `%APPDATA%\MagicScan\settings.ini`.

MagicScan fonctionne en zone de notification : clic droit sur l'icône
pour **Paramètres...**, **Mettre en pause**, **Ouvrir le dossier des scans**
ou **Quitter**. Un deuxième lancement dans la même session Windows quitte
silencieusement et ne crée pas de seconde icône.

## Diagnostic d'accès

D'abord dans PowerShell normal, puis en administrateur, exécutez :

```powershell
.\magicscan.exe --diagnose-access
```

Si le diagnostic indique `Windows error 5` sans administrateur mais
`NOVA SCSI identification: OK` en administrateur, conservez l'élévation
pour l'application. **Ne modifiez pas les autorisations des disques physiques
ni les pilotes du scanner.**

## Mise à jour / suppression

Quittez MagicScan, puis remplacez le dossier extrait par la nouvelle version.
Les paramètres enregistrés dans votre profil Windows sont conservés.
Pour désinstaller la version portable, supprimez son dossier ; supprimez
`%APPDATA%\MagicScan` seulement si vous souhaitez effacer vos préférences.

Il n'y a pas de service, de démarrage automatique ni de droits d'accès USB
modifiés par cette archive.
