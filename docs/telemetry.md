# Télémétrie lecture seule (phase 3b)

Objectif : valider en jeu les offsets statiques de
[static-pass-2.md](../research/findings/static-pass-2.md) avant de les exposer via
`GameAccess`. Rien n'est écrit dans la mémoire du moteur hormis le rel32 du hook.

## Fonctionnement

1. Empreinte reconnue (`identity.cpp`), sinon rien ne démarre.
2. Le module est épinglé (`GET_MODULE_HANDLE_EX_FLAG_PIN`) : le code du moteur
   pointera vers lui jusqu'à la fin du processus.
3. Les modules chargés sont journalisés, puis un thread d'écriture démarre.
4. Hook : seule la cible rel32 du `CALL 0x00609CE0` à `0x00581407` est remplacée,
   après contrôle de la base d'image (`0x00400000`) et des octets `E8 D4 88 08 00`,
   par un échange atomique aligné. Tout écart refuse le hook et le journalise.
5. Le trampoline (`__fastcall` + EDX inutilisé, ABI identique à thiscall `RET 4`)
   appelle le tick d'origine puis copie un échantillon. Il ne s'exécute que pendant
   un tick réel (GameFlow 6, hors pause supposée).
6. Capture sur le fil moteur : copies 32 bits brutes sous SEH (une violation d'accès
   coûte un échantillon, pas le jeu), **aucune instruction x87/SSE** (vérifié au
   désassemblage), file SPSC bornée de 4096 échantillons sans blocage.
7. Le thread d'écriture vide la file toutes les 100 ms et journalise les transitions
   GameFlow, les changements de véhicule, les pertes et fautes. À la première entrée
   en conduite, il rejournalise les modules et vérifie que le hook est intact.

Fichiers dans `%TEMP%` :

- `UGWorld-diagnostic.log` : cycle de vie, modules, transitions, anomalies.
- `UGWorld-telemetry-AAAAMMJJ-HHMMSS.csv` : une ligne par tick (~60 lignes/s).

## Colonnes CSV

| Colonne | Source | Attendu à valider |
| --- | --- | --- |
| `frame`, `dt` | compteur du hook, argument du tick | `dt` ≈ 1/FPS en secondes |
| `game_flow` | `[0x008654A4]` | 6 pendant tout le CSV |
| `player_count`, `race_status`, `world_flag`, `mode_flags`, `event_id` | globales candidates § 2.3 | distinguer free roam, course, garage |
| `status` | 0 ok, 1 pas de P, 2 pas de V, 3 faute | 0 en conduite |
| `player`, `vehicle`, `vehicle_index` | `P=[0x008900AC]`, `V=[P+4]`, `V+8` | stables pendant une session |
| `controller_type`, `model_type`, `full_model` | `V+0x24`, `V+0x28`, `V+0x5C0` | 1 / modèle actif (1 ou 4 ?) |
| `heading` | mot `V+0x40C` | tour complet = 65536 ? |
| `pos_*` | `V+0x60/64/68` | quel axe est vertical ? unité (m ?) |
| `vel_*`, `speed` | `V+0x70/74/78`, `V+0x430` | `speed` = ‖vel‖ = ‖Δpos‖/dt |
| `forward_speed` | `V+0x42C` | signe en marche arrière |
| `b0_*`, `b1_*`, `b2_*` | lignes de `V+0x90` (pas 16 octets) | b0 = axe avant ? base orthonormée ? |
| `ang_*` | `V+0x110` | vitesse angulaire en virage ? |

## Protocole de test (à lancer par l'utilisateur)

1. `UGWorld.asi` (version télémétrie) dans `scripts/`, jeu lancé normalement.
2. Menu principal ~10 s, puis free roam.
3. En free roam : 10 s à l'arrêt ; ligne droite en accélérant jusqu'à ~100 km/h
   (noter la vitesse affichée au compteur) ; freinage ; marche arrière quelques
   secondes ; un tour complet sur place ou un rond-point ; une pente si possible.
4. Menu pause ~5 s, reprise.
5. Entrer au garage / magasin puis ressortir.
6. Lancer une course, la quitter, revenir au menu principal, quitter le jeu.
7. Fournir le log et le CSV. Noter tout gel, saccade ou plantage.

