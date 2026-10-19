# Sources et provenance

Recherche effectuée le 19 octobre 2026. Les dépôts officiels des auteurs ont été consultés sur GitHub puis clonés pour examiner le code à une révision immuable. Les copies de travail sous `research/upstream/` sont des références locales à exclure du suivi Git et de toute livraison.

| Source | Révision inspectée | Licence observée | Usage dans Underground World |
| --- | --- | --- | --- |
| [NFSU2 Extra Options, ExOptsTeam](https://github.com/ExOptsTeam/NFSU2ExOpts/tree/801539e248cea6a91769dce72e7f086753a29423) | `801539e248cea6a91769dce72e7f086753a29423` | [LICENSE GNU GPL v3](https://github.com/ExOptsTeam/NFSU2ExOpts/blob/801539e248cea6a91769dce72e7f086753a29423/LICENSE) | Pistes documentaires; aucun code intégré. |
| [NFSU2 Unlimiter, nlgxzef](https://github.com/nlgxzef/NFSU2Unlimiter/tree/45b6738378d76755d0dd0a19a68d26cfa70af250) | `45b6738378d76755d0dd0a19a68d26cfa70af250` | Aucun fichier LICENSE/COPYING trouvé dans cette révision; permission de réutilisation non établie. | Pistes documentaires; aucun code intégré. |
| [ENet, Lee Salzman](https://github.com/lsalzman/enet/tree/5a9c537fd464b3c6d3c55e1d3bd47588faf71b42) | `5a9c537fd464b3c6d3c55e1d3bd47588faf71b42` | [MIT, avis 2002–2024](https://github.com/lsalzman/enet/blob/5a9c537fd464b3c6d3c55e1d3bd47588faf71b42/LICENSE) | Transport UDP candidat, dépendance à intégrer explicitement avec son avis de licence. |

Les noms de fonctions et adresses attribués par la communauté ne sont pas des symboles officiels du jeu. Leur présence dans une source ne prouve ni la compatibilité avec notre exécutable ni la validité d'une convention d'appel. Voir [les pistes communautaires](../research/findings/community-leads.md).

## ENet : décision de transport

L'en-tête inspecté déclare la version **1.3.18**, les paquets fiables, les canaux et la boucle `enet_host_service`: [enet.h](https://github.com/lsalzman/enet/blob/5a9c537fd464b3c6d3c55e1d3bd47588faf71b42/include/enet/enet.h#L21-L28), [indicateurs des paquets](https://github.com/lsalzman/enet/blob/5a9c537fd464b3c6d3c55e1d3bd47588faf71b42/include/enet/enet.h#L99-L138), [API](https://github.com/lsalzman/enet/blob/5a9c537fd464b3c6d3c55e1d3bd47588faf71b42/include/enet/enet.h#L565-L582).

Choix d'architecture proposé, à tester : canal fiable pour connexion, apparition et disparition; canal distinct pour états fréquents séquencés sans garantie de livraison. Le transport ne remplace pas le format de messages, les validations serveur, la limitation du débit ou la gestion des identifiants. La présence de cette recherche ne signifie pas qu'ENet est déjà intégré ou testé dans le client.
