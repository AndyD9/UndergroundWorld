# Passe statique 2 (phase 3a) — vitesse, GameFlow, hook, véhicules monde

Examen du 19 octobre 2026, **sans lancer le jeu**. Suite de
[static-vehicle-path.md](static-vehicle-path.md).

## Portée, méthode et conventions

- Binaire : `SPEED2.EXE`, SHA-256
  `f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`, base
  `0x00400000`. Toutes les adresses sont des VA ; RVA = VA − `0x400000`. Pour
  `.text` et la partie initialisée de `.data`, l'offset fichier est égal à la RVA
  (manifeste : `raw_offset == rva`).
- Ghidra 12.1.4, base existante ouverte avec `-readOnly -noanalysis`. Nouveau script
  `research/ghidra/ExportQueries.java` (fonctions avec octets, xrefs, listings hors
  fonction, scans de constantes et de motifs) lancé par `tools/query-ghidra.ps1`.
  Vingt exports `research/ghidra/exports/p3a-pass1..20.json`, locaux et ignorés
  par Git ; empreintes et requêtes listées dans
  [static-export-index.json](static-export-index.json).
- Les octets du site de hook ont été relus **directement dans le fichier**, en plus de
  Ghidra : résultats identiques.
- Noms communautaires utilisés comme indices seulement : commentaires de la source
  figée NFSU2Unlimiter (`Car::dtor` pour 0x005F4604, `TrafficTeleporter::DoTimeStep`
  pour 0x0042F9B3, `TrackLoader::LoadHandler` pour 0x0057F992,
  `BeginGameFlowLoadingFrontEnd` pour 0x00580080), commentaire
  ExtraOptions (« 3 = FE, 4&5 = Loading screen, 6 = Gameplay »).
- **Observation** = octets, xrefs ou instructions relus. **Interprétation** = sens
  déduit. Aucune signature C++ n'est prouvée : seules les conventions visibles
  (registre `ECX`, `RET n`) sont rapportées.
- V désigne l'objet véhicule (« Car », 0x9E0 octets), P l'objet joueur (0x9C0
  octets), W le monde/gestionnaire pointé par `[0x00890080]`.

## Synthèse

| Point | Conclusion | Confiance |
| --- | --- | --- |
| 1. Vitesse/orientation | Chaîne complète par frame jusqu'aux écritures de V+0x60, V+0x70/74/78, V+0x90…, V+0x40C, V+0x42C, V+0x430. Producteur principal : modèle physique de V+0x34, appelé par `0x006012B0`. `dt` en secondes, variable par frame, sous-pas dans le modèle 1. | élevée pour la chaîne, moyenne pour le modèle effectivement actif |
| 2. GameFlow | Valeurs écrites 1 à 8 (10 sites), transitions via une file différée exécutée en début de frame. Pas de valeur distincte pour garage ou free roam. | élevée pour les écritures, moyenne pour les noms |
| 3. Hook | `CALL 0x00609CE0` à **0x00581407** (`E8 D4 88 08 00`), après mise à jour des joueurs et de la physique. Non patché par les sources figées ni par les constantes des ASI installés. | moyenne-forte |
| 4. Phase 4 | Voitures créées une seule fois au chargement de piste et détruites au déchargement ; le joueur 1 est prévu par le moteur mais sa présentation (caméra/HUD) reste inconnue. | moyenne |

## 1. Vitesse et orientation

### 1.1 Chaîne d'appel par frame (observation)

