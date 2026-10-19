# Underground World

Prototype multijoueur C++ pour Need for Speed Underground 2 : conserver Bayview,
la conduite, le tuning et l'ambiance. Premier jalon : deux joueurs roulant ensemble.

État au 19 octobre 2026 : base et recherche statique réalisées, empreinte locale
confirmée. ASI de diagnostic **compilé en x86** (MSVC 19.44, CRT statique) avec
amorçage DllMain, validé hors jeu puis **chargé en jeu sans plantage** (empreinte
reconnue, profil moddé). Phase 3b en cours : hook de frame et télémétrie **lecture
seule** implémentés, compilés et testés hors jeu ; **pas encore exécutés en jeu**.
Aucune synchronisation réseau implémentée.

Mise à jour : Ghidra 12.1.4 fonctionne avec le JDK Temurin 25 installé ; l'import,
l'analyse automatique x86 et la sauvegarde du projet ont réussi. Voir le guide
Ghidra pour le projet et les avertissements conservés.

- [Compilation et diagnostic](docs/build.md)
- [Télémétrie lecture seule et protocole de test](docs/telemetry.md)
- [Architecture et étapes](docs/architecture.md)
- [Bilan et prochaines recherches](research/findings/phase-1.md)
- [Sources et licences](docs/sources.md)
- [Pistes communautaires](research/findings/community-leads.md)
- [Résultats statiques joueur, position et cycle de vie](research/findings/static-vehicle-path.md)
- [Passe statique 2 : vitesse, GameFlow, hook, véhicules monde](research/findings/static-pass-2.md)
- [Identité reproductible](research/findings/binary-manifest.json)
- [Préparation Ghidra](research/ghidra/README.md)

Les exécutables, installations du jeu et clones de référence sont exclus de Git.
Le projet n'inclut ni le jeu ni du code communautaire. La licence de distribution
du code original du projet reste à décider avant publication.
