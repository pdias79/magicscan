# PR #14 — Diagnostic du service USB pour un compte Windows standard

**Phase 1 uniquement :** ce prototype vérifie qu'un service Windows peut
identifier le scanner en contexte privilégié et transmettre **un état fixe**
à un utilisateur standard. Il **ne permet pas encore de numériser sans admin** :
l'application systray conserve son accès USB direct jusqu'à une phase ultérieure.

Le service `MagicScanUsb` est installé manuellement par un administrateur,
en compte LocalSystem et en démarrage **manuel**. Le script copie l'exécutable\ndans un dossier protégé sous `%ProgramFiles%\\MagicScan\\Service` ; le service\nne doit jamais être enregistré depuis le dépôt de développement. Il n'accepte aucun chemin,
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


## Phase 2 expérimentale — Acquisition via le service

La validation du diagnostic `--service-status` a été obtenue dans un **véritable
compte Windows standard**. Cette branche expérimente maintenant la transmission
des bandes BGR par un **deuxième pipe local sortant**, avec taille de bande
fixe (2592 × 192 × 3), messages bornés, limites mémoire de 256 Mio et sans
commande arbitraire côté utilisateur. Les commandes SCSI en lecture seule
continuent d'être exécutées uniquement par le service. Le processus systray
reçoit les bandes, construit l'aperçu et écrit les PNG dans le profil utilisateur.

**Attention :** cette phase n'a pas encore été validée sur le matériel réel.
Conservez la version publiée 0.4.0 comme solution de secours.

### Mettre à jour le service de test

Depuis PowerShell **administrateur**, après avoir fermé MagicScan :

```powershell
cd "C:\Users\Philou\Documents\repos\magicscan"
git fetch origin
git switch feature/standard-user-service-foundation
git pull origin feature/standard-user-service-foundation
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\uninstall-service.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\install-service.ps1
sc.exe query MagicScanUsb
.\build\Release\magicscan.exe --service-status
```

La désinstallation/réinstallation est **indispensable** pour remplacer le binaire
du service, initialement installé dans `Program Files`. Ne pas laisser le service
sous LocalSystem exécuter une copie située dans le dépôt de développement.

### Essai sous compte Windows standard

L'utilisateur standard doit avoir une copie des fichiers compilés à laquelle
il peut accéder sans droits administrateur. Il lance `magicscan-tray.exe`
**sans élévation** depuis ce compte, scanner branché, puis vérifie :

1. L'icône systray indique la connexion du scanner ;
2. Le bouton physique SCAN démarre l'aperçu et envoie des bandes ;
3. Un deuxième appui finit le scan, le PNG se trouve dans **son propre dossier** ;
4. Le presse-papiers et les réglages fonctionnent depuis sa session ;
5. Mettre en pause/reprendre et débrancher/rebrancher ne fige pas l'application ;
6. Le service restant actif, `sc.exe query MagicScanUsb` répond RUNNING.

Le service reste en **démarrage manuel** pour ces essais. Après redémarrage du
PC, il devra être lancé par l'administrateur. Aucun ACL de disque ni pilote
n'est modifié. En cas de régression, quitter le systray expérimental et
désinstaller le service avec `uninstall-service.ps1`, puis utiliser la release
0.4.0 en admin. **Ne pas soumettre cette PR avant validation d'un scan complet.**