| Étape | VA (RVA) | Octets / preuve | Observation |
| --- | --- | --- | --- |
| Boucle principale | `0x00581407` (`0x181407`) | `E8 D4 88 08 00` | `CALL 0x00609CE0`, ECX = `[0x00890080]`, argument `dt` poussé à `0x00581406`. |
| Tick du monde | `0x0060A032` (`0x20A032`) | `E8 C9 6E FE FF` | `CALL 0x005F0F00` avec `dt` (`[EBP+8]`). |
| Liste physique | `0x005F0F5B` (`0x1F0F5B`) | `FF 50 2C` | Parcourt la liste `0x0086B4C8` (nœud à objet+4), garde les objets dont l'octet +0x12 ≠ 0, appelle `vtable[11](dt)`. |
| Objet simulé « Car%d » | vtable `0x007A2470`, entrée `0x007A249C` = `0x005A9F10` | scan : seule occurrence de cette valeur | Constructeur `0x005AEC20` : vtable à `0x005AEC5F`, `+0x130 = V` à `0x005AEC9C`. La chaîne « Car%d » précède la vtable. |
| Mise à jour objet | `0x005A9F10` (hors fonction Ghidra) | `0x005A9F25 E8 86 73 05 00` | Appelle `0x00593210(dt)`, puis `0x006012B0(dt)` avec ECX = `[obj+0x130]` = V. |
| Mise à jour de V | `0x006012B0` (`0x2012B0`), `RET 4`, retourne un flottant | `0x006012C1 8B 4E 34`, `0x006012CB FF 50 04` | Charge le modèle `[V+0x34]` et appelle `modèle->vtable[1](dt)`. |

Le tick `0x00609CE0` appelle aussi, pour chaque véhicule de `W+0x1C`, le contrôleur
`[V+0x2C]->vtable[3](dt)` (`0x00609F0E`, `0x00609F4E`, `0x00609F8D`) et le
pilote `[V+0x30]->vtable[1](dt)` (`0x00609FC2`) ; puis `0x0042F910`
(TrafficTeleporter, `0x00609ED7`) et enfin la boucle joueurs
`0x0060A240–0x0060A254` (`0x00602650`).

