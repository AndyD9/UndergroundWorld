# Compilation et diagnostic

Prérequis : Windows, Visual Studio 2022 Build Tools avec Desktop development with
C++, SDK Windows, CMake >= 3.24. Installés le 19 octobre 2026 via winget :

```powershell
winget install --id Kitware.CMake -e
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --includeRecommended"
```

Versions constatées : CMake 4.4.4, MSVC 19.44.35229 (Build Tools 17.14), SDK
10.0.26100.0. Ouvrir un nouveau terminal après installation pour que CMake soit
dans PATH.

Depuis la racine, avec les outils installés :

```powershell
cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32
cmake --build build-x86 --config Release
dumpbin /headers build-x86\Release\UGWorld.asi
dumpbin /exports build-x86\Release\UGWorld.asi
python tools/inspect_binary.py research/binaries/SPEED2.EXE --output research/findings/binary-manifest.json
```

Vérifier machine 14C et l'export UGWorldInitialize (nom x86 stdcall potentiellement
décoré `_UGWorldInitialize@0`). Un build x64 doit être refusé à la configuration.
Aucune dépendance ENet n'est téléchargée pendant la configuration.

Amorçage : un ASI loader n'appelle que DllMain. Sur `DLL_PROCESS_ATTACH`, DllMain
prend une référence sur le module (`GetModuleHandleExW`) et lance un unique thread
sans l'attendre ; ce thread ne s'exécute qu'après la libération du verrou du
chargeur, appelle `UGWorldInitialize` puis rend la référence via
`FreeLibraryAndExitThread`. Si la création du thread échoue, la référence est
volontairement conservée (pas de `FreeLibrary` dans DllMain). Depuis la phase 3b,
une empreinte reconnue démarre la télémétrie lecture seule (hook de frame), décrite
dans [telemetry.md](telemetry.md) ; une empreinte inconnue ne démarre rien.

L'export vérifie la taille et le SHA-256 du fichier de l'exécutable courant via
Windows CNG. Une erreur ou une empreinte inconnue désactive l'initialisation pour
la durée de chargement. Les appels concurrents ne relancent pas l'initialisation.
Journal : `%TEMP%\UGWorld-diagnostic.log` et OutputDebugString.

Reconnaître le fichier disque ne prouve ni l'intégrité de l'image chargée ni la
compatibilité d'un hook. Les accès moteur restent donc désactivés dans tous les
cas. Avant de futurs hooks : vérifier les octets en mémoire, ASLR, les autres
mods et les conventions d'appel, puis documenter le désarmement et l'unload.

## Résultats du 19 octobre 2026

- Build Release Win32 sans avertissement en `/W4 /permissive-`.
- `dumpbin` : machine 14C, Dynamic base, NX ; export unique `_UGWorldInitialize@0`
  (décoré stdcall) ; dépendances limitées à `bcrypt.dll` et `KERNEL32.dll` grâce au
  CRT statique (`/MT`), donc pas de redistribuable VC++ requis. Taille 91 136 octets.
- Configuration `-A x64` refusée avec le message attendu.
- Hôte synthétique x86 (hors dépôt) : `LoadLibrary` en 78 ms, le thread d'amorçage
  journalise un refus « unknown/unreadable executable » ; quatre appels concurrents
  à l'export renvoient `ERROR_NOT_READY` (21) sans relancer l'empreinte ni journaliser.

- **Chargement en jeu (profil moddé)** : `UGWorld.asi` (SHA-256 `a70d381d…6533`) copié
  dans `scripts/` à côté d'ExtraOptions, Unlimiter et WidescreenFix, chargé par
  l'Ultimate ASI Loader existant. Partie lancée par l'utilisateur : journal unique
  « disk fingerprint recognized » à 17:52:09, aucun plantage signalé ni événement
  d'erreur Windows pour `SPEED2.EXE`. **Critère de sortie de phase 2 atteint.**

Restent à faire, non bloquants : profil vanilla (sans les autres ASI), fichier
inaccessible, journal non inscriptible.
