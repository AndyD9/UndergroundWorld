Proposition, pas encore un format réseau : version, type, longueur, identifiant de
session et de véhicule, séquence, position, orientation, vitesse. Définir encodage
explicite, ordre des octets, limites et refus des NaN/Inf avant implémentation.
Ne jamais transmettre une structure C++ brute. Réserver un canal fiable aux
connexions/destructions et des états séquencés non fiables aux mouvements.