**Interprétation :** `0x00602650` est la logique joueur (réinitialisations,
minuteries, détection d'immobilisation), **pas** l'intégrateur. L'intégration a lieu
avant, dans `0x005F0F00` → `0x006012B0` → modèle physique.

### 1.2 Modèles physiques de V+0x34 (observation)

`0x005F46A0` choisit le modèle. Ghidra ne l'a pas reconnu comme fonction, mais
46 appels directs y mènent ; les octets `C3 90 90 90 90 90` puis
`6A FF 68 A2 9B 77 00` à `0x005F4690–0x005F46A6` marquent son prologue.
Le type est stocké en V+0x28, l'objet en V+0x34, la table de sauts est à
`0x005F493C` (6 entrées). Après création, `[modèle+4] = [V+0x30]` (`0x005F4905`)
et les tables globales `0x0089CD50[id]` (type) et `0x0089CDA8[id]` (modèle) sont
mises à jour.

| Type | Allocation | Constructeur | vtable | `vtable[1]` (mise à jour) |
| --- | --- | --- | --- | --- |
| 0 | pool `[0x0086B304]` via `0x00440E50` | en ligne (`0x005F4768`), `+8 = V+0x40` | `0x007A5BB0` | `0x005EB920` : appelle `0x005923E0` sur V+0x40, aucune écriture de vitesse vue |
| 1 | `0x130` octets | `0x005B5CC0` | `0x007A25E0` | `0x005B72D0` |
| 2 | `0x24` octets | `0x00408740` | `0x0078456C` | `0x00408820` |
| 3 | pool `[0x0086B300]` | `0x005B6D00` | `0x007A2538` | `0x005AAD90` (non analysé) |
| 4 | `0x4C` octets | `0x005A9030` | `0x007A2144` | `0x0059EC80` |
| 5 | `0x00440820(0xA0)` | `0x005B6FC0` | `0x007A2590` | `0x005AB3C0` (non analysé) |

Règle observée à `0x005F471E–0x005F473E` : si l'octet V+0x5C0 vaut 0 et que le type
demandé n'est ni 0 ni 4, le type devient 0 quand V+0x24 = 1 (contrôleur humain),
sinon 4. Un forçage global `[0x008021AC]` ≠ −1 s'applique aux véhicules dont
l'enregistrement a le type 1 (`0x005F46BB–0x005F46E4`).

**Interprétation, confiance moyenne :** le modèle 1 (sous-pas, corps rigide) est la
dynamique complète des voitures de course quand V+0x5C0 est armé ; le modèle 4 est
une cinématique de repli. Le pilote humain `0x0041E2C0` teste `[V+0x28] == 1`
(`0x0041E3A4`). Le modèle réellement actif pour le joueur en course reste à mesurer.

### 1.3 Producteurs de V+0x70/74/78, V+0x430 et de l'orientation

**Modèle 1, observation forte.** Le constructeur `0x005B5CC0` range V en
`modèle+0x5C` et **V+0x40 en `modèle+0x58`** (`0x005B5D16–0x005B5D31`) ;
`modèle+0x20` reçoit l'état rigide créé par `0x005A7F30` (`0x005B5D7D`).
`0x005B72D0` calcule `n = ftol(dt / [0x007FC070]) + 1` et `h = dt / n`
(`0x005B7344–0x005B736A`), exécute `n` sous-pas (`[0x007FC800] = h` à `0x005B73C8`,
intégration `0x005AA480(h)`), puis appelle `0x005A5540` (`0x005B74D1`). Cette
routine recopie l'état rigide dans le sous-objet V+0x40 :

| Écriture | Instructions | Champ V | Interprétation |
| --- | --- | --- | --- |
| position | `0x005A555F`, `0x005A5562`, `0x005A5565` | V+0x60/64/68 | position monde (déjà connue) |
| matrice | `0x005A5576 CALL 0x004012B0` (destination V+0x90, source état+0x30) | V+0x90… | base d'orientation, lignes de 16 octets |
| seconde matrice | `0x005A559C CALL 0x004012B0` (destination V+0xD0, source V+0x90) | V+0xD0… | dérivée de la base (transposée ou inverse ?, faible) |
| vitesse | `0x005A55C0`, `0x005A55C3`, `0x005A55C6` (depuis état+0xA0/A4/A8) | **V+0x70/74/78** | vitesse linéaire |
| norme | `0x005A55F0` après `FSQRT` à `0x005A55E8` | **V+0x430** | vitesse scalaire |
| vecteur | `0x005A5617`, `0x005A561A`, `0x005A561C` (depuis état+0xC0…) | V+0x110/114/118 | vitesse angulaire probable |
| cap | `0x005A562A CALL 0x0043C570(V+0x90, V+0x94)` puis `0x005A5635` (mot) | **V+0x40C** | cap 16 bits issu de la première ligne de la base |
| vitesse avant | `0x005A563F–0x005A5655` produit scalaire de V+0x70… et V+0x90… | V+0x42C | vitesse longitudinale signée |

**Modèle 4, observation.** `0x0059EC80` prend V dans `[this+8]` et le sous-objet
V+0x40 dans EDI (`0x0059ED05–0x0059ED08`), écrit V+0x90…0xAC
(`0x0059F18C–0x0059F1C6`), appelle `0x005F5010` (`0x0059F09E`, constructeur de base
de la phase 1), divise par `dt` (`0x0059F22F`) puis appelle `0x00592160`
(`0x0059F2DF`), qui écrit V+0x70/74/78 et V+0x430. V+0x42C est écrit à
`0x0059F2E8`. Une branche dégénérée remet la vitesse à zéro (`0x0059F4EB–0x0059F500`).
**Modèle 2 :** `0x00408820` appelle aussi `0x005F5010` (`0x00408A90`) et
`0x00592160` (`0x00408B01`) après une division par `dt` (`0x00408B4C`).

Autres écrivains de la norme V+0x430 (sous-objet+0x3F0) : `0x00592160`
(setter), `0x0059EB90` (direction × vitesse, appelé par `0x005A92F0`),
`0x0059F880` (remise à zéro appelée par la téléportation `0x005F52C0`) et
`0x005A2B60` (non suivi).

**Interprétation :** la base V+0x90/A0/B0 est réécrite à chaque pas par le modèle
actif ; ce n'est pas seulement une valeur de repositionnement. La première ligne
sert au cap et à la vitesse avant : c'est probablement l'axe avant. L'ordre des deux
autres axes et le sens du repère restent à mesurer.

### 1.4 Pas temporel

- `dt` est un flottant stocké en `0x00865154` à `0x0058133E`, `0x005813A3`,
  `0x005813E1`, `0x00581428` (toujours après `0x00601A60`) et `0x0058145F`
  (copie de `[0x00865198]` quand GameFlow ≠ 6).
- Chemin normal : `0x005813CA CALL 0x005EA360` (W) → `0x005813D8 CALL 0x00601A60`
  (ECX = `0x0089CF48`) → `FST [ESP+0x14]` et `FSTP [0x00865154]` → argument de
  `0x00609CE0`. `0x00601A60` multiplie par `[0x007A5730]` = 1,0 (`0000803F`) et
  compte les frames où `dt > [0x00784504]` ≈ 0,0769 (`D9899D3D`).
- Unité : `0x00602764–0x00602770` convertit `dt × [0x00784264] + [0x00784260]`
  en entier, soit `dt × 4000,0 + 0,5` (`00007A45`, `0000003F`). Le compteur entier
  `0x0086518C` est multiplié par `[0x00784268]` = 0,00025 (`6F128339`).
  **Interprétation, confiance moyenne-forte :** `dt` en secondes, horloge au 1/4000 s.
- La boucle mesure le temps réel par frame : `0x005812DC CALL 0x0043BE20`, résultat
  × `[0x0078435C]` = 0,001 (`6F12833A`), passé à `0x0057EAD0`.
- Sous-pas du modèle 1 : `[0x007FC070]` = `E02D903B` ≈ 0,0044 s dans le fichier
  (section `.data`, modifiable). `[0x007FC800]` est remis à 0 à `0x00609CF0` et
  `0x0060A0AA`.
- **Pas d'accumulateur à pas fixe** observé dans `0x005811C0` : un tick par frame,
  `dt` variable. WidescreenFix installé avec `FPSLimit = -1` et une mention de
  « SimRate » dans son ini : il peut modifier la cadence. **Inconnue.**

### 1.5 Contrôleurs V+0x2C (pilotage, pas intégration)

`0x005EBA30` remplace le contrôleur (`V+0x24` = type, ancien objet détruit par
`vtable[0](1)`), puis `V+0x30 = [contrôleur+8]` et `[[V+0x34]+4] = V+0x30`.

| Type | Taille | Constructeur | vtable | `vtable[3]` (mise à jour) |
| --- | --- | --- | --- | --- |
| 0 | 0x54 | `0x005EB930` | `0x007A5CA8` | `0x004031C0` (souche) |
| 1 | 0x820 | `0x004266D0` | `0x00785230` | `0x0042CAA0` |
| 2 | 0x7D4 | `0x004299C0` | `0x00785518` | `0x0042DF10` |
| 3 | 0x7B4 | `0x004260B0` | `0x00785370` | `0x0042C0C0` |
| 4 | 0x7A8 | `0x00426580` | `0x00785428` | `0x00429D90` |
| 5 | 0x68 | `0x00426650` | `0x00784680` | `0x0040A100` |

`0x005F3080` force le type 1 et `0x005F3010` le type 2. `0x00602650` les appelle
selon `P+0x22C` (`0x00602A79–0x00602A98`). **Interprétation :** 1 = conduite
humaine, 2 = pilote automatique ; ExtraOptions (`Player_AutoPilotOn/Off`,
`0x005FAE20`/`0x005FAF10`) appelle bien `0x005EBA30`. Le type 0 n'a aucune
logique de pilotage : piste intéressante pour un véhicule distant (§ 4.3).

**Prochaine validation (runtime autorisé) :** lire V+0x28, V+0x5C0, V+0x60…,
V+0x70…, V+0x90… et `[0x00865154]` sur quelques frames ; comparer la norme
V+0x430 à Δposition/dt ; vérifier l'axe avant et le signe de V+0x42C.

## 2. États GameFlow (`0x008654A4`)

### 2.1 Écritures (observation, 10 sites)

| Valeur | Site d'écriture | Octets | Routine | Déclencheur observé | Interprétation |
| --- | --- | --- | --- | --- | --- |
| 0 | aucune écriture trouvée | — | — | valeur initiale (zone `.data` non initialisée) | aucun état |
| 1 | `0x0057FF1D` | `C7 05 A4 54 86 00 01 00 00 00` | `0x0057FF10` | file différée : `0x00580E5C` (démarrage `0x00580E00`), `0x00581803` | chargement du front-end |
| 2 | `0x00581A2E` | `89 1D A4 54 86 00` (EBX = 2) | `0x00581A20` | étape posée par `0x00578320` à `0x00578336` | déchargement du front-end |
| 3 | `0x005800BA` | `C7 05 A4 54 86 00 03 00 00 00` | `0x00580090` | file différée posée à `0x0057836B` | front-end (mods : « 3 = FE ») |
| 4 | `0x0057F22F` | `MOV [0x008654A4], 4` | `0x0057F210` | `JMP` depuis `0x00580EE5` et `0x00581984` | chargement de région |
| 5 | `0x0057F5CE`, `0x0058198F`, `0x00581586` | `MOV [0x008654A4], 5` | `0x0057F2D0`, `0x00581820`, `0x00581580` | `0x0057A649 → 0x00581580 → JMP 0x0057F2D0` ; `0x00581820` | chargement de piste (`TrackLoader::LoadHandler`) |
| 6 | `0x005782F6` | `C7 05 A4 54 86 00 06 00 00 00` | `0x005782F0` | adresse poussée par `0x0057F2D0` à `0x0057FADE` | conduite |
| 7 | `0x005815E7` | `MOV [0x008654A4], 7` | `0x005815C0` | appelé à `0x005817E3`, `0x005818EE` ; appelle le démontage `0x0060A9A0` à `0x00581621` | déchargement de piste |
| 8 | `0x00580EF8` | `MOV [0x008654A4], 8` | `0x00580EF0` | appelé à `0x005817ED`, `0x00581977` | déchargement de région |

101 références au total ; les lectures comparent surtout à 3 et 6. Dans la boucle,
`0x005812AE` appelle `0x0057EE00` quand l'état ≠ 6 et `0x00581349` n'entre dans la
simulation que pour l'état 6.

### 2.2 Transitions (observation + interprétation)

- **File différée :** `0x00865484` (fonction), `0x00865488` (argument),
  `0x0086548C` (libellé). Elle est vidée en tête de frame (`0x00581210–0x0058123A`,
  `0x00581272–0x0058129C`) sur le fil de la boucle principale, **avant** la partie
  simulation : un changement d'état ne survient pas au milieu d'un tick.
- Démarrage : `0x00580E00` met `0x0057FF10` (état 1) en file ; l'état 3 suit via
  `0x00580090`.
- Front-end → course : 2 (`0x00581A20`), puis 5 et `0x0057F2D0`, qui crée la course
  (`0x0057F952 CALL 0x0060A680`) et termine par le rappel `0x005782F0` (6).
- Course → front-end (`0x005817D0`) : 7, puis 8, puis `0x0057FF10` en file (1).
- Course → course (`0x00581820`) : 7, puis comparaison de noms de région
  (`0x0043DC30` sur `0x005D3E40(...)+0x40`) : soit 8 et 4 (`0x0057F210`), soit 5
  directement (`0x0057F2D0`). Interprétation : changement ou non de région.

### 2.3 Garage, free roam, pause : inconnues tracées

Aucune valeur au-delà de 1 à 8 n'est écrite : **garage et free roam n'ont pas d'état
GameFlow propre** d'après le statique. Pour les distinguer, il faudra d'autres
globales, candidates : statut de course `[[0x008900D8]]` (valeurs 1, 4, 5 et 6
testées), identifiant d'épreuve `[0x0089E7A0]` (plages `0x1068–0x10CC`,
`0x1130–0x1194`, valeur `0x7D0`), mode `[0x00850078]` (1, 2, 4, 8, `0x200`).

