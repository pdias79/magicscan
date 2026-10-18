# MagicScan

### Numériser devient un jeu d'enfant !

**MagicScan** est une application gratuite et open source pour **Windows 64 bits**, conçue pour utiliser facilement le scanner portable **IRIScan Book WiFi** en **USB**.

Elle permet notamment aux enfants utilisant une tablette Windows de numériser leurs dessins, exercices et documents sans naviguer dans des menus complexes : **on appuie sur SCAN, on fait glisser le scanner sur la feuille, et le résultat apparaît à l'écran**.

> **Scanner testé : IRIScan Book WiFi.** Il est identifié techniquement sous le nom **NOVA**. La compatibilité d'autres modèles reste à vérifier. MagicScan utilise une liaison **USB**, pas le Wi-Fi.

## En trois gestes

1. **Je pose ma feuille** bien à plat et je place le scanner au début du dessin ou du texte.
2. **J'appuie sur SCAN**, je fais glisser doucement le scanner sur le papier, puis **j'appuie à nouveau sur SCAN** pour terminer.
3. **Je retrouve mon image à l'écran** : je peux la regarder, la copier ou l'enregistrer.

La prévisualisation s'affiche automatiquement pendant la numérisation. Une fois MagicScan installé par un adulte, l'enfant peut l'utiliser depuis un **compte Windows standard**, sans droits administrateur.

## Mode d'emploi pour les enfants

### Que font les boutons ?

Dans la fenêtre de prévisualisation, les boutons sont identifiés par des textes en français.

| Bouton | Ce que je peux faire |
| --- | --- |
| **Copier** | Copier l'image pour la coller dans Paint, Word ou un autre logiciel. |
| **Enregistrer** | Choisir un fichier **PNG**, **JPEG** ou **PDF**. |
| **Effacer** | Retirer le cadre de sélection, sans supprimer le dessin. |
| **Ajuster** | Faire tenir l'image entière dans la fenêtre. |
| **Zoom +** / **Zoom -** | Voir l'image de plus près ou de plus loin. |
| **Tourner** | Remettre le dessin dans le bon sens. |
| **Réglages** | Modifier le gamma, la luminosité et le contraste. |

### Garder seulement une partie du dessin

1. Je clique (ou je touche l'écran) au début de la zone à conserver.
2. Je fais glisser jusqu'au coin opposé, puis je relâche.
3. Je clique sur **Copier** ou **Enregistrer** : seule la zone sélectionnée est utilisée.
4. Pour revenir à l'image entière, je clique sur **Effacer** (cela efface **la sélection**, pas l'image).

### Éclaircir ou améliorer la lisibilité

Je clique sur **Réglages**. Trois curseurs permettent de modifier l'apparence :

- **Luminosité** : éclaircir ou assombrir.
- **Contraste** : mieux distinguer les zones sombres et claires.
- **Gamma** : ajuster les tons de l'image.

Je vois le résultat immédiatement dans la prévisualisation.

### Enregistrer ou partager

Je clique sur **Enregistrer**, je choisis où ranger le fichier et je sélectionne un format :

- **PNG** : pour conserver un dessin ou une image.
- **JPEG** : pour partager une image avec un fichier souvent plus léger.
- **PDF** : pour conserver une image dans un document d'une page A4.

Je peux également cliquer sur **Copier**, ouvrir Paint ou Word, puis utiliser **Ctrl + V** pour coller l'image.

Si la sauvegarde automatique est activée dans les paramètres, MagicScan conserve aussi les numérisations dans le dossier prévu.

## Installer MagicScan (pour les adultes)

**Matériel requis :** tablette ou ordinateur Windows 64 bits, scanner **IRIScan Book WiFi**, connexion USB fonctionnelle. Un administrateur Windows doit réaliser **l'installation initiale**.