Critère de sortie de phase 3b : sur plusieurs sessions, aucune faute ni plantage,
unités et axes établis, et comportement aux transitions documenté.

## Session 1 — 19 octobre 2026, 18:05–18:11 (profil moddé)

Fichiers : `UGWorld-telemetry-20261019-180506.csv` (7 978 lignes, 2,2 Mo) et
journal de diagnostic. Analyse par script Node hors dépôt. Parcours effectué :
menu → free roam → conduite → entrée au garage (retour front-end) → sortie du jeu.
**Pas de course lancée** dans cette session.

**Stabilité.** Aucun plantage ni événement d'erreur Windows. Hook installé, intact
en conduite. 7 978 ticks consécutifs (aucun trou de numérotation), **tous au statut
0** : aucune faute SEH, aucune perte. P (`0x173B6170`) et V (`0x15A9E528`) stables
toute la session, `vehicle_index` 0, `player_count` 1.

**Pas temporel.** `dt` médian 0,013333 s (1/75, écran 75 Hz), p99 1/60, max 0,040 ;
108,2 s simulées. Aucun pic de `dt` à la reprise après la pause : la pause ne
fait qu'interrompre les ticks (le hook ne s'exécute pas).

| Hypothèse statique | Résultat mesuré | Verdict |
| --- | --- | --- |
| `V+0x60/64/68` = position monde | ‖Δpos‖/dt ÷ ‖vel‖ médiane **0,9996**, cos(Δpos, vel) médian 1,0000 | **validé** |
| Axe vertical | `pos_z` 14,8–21,7 ; ‖vel_z‖ médian 0,16 contre 11–17 en x/y ; b2 moyen (0,004 ; 0,002 ; 0,999) | **Z vers le haut** |
| `V+0x70/74/78` = vitesse linéaire monde, unités/s | ratio ci-dessus = 1 | **validé** |
| `V+0x430` = ‖vitesse‖ | ‖vel‖ ÷ speed = 1 exactement | **validé** |
| `V+0x90` = base d'orientation | 3 lignes unitaires, produits scalaires < 2·10⁻⁷, déterminant +1 | **base orthonormée directe** |
| b0 = axe avant | cos(vel, b0) médian 0,9998 (négatif en marche arrière) ; b2 = haut ; b1 = gauche (b2 × b0) | **validé** |
| `V+0x42C` = vitesse longitudinale signée | `forward_speed − vel·b0` < 1,4·10⁻⁵ ; min −4,8 en marche arrière (127 ticks) | **validé** |
| `V+0x40C` = cap 16 bits | `heading = atan2(b0_y, b0_x) × 65536 / 2π`, erreur médiane 0,07°, max 0,29° | **validé** : 0 = +X, sens trigonométrique, tour = 65 536 |
| `V+0x110` = vitesse angulaire | ‖ang‖ médiane 0,37, max 2,46 (rad/s plausibles) | probable, axe non vérifié |
| Modèle actif du joueur | `controller_type` 1 / `model_type` 1 / `full_model` 1 en conduite | **modèle 1** (corps rigide, sous-pas) |