Dans l'état 6, `0x00609CE0` **n'est pas appelé** si : `0x005EA320(W)` ≠ 0
(minuterie `W+0xFC` > 0, `[0x00890084]` ≠ 0 ou `W+0x100` ≠ 0, sauf
`[0x00890088]` ≠ 0) ; ou `[[0x008900D8]] == 1` ; ou `[[0x008650B8]+0x1C]` ≠ 0 ; ou
hors ligne avec `[[0x008900D8]] == 6`. Lequel correspond au menu pause : **inconnu**.

**Prochaine validation :** journaliser `[0x008654A4]`, `[[0x008900D8]]`,
`[0x0089E7A0]`, `[0x00850078]`, `[0x00890084]` en menus, chargements, garage,
free roam, course et pause.

## 3. Point de hook « après mise à jour des joueurs »

### 3.1 Site retenu : 0x00581407 (RVA et offset fichier `0x181407`)

Octets relus dans Ghidra **et** dans le fichier :

```text
005813FC  8B 44 24 14        MOV EAX,[ESP+0x14]      ; dt
00581400  8B 0D 80 00 89 00  MOV ECX,[0x00890080]    ; W
00581406  50                 PUSH EAX
00581407  E8 D4 88 08 00     CALL 0x00609CE0         ; rel32 = 0x000888D4
0058140C  8B 0D B8 50 86 00  MOV ECX,[0x008650B8]
00581412  88 5C 24 0C        MOV byte [ESP+0xC],BL
00581416  E8 F5 79 FF FF     CALL 0x00578E10
```

