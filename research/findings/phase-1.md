# Bilan — 19 octobre 2026

**Passe statique suivante :** 30 cibles examinées dans la base Ghidra existante.
Liaison joueur/objet, chemin HotPosition et retrait des liens étayés par les
instructions ; vitesse, orientation et boucle restent des candidats à compléter.
Voir [les observations et limites](static-vehicle-path.md).

**Mise à jour ultérieure du même jour :** après installation de Java par
l'utilisateur, Ghidra 12.1.4 a terminé l'analyse automatique et sauvegardé le
projet en 222 secondes (sortie 0). Le blocage Ghidra/Java mentionné dans le bilan
initial ci-dessous est levé. Les avertissements et le chemin du projet sont
consignés dans `../ghidra/README.md` ; les conclusions sémantiques restent ouvertes.

## Observations

Le dossier projet était vide. L'EXE a été retrouvé dans Téléchargements puis une
installation a été ajoutée par l'utilisateur dans le projet. Les deux fichiers
et la copie `research/binaries/SPEED2.EXE` ont le même SHA-256 attendu et la taille
4 800 512 octets. Les originaux n'ont pas été modifiés. Aucun lancement de jeu,
chargement ASI, injection ou patch mémoire n'a été réalisé.

Le manifeste produit par `tools/inspect_binary.py` reproduit machine 0x14C,
PE32, base 0x00400000, entrée RVA 0x0035BCC7. Marqueurs ASCII HOODLUM aux offsets
fichier 4055 et 3836840. Ce sont des offsets fichier, pas des adresses moteur.

REA `inspect_pe_resources` a examiné les ressources sans exécuter le binaire.
Preuve `ev_b36dfc3d6768ec4cb143c616250a7a826725c83d39a0dfffbc75341d9b259b58`,
conservée dans `pe-resources.json` : 25 ressources de types 3, 10 et 14, aucune de
type 16 (version). Confiance élevée dans cette observation statique.

Le registre communautaire fournit les pistes joueur, flow, sauvegarde de position,
paramètres physiques et type de voiture avec sources figées. `candidate-bytes.json`
relie les candidats aux sections et octets de cette empreinte. Les zones non
initialisées n'ont pas d'octets fichier à lire. Aucun de ces relevés ne valide
les noms, structures, ABI ou durées de vie suggérés par les sources.

## Hypothèses et inconnues

La correspondance taille/entrée et les marqueurs soutiennent l'hypothèse d'une
variante modifiée 1.2 correspondant à la cible NTSC des mods. La région commerciale
et la provenance exacte restent inconnues. Aucun test communautaire d'entrée
ne remplace une validation par empreinte et désassemblage.

Le véhicule joueur, ses transformations, la factory/destruction et le tick restent
non validés. Aucun offset de structure moteur n'a été inventé. ENet est évalué
documentairement uniquement : aucune mesure réseau ou intégration faite.

## Vérifications et limites

- Identité reproduite par SHA-256 PowerShell et parseur Python indépendant.
- Copie et deux originaux concordants ; parser refuse le fichier altéré lors du contrôle négatif.
- Exclusions Git vérifiées sur le dossier jeu, la copie EXE et les clones communautaires.
- Arborescence, CMake x86 et code diagnostic préparés sans code tiers réutilisé.
- Ghidra non configuré selon REA ; Java non résolu. Pas de désassemblage réalisé.
- CMake, compilateur C++ et SDK Windows non détectés dans les emplacements inspectés.
  **Compilation non effectuée : phase 2 préparée, critère de sortie non atteint.**

## Recherches nécessaires, dans l'ordre

1. Fournir une chaîne Windows C++ x86/CMake/SDK, compiler et inspecter PE/exports.
2. Configurer Ghidra et son JDK, importer la copie exacte selon le guide.
3. Suivre les xrefs de PlayersByNumber et GameFlowManager : identité locale,
   indirections, transitions et invalidation des objets.
4. Suivre SaveHotPosition/JumpToHotPosition jusqu'aux données physiques ; établir
   position/orientation/vitesse, repère et unités sans confondre rendu et physique.
5. Identifier la boucle de simulation, son thread et un point de lecture sûr ;
   déterminer ABI, registres et gestion d'erreur avant tout hook.
6. Remonter des listes/types aux fonctions de création/destruction de véhicules
   monde ; documenter ownership, IA, collisions et nettoyage lors des transitions.
7. Après autorisation d'exécution seulement, valider diagnostic puis télémétrie
   sur plusieurs sessions. Tester menus, garage, chargements et changement de voiture.
8. Valider le second véhicule local avant de coder le réseau à deux joueurs.

La phase 1 atteint son objectif de base structurée, identité reproductible et
premières pistes ; l'analyse sémantique reste à mener avec Ghidra. Les comptes,
l'économie, la persistance et le launcher restent hors de cette livraison.
