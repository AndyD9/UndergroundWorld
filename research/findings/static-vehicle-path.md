# Joueur, position et cycle de vie — examen statique du 19 octobre 2026

## Portée et preuves

Toutes les adresses ci-dessous sont des VA préférées pour **le seul SHA-256**
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
Base `0x00400000`, RVA = VA − base. Ghidra 12.1.4, langage
`x86:LE:32:default:windows`, JDK Temurin 25.0.4.1. Projet existant ouvert avec
`-readOnly -noanalysis` : pas de nouvelle analyse automatique, pas de modification
du programme, pas de lancement du jeu. Le script vérifie le SHA de la base.

Les fichiers locaux `research/ghidra/exports/*.json` contiennent les références,
instructions et pseudocode récupérés. Ils sont exclus de Git car ils contiennent
du code décompilé du jeu. Leurs empreintes et couvertures sont enregistrées dans
`static-export-index.json`. Le script source `ExportCandidates.java` et le lanceur
`tools/inspect-ghidra.ps1` permettent de reproduire l'extraction.

Les noms PLAYER, HotPosition et les candidats communautaires guident
l'interprétation ; les noms `FUN_*` ne sont pas des symboles originaux. Le
pseudocode ne prouve pas une signature C++ : des paramètres implicites et des
fonctions à sauts partagés peuvent être mal représentés. Les références
indirectes non résolues et fonctions non reconnues limitent la couverture.

## 1. Table et lien joueur/véhicule

**Observation, confiance élevée :** `0x008900AC` (RVA `0x004900AC`) reçoit
98 références dans la base analysée. Plusieurs instructions utilisent
`[index*4 + 0x8900AC]` : il s'agit d'un stockage indexé de valeurs 32 bits,
pas d'un simple pointeur à déréférencer deux fois pour obtenir une table.
`0x005F1670` (RVA `0x001F1670`) inscrit l'objet à cette adresse à
`0x005F1725` ; la routine reçoit l'index qui est également enregistré à `+0x10`.
Elle référence la chaîne PLAYER. `0x005F4250` (RVA `0x001F4250`) alloue
`0x9C0` octets puis appelle cette initialisation avec les indices 0 ou 1.
Cela ne prouve pas qu'on peut étendre la table au-delà de deux entrées.

**Observation :** `0x005F3E10` (RVA `0x001F3E10`) écrit le second objet dans
`premier + 4` à `0x005F3E17`, et le premier dans `second + 0x18` à
`0x005F3E1A`. L'appel vient de la sélection effectuée par `0x005F4250`.

**Interprétation, confiance forte mais statique :** table des joueurs et liaison
bidirectionnelle joueur/véhicule. La chaîne candidate de lecture pour l'indice 0
est `P = valeur32(base+0x4900AC)`, puis `V = valeur32(P+4)`. Ce n'est **pas** une
API activée ni une preuve que P et V sont valides dans tous les états.

**À valider :** identité locale, limites de table, moment où P/V deviennent
valides, différences menus/course/free roam, thread propriétaire, sessions répétées.
Preuves locales : `seeds.json`, `player-path.json`, `lifecycle.json`.

## 2. Position et cap candidats, avec trajet aller-retour

**Observation, confiance élevée :** `0x005EB450` (RVA `0x001EB450`) lit
l'indicateur `0x008900CC` (RVA `0x004900CC`), construit un chemin HotPosition,
et lit trois flottants depuis V aux offsets **0x60, 0x64, 0x68**.
Instructions à `0x005EB47E–0x005EB4A1` : chargement de V depuis `P+4`,
lecture 16 bits `V+0x40C`, puis lectures x87 des trois composantes.
La routine copie aussi ces valeurs vers `0x00890140/144/148`
(RVA `0x00490140/144/148`) et le mot vers `0x008900D0` (RVA `0x004900D0`).
Elle efface l'indicateur de sauvegarde.

`0x00600E10` (RVA `0x00200E10`) consomme l'indicateur `0x008900CD`
(RVA `0x004900CD`) et lit les données enregistrées ou leur copie en mémoire.
À `0x00600EEB`, ECX est chargé depuis `P+4` ; à `0x00600F00`, l'appel
`0x005FBD60` reçoit l'adresse des trois composantes et le mot mémorisé.
Cet appel conduit à `0x005F52C0` (RVA `0x001F52C0`), qui écrit le mot
à `V+0x40C` (`0x005F52FC`) et les trois composantes à `V+0x60/64/68`
(`0x005F5316/19/22`). Plusieurs sous-systèmes sont ensuite réinitialisés.

**Interprétation :** position monde fortement étayée ; cap sur 16 bits probable.
Une téléportation/réinitialisation est probable, mais **pas un setter de transform
à appeler chaque frame**. Ces routines touchent aussi contrôleurs et physique.

**Inconnues :** unité de distance, axes et signe, échelle du cap, inclinaison et
roulis, représentation de l'orientation complète, différence entre position
physique et centre de rendu. À confirmer par données réelles après autorisation.
Preuves : `player-path.json`, `transform-path.json`, `lifecycle.json`.

## 3. Vecteur et norme candidats pour la vitesse