**Proposition (non implémentée) :** remplacer uniquement la cible rel32 de ce `CALL`
par un trampoline qui appelle `0x00609CE0` avec le même ECX et le même argument,
puis exécute le post-hook. Une instruction de 5 octets, sans relocalisation.
Avant écriture : vérifier `E8 D4 88 08 00` et refuser toute autre valeur.

**ABI observée :** `thiscall` ; ECX = W, un argument 32 bits (flottant `dt`) sur la
pile, retiré par l'appelé (`RET 4` à `0x0060A297`). Valeur de retour ignorée (ECX est
rechargé à `0x0058140C`). L'appelant compte ensuite sur **EBX = 0**
(`XOR` à `0x005811D9`, réutilisé à `0x00581412` et `0x00581444`), **ESI = 6**
(`0x005812B3`) et EDI ; ses variables locales sont adressées par ESP
(`[ESP+0xC]`, `[ESP+0x10]`, `[ESP+0x14]`). Le trampoline doit préserver
EBX/ESI/EDI/EBP, rétablir ESP, rendre une pile x87 vide (elle l'est à l'appel :
`FSTP [0x00865154]` à `0x005813E1`) et ne modifier ni le mot de contrôle FPU ni MXCSR.

**Ordre garanti :** le post-hook s'exécute après la physique (`0x0060A032`), les
contrôleurs et pilotes, TrafficTeleporter, la boucle joueurs
(`0x0060A240–0x0060A254`) et les appels finaux `0x0043AF20`, `0x00638070`,
`0x005E86E0`, `0x006076D0`.

