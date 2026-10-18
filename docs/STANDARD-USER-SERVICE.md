# PR #14 — Diagnostic du service USB pour un compte Windows standard

**Phase 1 uniquement :** ce prototype vérifie qu'un service Windows peut
identifier le scanner en contexte privilégié et transmettre **un état fixe**
à un utilisateur standard. Il **ne permet pas encore de numériser sans admin** :
l'application systray conserve son accès USB direct jusqu'à une phase ultérieure.

Le service `MagicScanUsb` est installé manuellement par un administrateur,
en compte LocalSystem et en démarrage **manuel**. Il n'accepte aucun chemin,
aucun paramètre et aucune commande SCSI depuis l'utilisateur. Sa seule réponse
est un octet : scanner identifié par SCSI NOVA, ou indisponible.

La communication emploie un pipe nommé Windows local, refusant les connexions
réseau ; sa DACL laisse uniquement la lecture aux utilisateurs standard,
tandis que SYSTEM et les administrateurs en ont le contrôle. L'état renvoyé
ne contient ni numéro de disque, ni données de numérisation.

## Installation de test (administrateur seulement)

Depuis le dépôt, ouvrir **PowerShell en tant qu'administrateur** :

```powershell
git fetch origin
git switch feature/standard-user-service-foundation
git pull origin feature/standard-user-service-foundation
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\install-service.ps1
sc.exe query MagicScanUsb
.\build\Release\magicscan.exe --service-status
```

Fermer le systray administrateur si nécessaire pour le test d'accès exclusif.
Le service est démarré mais pas configuré pour démarrer automatiquement.

## Test du compte Windows standard

Se connecter sous le **vrai compte utilisateur standard** (pas une console
élevée sous le compte administrateur). Ouvrir PowerShell dans le dépôt ou
copier uniquement `magicscan.exe` depuis `build\Release` vers un dossier
accessible à ce compte, puis :

```powershell
.\magicscan.exe --service-status
```

Attendu avec scanner branché : `Service MagicScan USB : scanner identifie
(NOVA OK).` (code de retour 0). Si `scanner absent ou inaccessible`,
le service existe mais n'a pas identifié le périphérique. Si `service
indisponible` ou `connexion refusee`, relever le code Windows.

Cette commande ne demande pas d'élévation et n'accède pas aux disques physiques.

## Nettoyage (administrateur)

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\uninstall-service.ps1
```

Avant suppression, fermer les instances qui interrogent le service. Le service
et son exécutable sont distincts : ne pas déplacer ni effacer l'exécutable
tant que le service est installé. La suppression ne modifie aucune ACL de disque,
aucun pilote ni paramètre USB.

## Étapes suivantes après validation sur matériel

1. Protocole IPC versionné pour les évènements de bandes et l'annulation ;
   taille des messages bornée, aucune commande SCSI arbitraire.
2. Exécuter l'acquisition dans le service et transmettre les bandes au GUI
   non élevé. Le service ne doit pas écrire de fichiers utilisateur.
3. Gérer les déconnexions, la reprise, l'arrêt du service et la fermeture de
   session, puis installer la variante service dans un dossier protégé
   avec permissions administrateur uniquement.
4. Tester le flux complet dans le compte utilisateur standard avant de
   promettre une utilisation sans élévation ou de créer une release.

**Ne pas installer ce prototype sur une machine sans pouvoir le désinstaller.**