**Unité de distance : le mètre (validé).** Vitesse maximale mesurée 43,625
unités/s, soit 157,05 km/h si l'unité est le mètre. Le maximum affiché au compteur
était de **158 km/h** (relevé de l'utilisateur), écart inférieur à 1 %.

**Transitions observées.**
- Démarrage : GameFlow 0 → 1 → 3 (front-end) → 4 → 5 → 6 (free roam).
  `mode_flags` passe de 0x2 (front-end) à 0x1, `event_id` de 0x7D0 à 0xFA0.
- En free roam : `race_status` [[0x008900D8]] = 4, `world_flag` 0.
- **Entrée au garage** : pendant les 3,7 dernières secondes (279 ticks),
  `controller_type` 0 et `model_type` 5, vitesse nulle. La voiture est reprise
  par le jeu, sans pilote (cf. contrôleur 0, static-pass-2 § 4.3). Ensuite
  GameFlow 6 → 1 → 3 : **le garage est un retour au front-end**, la piste est
  déchargée. Les états intermédiaires 7/8 n'apparaissent pas : la scrutation à
  100 ms est trop lente pour les voir.
- Au départ, 412 ticks avec une position strictement constante (voiture immobile
  au point d'apparition, corps rigide endormi).

## Session 2 — 19 octobre 2026, 18:18–18:23 (profil moddé)

Fichier `UGWorld-telemetry-20261019-181816.csv` (13 689 lignes, 3,8 Mo).
Parcours : free roam → garage → retour en free roam → course (événement
`0x11FA`) → retour en free roam → menu.

**Stabilité.** Aucun plantage ni erreur Windows. 13 689 ticks consécutifs, **tous
au statut 0**, sur **quatre chargements successifs**. Toutes les validations de la
session 1 se reproduisent :
- ‖Δpos‖/dt ÷ ‖vel‖ médiane 0,9995 ;
- base orthonormée directe ;
- `forward_speed = vel·b0` ;
- cap = `atan2(b0_y, b0_x)`, erreur maximale 0,29°.

**Durée de vie de P et V (validé).** Chaque chargement (GameFlow 4 → 5 → 6) crée
un **nouveau P et un nouveau V** à de nouvelles adresses, sans aucune faute à la
transition :

| Segment | P | V | Durée | Contexte |
| --- | --- | --- | --- | --- |
| Free roam | `0x171B0430` | `0x170B5F38` | 85,7 s | `event` 0xFA0, `race` 4 |
| Après garage | `0x173A0738` | `0x170AB078` | 24,9 s | sortie scriptée 4,0 s (contrôleur 0 / modèle 5), puis 1/1 |
| Course | `0x173A1D08` | `0x17141070` | 71,3 s | `event` 0x11FA, `race` 3 → 4 → 7 |
| Free roam | `0x173C96C8` | `0x17140668` | 2,7 s | `event` 0xFA0 |

Conséquence pour le client : ne jamais conserver P ou V d'un tick à l'autre au-delà
d'un chargement ; les relire à chaque tick, comme le fait déjà la capture.

**États de course (`race_status` = [[0x008900D8]]).**

| Valeur | Moment observé | Interprétation |
| --- | --- | --- |
| 3 | 51 ticks au début de la course (contrôleur 0 → 1) | pré-départ |
| 4 | free roam **et** course en cours | course active (le free roam est une « course » permanente) |
| 7 | après l'arrivée (contrôleur 0 / modèle 5, vitesse 0) | course terminée, écran de résultats |

- Juste avant l'état 7, le contrôleur passe à **3** pendant 179 ticks, à
  26,7 m/s : le jeu pilote la voiture après la ligne d'arrivée (contrôleur IA).
- `event_id` distingue le free roam (0xFA0) de l'épreuve (0x11FA). `mode_flags`
  reste à 0x1 et `world_flag` à 0 en conduite.
- Une course hors région courante passe par GameFlow 6 → 4 → 5 → 6 (rechargement
  complet), comme en statique (static-pass-2 § 2.2).
- La boucle physique tourne aussi pendant le compte à rebours : 315 ticks
  immobiles au début de la course.

**Anomalies mineures.** Quelques écarts isolés entre Δpos et vel·dt : sauts de
0,4 m au plus sur une seule frame, en conduite (contrôleur 1), lors de contacts à
vitesse modérée. Interprétation : correction de position par la résolution des
collisions, pas une téléportation. `dt` minimal 0,0053 s, maximal 0,040 s.

**Hors de portée de cette télémétrie.** Elle ne lit que le joueur 0 :
`player_count` vaut 1 en course aussi, et les voitures IA ne sont pas mesurées.
Les lire passera par la liste `W+0x1C` (static-pass-2 § 4.1), à faire en phase 4.

## Bilan de la phase 3b

Critère de sortie **atteint** pour le jeu moddé : deux sessions, 21 667 ticks,
aucune faute ni plantage. Unité (mètre), axes (Z haut, base avant/gauche/haut),
cap, vitesses, modèle physique et durée de vie de P/V sont établis, et les
transitions sont documentées. Restent ouverts, sans bloquer la suite :
- le profil vanilla ;
- l'identification de la pause ;
- la scrutation GameFlow à 100 ms, qui manque les états brefs 2, 7 et 8.