**Branches qui l'évitent** (toutes dans `0x005811C0`) : GameFlow ≠ 6
(`0x00581459`) ; pause supposée (`0x0058141D` : `dt` calculé, `0x005F1390(W)`) ;
`[[0x008650B8]+0x1C]` ≠ 0 (`0x00581398`) ; course terminée hors ligne
(`0x00581333`). Le hook ne s'exécute donc **que pendant un tick réel**.

**Fil :** même fil que la boucle `0x005811C0` (appelée par `0x00580E70`). Aucun autre
fil de simulation observé ; non prouvé.

### 3.2 Alternatives écartées

- `0x0060A256` (`8B 45 08 50 B9 88 7B 82 00`) : juste après la boucle joueurs,
  mais 9 octets à relocaliser (3 instructions) au milieu de `0x00609CE0`.
- `0x00581475` (`E8 46 0E E8 FF`, appel de la souche `0x004022C0` = `C3`) :
  exécuté à **chaque** frame, tous états confondus ; utile pour observer GameFlow, pas
  pour un post-tick. Ne pas toucher `0x00581470`, voisin et utilisé par ExtraOptions.

### 3.3 Vérification des mods

- **Sources figées :** NFSU2ExOpts `801539e2` (7 janvier 2022) et NFSU2Unlimiter
  `45b67383` (18 octobre 2026). 1354 constantes hexadécimales distinctes extraites.
  Dans `0x005811C0–0x00581575`, seule `0x00581470` est patchée : `MakeCALL`
  d'ExtraOptions vers `Thing()` (`pushad`/`popad`, sans rappel de `0x004022C0`, qui
  n'est qu'un `RET`). Rien dans `0x00609CE0–0x0060A297`, `0x00602650–0x00602BCF`,
  `0x005F0F00` ni `0x006012B0`. Unlimiter écrit `0x005F4604` dans `Car::dtor`
  (§ 4.1), hors zone de hook.
- **Binaires installés** (`scripts/`) : ExtraOptions.asi (274 432 octets, 4 janvier
  2024), Unlimiter.asi (275 456 octets, 21 août 2022), WidescreenFix.asi
  (366 080 octets, 4 janvier 2024). **Leurs dates ne correspondent pas aux sources
  figées** : la vérification par les sources ne suffit donc pas.
