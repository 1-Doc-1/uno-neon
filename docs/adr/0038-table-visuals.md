# 0038 — Sièges égaux, roue de couleur, impact des pénalités et cinématique de fin de partie

- **Statut** : accepté
- **Date** : 2026-10-11
- **Complète** : ADR 0021 (direction artistique), 0022 (animations pilotées par les événements), 0031 et 0032 (sons)

## Décision
- **Sièges** : pour tous les adversaires, la pastille (avatar, nombre de cartes, pseudo) puis, juste dessous, leur main de
  dos en **accordéon** (cartes droites, décalées ; au plus 15 dos, largeur plafonnée à 3,6 cartes). Plus de siège « en
  face » agrandi ni de rotation (`facing` et `yaw` disparaissent de `SeatPlacement`) : mêmes proportions pour tous, seule
  la table décide si les sièges sont compacts.
- **Défaut de taille selon la parité** : avec 1, 3 ou 5 adversaires, un siège est en haut au centre ; il rétrécit
  l'ellipse de la table, et la boucle de `tableGeometry` réduisait alors les sièges d'un cran jusqu'à ce que les piles
  tiennent, alors que 2 ou 4 adversaires (rien en haut) gardaient des sièges grands. Maintenant la taille des sièges est
  choisie d'après `referenceOpponents(n)` (un nombre pair est mesuré comme l'impair qui le suit) ; la table, elle, est
  dessinée avec les sièges réels. Testé : même taille pour 2k et 2k+1, jamais plus petite à 3 qu'à 4.
- **Roue de couleur** : quatre quarts de disque (un `button` chacun, nommé par la couleur, touches 1 à 4, Échap) qui se
  déploient en cercle autour de la pile de défausse, où la carte va se poser ; la roue est ramenée dans l'écran. Chaque
  quart est un quart de disque posé dans son quadrant : rien ne déborde (mesuré à 375, 768, 1280 et 1920 px). La couleur
  choisie grossit, brille et lance des étincelles pendant 520 ms avant que le choix parte ; en mouvement réduit, il part
  tout de suite.
- **Impact d'un +2 / +4** : un éclair et des ondes de choc (`ImpactBurst` : 2 pour le +2, 4 pour le +4) depuis la
  défausse, qui se tasse et tremble (plus fort pour le +4), 30 puis 40 grains de poussière (le plafond de 40 est
  conservé). Deux sons distincts : deux coups sourds qui s'enfoncent pour le +2, quatre coups qui montent puis un énorme
  boum pour le +4.
- **Cinématique de fin de partie** (`MatchEndCinematic`) : podium (1er au centre, 2e et 3e avec leurs avatars, les autres
  en liste), scores qui défilent de zéro à leur valeur, pluie de cartes et de confettis aux couleurs du jeu, son de victoire
  ou de défaite. Passable d'un clic ou de Entrée / Espace / Échap, 8 s sinon (3,5 s et un simple fondu en mouvement
  réduit), puis l'écran Revanche / Retour au salon (ADR 0037).

## Alternatives écartées
- **Réduire les sièges pour tous les cas dès qu'un siège est en haut** : trop de place perdue à deux joueurs.
- **Garder une main en arc sous le profil** : l'accordéon demandé est droit, plus lisible et plus compact.
- **Animer l'impact par un canvas dédié** : trop de code pour des ondes que le CSS fait seul ; le plafond de 40
  particules reste celui de `ParticleSystem`.

## Conséquences
- `SeatPlacement` n'a plus que `x`, `y`, `angle` et `compact`.
- `ColorPicker` n'est plus dans une `Modal` : c'est un `role="dialog"` à part entière, avec son propre fond.