**[Télécharger MagicScan v0.5.0 pour Windows](https://github.com/pdias79/magicscan/releases/download/v0.5.0/MagicScan-0.5.0-setup-win64.exe)**

[Voir la page des versions et les téléchargements](https://github.com/pdias79/magicscan/releases)

1. Télécharger et lancer l'installateur Windows.
2. Accepter l'autorisation administrateur (UAC).
3. Suivre les indications à l'écran.
4. Brancher le scanner en USB.

L'installateur configure le service **MagicScanUsb** (démarrage automatique) et crée un raccourci **MagicScan** dans le menu Démarrer. L'application peut également démarrer à l'ouverture de la session.

**Pour ouvrir MagicScan :** touche **Windows** → taper **MagicScan** → sélectionner l'application. Elle reste ensuite présente près de l'horloge, dans la zone de notification Windows.

L'installateur n'est pas signé numériquement : Windows SmartScreen peut afficher un avertissement. Téléchargez-le uniquement depuis la page officielle du projet et vérifiez sa provenance.

### Où retrouver les numérisations ?

Par défaut, les fichiers automatiquement enregistrés se trouvent dans le dossier :

`C:\Users\<utilisateur>\Pictures\MagicScan`

Le chemin peut être modifié dans les paramètres. Pour ouvrir directement ce dossier, **double-cliquer** sur l'icône MagicScan près de l'horloge, ou faire **clic droit → Ouvrir le dossier des scans**.

Si la sauvegarde automatique est désactivée, utilisez **Enregistrer** dans la prévisualisation pour conserver le fichier.

## Utiliser l'icône MagicScan près de l'horloge

Un **clic droit** sur l'icône ouvre les commandes :

- **Mettre en pause** / **Reprendre la surveillance** : arrêter ou reprendre l'attente des scans.
- **Ouvrir le dossier des scans** : retrouver les fichiers.
- **Paramètres...** : régler le fonctionnement.
- **À propos...** : informations sur MagicScan.
- **Quitter** : fermer l'application.

Fermer la fenêtre de prévisualisation **ne quitte pas MagicScan** : l'application continue de fonctionner en zone de notification.

### Paramètres (adultes ou enseignants)

Depuis **clic droit → Paramètres...**, vous pouvez choisir :

- l'enregistrement automatique des numérisations ;
- la copie automatique dans le presse-papiers ;
- le dossier de sauvegarde ;
- la valeur de gamma par défaut.

Ces réglages permettent d'adapter l'application aux usages d'une classe ou d'un foyer.

## Scanners compatibles

| Scanner | État |
| --- | --- |
| **IRIScan Book WiFi** | **Testé et compatible** via USB |
| Autres modèles **IRIScan Book** | Non testés |
| Autres scanners portables USB | Non testés |

**NOVA** est l'identification technique rencontrée avec le modèle validé ; ce n'est pas son nom commercial. Un nom ou une apparence similaire ne garantit pas la compatibilité du protocole USB.

Vous possédez un autre scanner ? Ouvrez une [issue GitHub](https://github.com/pdias79/magicscan/issues) avec son modèle exact et, si possible, son identification USB. **Ne supposez pas que la connexion Wi-Fi du scanner est prise en charge : MagicScan communique actuellement en USB.**

## Petits problèmes fréquents

| Problème | Que faire ? |
| --- | --- |
| Rien ne se passe lorsque j'appuie sur SCAN | Vérifier le câble USB, la présence de MagicScan près de l'horloge et que la surveillance n'est pas en pause. |
| Je ne vois pas la fenêtre | Démarrer une numérisation ; la prévisualisation doit s'afficher automatiquement. |
| Mon dessin est trop sombre | Ouvrir **Réglages** et ajuster la luminosité ou le gamma. |
| Je ne retrouve pas mon fichier | Clic droit sur l'icône → **Ouvrir le dossier des scans** ; vérifier aussi que la sauvegarde automatique est activée. |
| MagicScan signale un problème de connexion | Demander à un adulte de vérifier le branchement et le service Windows **MagicScanUsb**. |

## Quelques idées pour la classe

- **Arts plastiques** : numériser des dessins pour créer un album numérique.
- **Écriture** : récupérer un texte manuscrit pour le présenter à l'écran.
- **Sciences** : conserver un croquis réalisé sur papier.
- **Projets collectifs** : copier une réalisation dans une présentation.

MagicScan fait le lien entre **le papier et la création numérique**.

## Pour les développeurs

MagicScan est développé en **C++17** avec l'API **Win32**. Le projet comprend :

- `magicscan-tray.exe` : interface graphique, surveillance et prévisualisation ;
- `magicscan.exe` : diagnostic et ligne de commande ;
- `magicscan-usb-service.exe` : service Windows USB utilisé par les comptes standards ;
- un installateur Windows Inno Setup.

### Compiler sous Windows

Prérequis : Visual Studio avec les outils de compilation C++ et CMake.

```powershell
git clone https://github.com/pdias79/magicscan.git
cd magicscan
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Voir également la [documentation de l'installateur Windows](docs/WINDOWS-INSTALLER.md).

Pour signaler un bug ou proposer une amélioration, utilisez les [issues GitHub](https://github.com/pdias79/magicscan/issues).

---

**MagicScan — un scanner, un bouton, et les créations prennent vie à l'écran.**