- **Scan statique des binaires, sans exécution :** constantes 32 bits pointant dans les
  zones ci-dessus. ExtraOptions.asi contient `0x00581470` à l'offset `0x777A`,
  cohérent avec la source. Les autres occurrences, dans Unlimiter.asi et dinput8.dll
  (Ultimate ASI Loader), sont des faux positifs : lectures non alignées dans
  `MOV EAX,[0x10040060]`, `PUSH 0x100060A0` et des `CALL rel32`. Rien dans
  WidescreenFix.asi, server.dll ni dimap.dll.
- **WidescreenFix utilise des motifs d'octets :** 62 motifs ASCII extraits, 69
  correspondances dans SPEED2.EXE, **aucune dans les zones de hook**. Les plus
  proches : `0x0057ED8F`, `0x0057EB0A`, `0x0057EB8B` (autour de `0x0057EAD0`, temps
  de frame) et `0x005FF67C` (constructeur du monde). Limite : des motifs stockés
  autrement ou des décalages calculés depuis une correspondance ne sont pas couverts.

**Conclusion :** aucune preuve statique de patch sur `0x00581400–0x0058141B` ;
confiance moyenne-forte, faute de désassemblage complet des ASI. À l'installation, le
client devra comparer les 5 octets et journaliser les modules chargés. server.dll et
dimap.dll, présents dans l'installation, ne sont pas analysés.

## 4. Préparation de la phase 4

### 4.1 Allocation et destruction des véhicules monde

- **Création, au chargement de piste uniquement (état 5)** : `0x0057F952 CALL
  0x0060A680`. Cette routine crée `[0x008650B8]` (0x50F0 octets),
  `[0x00890118]` (0x2780), puis W (0x1600, constructeur `0x005FF490`, qui écrit
  `[0x00890080]` à `0x005FF537` et range TrafficTeleporter, créé par `0x004179F0`,
  en `W+0x118` à `0x005FF5FE`).
  Elle appelle ensuite `0x006065B0` (voitures, `0x0060A774`) et `0x005F4250`
  (joueurs, `0x0060A798`), et range son `this` dans `[0x008900D8]`
  (`0x0060A88D`).
- `0x006065B0` lit 6 places de grille (`0x005EB5D0(6, …)`, 0x20 octets chacune) et
  la table des concurrents `[0x0089E810]`, compte `[0x0089E814]`, **pas de 0x940
  octets**. Pour chaque place 1 à 6, il cherche un enregistrement avec octet +0 ≤ 0
  et octet +3 = place. Type `+4 = 1` (humain) : contrôleur 1, ou 3 si
  `[0x0089E7B8]` > 0. Type 2 (IA) : contrôleur 3. Octet `+0x900` armé :
  contrôleur 0. Départ personnalisé optionnel (`+0x91C`, position `+0x920`, cap
  `+0x930`). Les enregistrements de type 3 (trafic) passent par
  `0x0042F7A0` (TrafficTeleporter).
- `0x00606480` : `malloc(0x9E0)` (`0x006064CD 68 E0 09 00 00`), constructeur de V
  `0x00604260` (`RET 0x10`), `0x005EBA30` (contrôleur), `0x005F46A0` (modèle), puis
  insertion en fin de la liste chaînée de W (`0x00606578–0x0060658C`).
  Trafic : `0x0042C670`, également `malloc(0x9E0)` (`0x0042C787`), `0x00604260`,
  `0x005F46A0`, `0x005EBA30`. **V fait donc 0x9E0 octets, distinct de P (0x9C0).**
- `0x005F0D20` (`0x006066B7`, `0x0060671B`) reconstruit `W+0x1C` : humains d'abord
  (compte `W+0x14`), puis IA (`W+0x18` cumulé), puis trafic (`W+0x10` total). Il
  écrit `V+8 = index` et les tables `0x0089CCF8` (V), `0x0089CD50`, `0x0089CDA8`,
  `0x0089CE00`, `0x0089CE58`, `0x0089CEB0`. Ces tables sont espacées de 0x58 octets :
  22 véhicules indexés au plus (inférence).
