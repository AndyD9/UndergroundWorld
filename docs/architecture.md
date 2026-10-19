# Architecture et feuille de route

Le client ASI est Windows x86. `core` porte identité, cycle de vie et diagnostic ;
`game` est l'unique frontière autorisée vers le moteur ; `vehicles` contient des
copies de données appartenant à l'application, jamais des structures moteur
supposées. `network` gérera un transport séparé du thread de simulation.

Les lectures/créations/destructions devront se faire sur un thread moteur validé.
Transférer des snapshots possédés par valeur entre moteur et réseau, avec files
bornées. À chaque transition menu/chargement/garage, invalider les handles et
vider les snapshots. Aucun pointeur moteur ne sort de la frontière `game`.
Le repère, les unités et la durée de vie des objets restent inconnus.

Le serveur indépendant C++ gérera sessions et routage. ENet est le candidat initial
(voir sources/licence), à mesurer sur latence/pertes et charge. Sérialisation
explicite, limites de débit et de taille, validation des valeurs finies, séquences
avec gestion du rebouclage, déconnexion et timeout sont nécessaires. Un tampon
d'interpolation distant sera dimensionné par mesure ; ne pas choisir une cadence
de simulation avant de l'avoir observée.

Le prototype fera confiance aux positions déclarées par les clients : pas de
garantie anti-triche ni de validation compétitive. ENet seul n'apporte pas
d'authentification applicative ou de chiffrement. Le serveur devra néanmoins
refuser messages mal formés et identités de session incohérentes.

1. Base/statique : empreinte reproductible, pistes et inconnues tracées.
2. Diagnostic : compiler x86 ; valider le chargement seulement après autorisation.
3. Télémétrie : position/orientation/vitesse cohérentes sur plusieurs sessions et transitions.
4. Second véhicule local : création, contrôle et destruction stables, interactions physiques comprises.
5. Deux joueurs : protocole versionné, interpolation, tests pertes/délai et déconnexion propre.
6. Ensuite seulement : échelle, proximité, courses, comptes, garages, économie, launcher.

Les étapes 3 à 5 nécessitent des preuves d'exécution autorisées. Les comptes et la
persistance ne doivent pas devancer le jalon deux joueurs.
