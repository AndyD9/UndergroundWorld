# Préparation d'analyse statique

Ghidra **12.1.4 PUBLIC** retrouvé et extrait dans
`C:\Users\PACORCS\Downloads\ghidra_12.1.4_PUBLIC` le 19 octobre 2026.
`Ghidra/application.properties` confirme la version et Java minimum 21 ;
`support/analyzeHeadless.bat` est présent. Le guide fourni exige un **JDK 21 64 bits**.
Mise à jour : JDK Temurin **25.0.4.1+1**, 64 bits, trouvé dans
`C:\Program Files\Eclipse Adoptium\jdk-25.0.4.101-hotspot` après installation
par l'utilisateur. `java.exe` et `javac.exe` sont présents. Ghidra démarre avec
ce JDK et accepte l'import PE sous `x86:LE:32:default:windows`.
Analyse automatique **terminée avec succès**, sauvegarde et import confirmés,
code de sortie 0, durée d'analyse annoncée 222 secondes. Projet conservé :
`projects/UGWorld-6ed470fc646e492b8255f886e9bc3f8b.gpr`.
Voir `analysis-run.log` pour le journal complet.

Limites signalées : pas d'informations PDB ; bibliothèques externes non importées ;
données PNG invalides à 0x007DBE98 et 0x007DC4F0 ; lectures impossibles pendant la
décompilation à 0x004CA6D0 et 0x0070CF98 (cibles 0x01000000 et 0x01000100).
La réussite de l'analyse automatique ne valide pas les noms communautaires,
les structures moteur ou les conventions d'appel. Une première passe sur les
xrefs est documentée dans `../findings/static-vehicle-path.md`.

## Inspection reproductible de la base existante

```powershell
./tools/inspect-ghidra.ps1 -OutputName nouvelle-passe -Addresses '008900AC','005EB450','00600E10'
```

Les adresses doivent être des chaînes hexadécimales entre guillemets pour éviter
que PowerShell interprète le E comme un exposant. Le lanceur refuse les noms de
sortie déjà présents ; utiliser un nouveau nom. La base est ouverte en lecture
seule, sans relancer l'analyse automatique. Les exports bruts, contenant du
pseudocode propriétaire, restent locaux et ignorés par Git. Les six exports de
la première passe contiennent 30 cibles, sans troncature explicite et avec succès
de décompilation pour les fonctions demandées ; les limites de l'analyse
automatique et les appels indirects non résolus subsistent.

Pour reproduire, lancer :

```powershell
./tools/analyze-ghidra.ps1 -JavaHome 'C:\Program Files\Eclipse Adoptium\jdk-25.0.4.101-hotspot'
```

Ce script définit les variables uniquement pour son processus, vérifie l'empreinte
et importe la copie locale. Il ne lance jamais le jeu. Il ne configure pas le
serveur REA déjà actif : celui-ci conserve son environnement de démarrage.

Quand Ghidra et son JDK requis seront disponibles : importer uniquement
`research/binaries/SPEED2.EXE`, après reproduction du manifeste. Choisir PE,
langage `x86:LE:32:default`, compiler Windows, base 0x00400000. Confirmer dans
l'import le point d'entrée RVA 0x0035BCC7 et les sections du manifeste. Laisser
l'analyse automatique terminer et consigner versions, options et limitations.
Conserver le projet dans `research/ghidra/projects/` (ignoré).

Ordre : vérifier les octets aux candidats sourcés, suivre xrefs et appels ;
identifier les producteurs/consommateurs des listes de véhicules, puis le chemin
du joueur local, transformations et tick. Comparer les conventions d'appel,
bornes, indirections et branches de transitions ; ne pas importer aveuglément
des types ou signatures communautaires. VA préférée = base + RVA ; les offsets
fichier passent par les sections, pas une simple soustraction de base.

Chaque résultat doit préciser SHA-256, VA/RVA, plages d'octets ou preuve et
source figée, observation/interprétation, confiance et prochaine validation.
Une présence d'octets ne prouve pas le sens d'une fonction. Les états runtime,
unités et durées de vie resteront inconnus jusqu'aux essais autorisés.