- **Destruction** : les seuls appelants statiques de `Car::~Car` `0x005F44D0` sont
  `0x005FF3B0` (purge de liste, aussi utilisée par les gestionnaires d'exception
  `0x00779D43` et `0x00779DE3`) et le destructeur du monde `0x005FF6F0`. Chacun
  détache le nœud, appelle `0x005F44D0` et libère par `0x00575630`. `0x005FF6F0`
  est appelé par `0x0060A9A0` (`0x0060AA1C`), lui-même appelé par l'état 7
  (`0x00581621`) ; il remet `[0x00890080]` à 0 (`0x005FF7CC`).

**Interprétation, confiance moyenne-forte :** aucune allocation ni destruction de
voiture en cours de session parmi les appels directs. Le trafic est recyclé
(`TrafficTeleporter::DoTimeStep` `0x0042F910`, appelé à `0x00609ED7`) plutôt que
libéré. Les appels indirects ne sont pas exclus. Toute voiture ajoutée par le client
devra exister avant `0x005F0D20` et disparaître avant `0x005FF6F0`.

### 4.2 Faisabilité d'un véhicule dans le slot joueur 1

- `0x005F4250` crée P1 si `[0x0089E80C]` ≥ 0 et si `0x005ED920` trouve
  l'enregistrement dont l'octet +1 vaut cet identifiant, avec type +4 = 1 et octet
  +0 ≤ 0 (`0x005F42CD–0x005F42ED`). Le constructeur `0x005F1670` reçoit alors
  `[0x0089E7EC]` et l'index 1. P0 utilise `[0x0089E808]` et `[0x0089E7E8]` ; il est
  forcé si `[0x0089E7AC]` ≠ 0.
- Liaison : si `[0x0089E7AC]` = 0, recherche dans la liste de W d'un V dont le mot
  V+0xA vaut l'identifiant (`0x005F4351–0x005F4376`) ; sinon V = `W+0x1C[n_humains +
  P.index]`, donc dans le groupe IA (mode démonstration ? hypothèse). Ensuite
  `0x005F3E10` lie P et V dans les deux sens.
- Le moteur prévoit deux joueurs : `0x00602650` teste `[0x008900BC] == 2`
  (`0x0060291E–0x00602927`) et saute alors la réinitialisation automatique ; la boucle
  `0x0060A240` met à jour tous les joueurs actifs.
- `[0x0089E808/0C]` sont écrits par `0x00526970` (`0x00526A81/87`),
  `0x005322E0` (`0x00532489/8E`) et `0x00540780` (`0x00540890/96`) ; `0x005F02B0`
  écrit seulement `[0x0089E808]` (`0x005F02D6`). Leurs contextes de menu ne sont
  pas analysés.

**Conclusion, confiance moyenne :** un véhicule humain en slot 1 semble possible si
l'on prépare, avant `0x0060A680` (état 5), un second enregistrement humain placé
sur la grille et `[0x0089E80C]`. **Inconnues :** sens de `[0x0089E7E8/EC]` (port
d'entrée probable), caméra, HUD et viewport de P1 sur PC, origine du mode deux
joueurs, effets réseau (`0x0089CF48`).

### 4.3 Piste alternative pour un véhicule distant

Une voiture IA ou de trafic passée au **contrôleur 0** (`0x005EB930`, mise à jour
`0x004031C0` = souche) n'aurait plus de logique de pilotage, tout en restant mise à
jour par son modèle physique (§ 1.2). Écrire son état juste après le tick (hook § 3.1)
éviterait de concurrencer l'intégrateur. **Hypothèse à valider**, ainsi que le choix
du modèle (0, 4 ou 1) et l'effet sur collisions et IA.

## Limites et prochaines validations

- Toutes les conclusions sont statiques. Les unités de distance, les axes, le modèle
  actif, la pause et le fil d'exécution restent à mesurer avec autorisation explicite.
- `0x005F46A0`, `0x005A9F10`, `0x005AAD90`, `0x005AB3C0` et plusieurs routines
  GameFlow ne sont pas des fonctions Ghidra : ils ont été lus par listing (`ins:`).
- Les appels virtuels résolus ici le sont par vtable et par constructeur ; d'autres
  classes peuvent exister.
- Aucun offset n'a été intégré au client, aucun hook écrit, aucun accès moteur activé.
