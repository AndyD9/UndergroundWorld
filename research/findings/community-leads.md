# Pistes communautaires — aucune adresse validée sémantiquement

Ce registre conserve l'état initial des pistes communautaires. Une validation
statique partielle ultérieure sur l'empreinte exacte figure dans
[static-vehicle-path.md](static-vehicle-path.md). Aucune validation runtime.

Cible : SHA-256 `f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`. Le responsable de l'analyse a retrouvé une copie locale de 4 800 512 octets correspondant à cette empreinte. Ce document analyse les sources communautaires, pas l'exécution du jeu; il ne confirme aucun pointeur, hook, offset de structure ou ABI sur cette cible.

## Contrôle de version

[Extra Options, dllmain.cpp lignes 11–20](https://github.com/ExOptsTeam/NFSU2ExOpts/blob/801539e248cea6a91769dce72e7f086753a29423/NFSU2ExtraOptions/dllmain.cpp#L11-L20) et [Unlimiter, dllmain.cpp lignes 13–22](https://github.com/nlgxzef/NFSU2Unlimiter/blob/45b6738378d76755d0dd0a19a68d26cfa70af250/dllmain.cpp#L13-L22) ramènent le point d'entrée PE à une base de référence `0x400000` et le comparent à `0x75BCC7` (RVA `0x35BCC7`). Le message d'erreur mentionne NTSC v1.2 et 4 800 512 octets, mais le contrôle lui-même ne vérifie ni taille ni SHA-256. Notre adaptateur doit refuser toute empreinte inconnue; ce test communautaire n'est pas une validation suffisante.

## Candidats traçables

Les RVA ci-dessous sont **des soustractions arithmétiques** `VA communautaire − 0x400000`. Il faut confirmer ImageBase et sections du PE cible avant leur utilisation. Confiance : **élevée sur la transcription des sources; inconnue sur le sens et la compatibilité cible** pour toutes les lignes. Aucune de ces valeurs ne doit activer un hook de production à ce stade.

| Objet / symbole communautaire | VA candidate | RVA calculée | Preuve à la révision figée | Vérification suivante |
| --- | --- | --- | --- | --- |
| `_TheGameFlowManager` | `0x8654A4` | `0x4654A4` | Extra Options GlobalVariables.h:7; HotkeyStuff.h utilise l'état 6 | Xrefs, transitions menu/chargement/conduite et stabilité du stockage. |
| `_PlayersByNumber` | `0x8900AC` | `0x4900AC` | Extra Options GlobalVariables.h:18; HotkeyStuff.h:80–91 déréférence ce stockage | Type exact, tableau ou pointeur, durée de vie, joueur local, liens vers véhicule. |
| `Player_AutoPilotOn` / `Off` | `0x5FAE20` / `0x5FAF10` | `0x1FAE20` / `0x1FAF10` | Extra Options InGameFunctions.h:16–17 | Désassemblage, thiscall présumé, appelants et effets; ce ne sont pas des setters de transform. |
| `Player_SetInputMode` | `0x605C70` | `0x205C70` | Extra Options InGameFunctions.h:47 | ABI, valeurs de mode et thread d'appel. |
| `_SaveHotPosition` / `_JumpToHotPosition` | `0x8900CC` / `0x8900CD` | `0x4900CC` / `0x4900CD` | Extra Options GlobalVariables.h:12–13; HotkeyStuff.h:150–160 | Xrefs vers lecteurs et stockage réel du transform; ce sont des indicateurs, pas des positions. |
| `_Tweak_GameSpeed` | `0x7A5730` | `0x3A5730` | Extra Options GlobalVariables.h:25 | Identifier lecteurs et unité; ne constitue pas une boucle de simulation. |
| `_CarTypeInfoArray` | `0x8A1CCC` | `0x4A1CCC` | Unlimiter GlobalVariables.h:10 | Pointer indirection, taille, compte, mapping modèle et ownership. |
| `GetCarTypeInfo_Game` | `0x610110` | `0x210110` | Unlimiter InGameFunctions.h:11 | Fonction de consultation candidate; ne pas la confondre avec une création de véhicule. |
| `CompleatCarPhysicsInfo_BuildCarPhysicsInfo` | `0x5AC4F0` | `0x1AC4F0` | Unlimiter InGameFunctions.h:107 | ABI, allocation et appelants; construction de paramètres présumée, pas simulation par frame. |
| `CarRenderInfo_TriggerEffect_Game` | `0x615CC0` | `0x215CC0` | Unlimiter InGameFunctions.h:275 | Paramètres matrice/vecteur et contexte de rendu; ne prouve aucun accès au transform physique. |

Liens immuables pour les références du tableau :

- [Extra Options GlobalVariables.h](https://github.com/ExOptsTeam/NFSU2ExOpts/blob/801539e248cea6a91769dce72e7f086753a29423/NFSU2ExtraOptions/GlobalVariables.h)
- [Extra Options HotkeyStuff.h](https://github.com/ExOptsTeam/NFSU2ExOpts/blob/801539e248cea6a91769dce72e7f086753a29423/NFSU2ExtraOptions/HotkeyStuff.h)
- [Extra Options InGameFunctions.h](https://github.com/ExOptsTeam/NFSU2ExOpts/blob/801539e248cea6a91769dce72e7f086753a29423/NFSU2ExtraOptions/InGameFunctions.h)
- [Unlimiter GlobalVariables.h](https://github.com/nlgxzef/NFSU2Unlimiter/blob/45b6738378d76755d0dd0a19a68d26cfa70af250/GlobalVariables.h)
- [Unlimiter InGameFunctions.h](https://github.com/nlgxzef/NFSU2Unlimiter/blob/45b6738378d76755d0dd0a19a68d26cfa70af250/InGameFunctions.h)

## Lacunes explicites

**Transform véhicule, création/destruction d'un véhicule monde et tick de simulation : non identifiés.** Les références matrices de rendu, paramètres de physique et boutons de sauvegarde de position constituent uniquement des points de départ. Aucun offset position/rotation/vitesse, prototype de factory, ni point de hook de simulation n'est déduit de ces noms.

Validation requise : importer la cible dans REA/Ghidra; confirmer sections et octets aux candidats; suivre les xrefs et appelants; documenter ABI et ownership; puis observer des transitions contrôlées dans le jeu. Il faudra distinguer véhicule local, IA, objet de menu et objet rendu, vérifier les unités et axes, et reproduire après rechargement de course. Une signature de bytes doit être extraite du binaire exact, accompagnée de sa plage et de masques motivés; aucune signature générique n'est inventée ici.

Aucun code communautaire n'a été copié dans l'implémentation. Les licences et révisions figurent dans [sources.md](../../docs/sources.md).