**Observation :** dans `0x005F52C0`, EDI est construit comme `V+0x40`
(`0x005F52DC`). Il devient ECX avant l'appel de `0x00592160`
(RVA `0x00192160`) à `0x005F535F`. Cette routine copie trois flottants aux
offsets `+0x30/34/38` de **ce sous-objet** et calcule leur norme euclidienne
à `+0x3F0` (`FSQRT` à `0x00592196`, stockage à `0x0059219E`).
Les offsets résultants par rapport à V sont donc **0x70/74/78 et 0x430**.

Le vecteur d'entrée vient de `0x0043CF90` (RVA `0x0003CF90`) qui redimensionne
un vecteur fourni depuis `V+0x90` en utilisant un scalaire. La routine de
réinitialisation stocke ce scalaire à `V+0x42C`. Les octets de cette chaîne sont
observés ; la nature de vitesse est une **inférence de confiance moyenne**.
Il faut suivre les écritures pendant l'intégration physique et comparer la norme
aux déplacements mesurés. Aucune unité m/s ou km/h n'est attribuée.

**Attention :** `+0x30` du sous-objet ne signifie pas `V+0x30`. Le seul
pseudocode peut masquer l'ajustement d'ECX. Preuves : `lifecycle.json`,
`orientation-tick.json`, `vector-math.json`.

## 4. Orientation : base candidate, pas encore un quaternion

`0x005F5010` (RVA `0x001F5010`) transmet `V+0x90`, `V+0x60`, le mot à
`V+0x40C` et `V+0x5C4` à `0x005F49F0` (RVA `0x001F49F0`). Ce dernier
écrit trois groupes de trois flottants, espacés de 16 octets, à partir de
`V+0x90`, `V+0xA0`, `V+0xB0` ; les quatrièmes composantes sont mises à zéro.
Il combine une rotation et des résultats de requêtes géométriques, et peut
modifier la troisième composante de la position fournie.

**Interprétation, confiance moyenne :** base d'orientation construite lors du
repositionnement, possiblement adaptée au sol. Pas de preuve de son actualisation
à chaque pas ni de l'ordre des axes. Ne pas la convertir arbitrairement en
quaternion. La routine termine par `RET 0x10`, alors que le pseudocode récupéré
n'expose que trois arguments de pile : signature de compilation non validée.
Preuves : `orientation-tick.json`, `vector-math.json`.

## 5. Nettoyage : risque de pointeur périmé confirmé

`0x006063B0` (RVA `0x002063B0`) efface le lien `V+0x18` à `0x00606409`
puis `P+4` à `0x00606414`. `0x005F1CC0` (RVA `0x001F1CC0`) décrémente
le compte `0x008900BC` (RVA `0x004900BC`) et efface la case de la table indexée
à `0x005F1DAE`. `0x0060A9A0` (RVA `0x0020A9A0`) appelle ces chemins et
libère les joueurs après nettoyage des gestionnaires.

**Observation forte :** les liens ne sont pas permanents. Un lecteur ne doit
jamais conserver P ou V entre sessions ni supposer que tester une valeur non
nulle suffit à prévenir une destruction concurrente. Le contexte de lecture
sûr doit être établi sur le thread propriétaire. La routine de destruction
exacte du véhicule monde et tous ses chemins restent à étudier.
Preuves : `player-path.json`, `transform-path.json`, `lifecycle.json`.

## 6. Boucle et mise à jour candidates

`0x005811C0` (RVA `0x001811C0`) contient une boucle répétée avec gestion
d'états, calcul temporel et traitements par frame. Dans une branche conditionnée
notamment par l'état `0x008654A4 == 6`, elle charge ECX depuis `0x00890080`
(RVA `0x00490080`) à `0x00581400`, pousse une valeur temporelle puis appelle
`0x00609CE0` (RVA `0x00209CE0`) à `0x00581407`.

Cette dernière parcourt des pointeurs à partir de `gestionnaire+0x1C` et des
comptes aux offsets `+0x10/+0x14/+0x18`, appelle des méthodes virtuelles et
des traitements par objet. Elle termine notamment par une boucle sur les
joueurs actifs : chargement ECX via `[index*4+0x8900B4]` à `0x0060A240`,
appel de `0x00602650` (RVA `0x00202650`) à `0x0060A247`.

**Interprétation forte mais incomplète :** boucle principale et chemin de mise à
jour des véhicules/joueurs. Cela ne prouve ni fréquence fixe, ni unité temporelle,
ni identité du thread, ni un emplacement sûr pour patcher. Les appels virtuels
et les branches pause/menu doivent être résolus avant un hook.
Preuves : `lifecycle.json`, `orientation-tick.json`.

## Prochaine passe, sans exécution

1. Suivre tous les producteurs de la base d'orientation et du vecteur candidat
   vitesse, dont les méthodes virtuelles physiques ; contrôler le pas temporel.
2. Remonter l'allocation et la destruction des objets monde référencés par le
   gestionnaire, distincts de l'objet joueur de 0x9C0 octets.
3. Vérifier types, bornes, registres et pile des routines candidates avec leurs
   appelants ; résoudre les sauts partagés et méthodes virtuelles pertinentes.
4. Définir un protocole de mesures runtime pour unités, repère, menus, garage,
   chargements et plusieurs sessions. Son exécution reste soumise à autorisation.

Aucun offset n'a été intégré au client, aucun accès moteur activé, aucun hook
créé. La télémétrie et la seconde voiture locale ne sont pas encore validées.

Suite : [passe statique 2](static-pass-2.md) (producteurs de vitesse, pas temporel,
GameFlow, site de hook, cycle de vie des véhicules monde).
