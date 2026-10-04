# 0025 — Rythme du jeu : durées centralisées, vitesse réglable, pioche depuis le paquet

- **Statut** : accepté
- **Date** : 2026-10-04

## Contexte
Retour de partie : tout allait trop vite pour qu'on suive, les cartes piochées « montaient du bas de l'écran avec un
saut » au lieu de partir du paquet, le sens du jeu et les mains adverses étaient peu lisibles.

**Cause du bug de pioche** : quand une pioche ouvrait la file d'effets, elle démarrait aussitôt, avant que la main ait
rendu la carte qu'elle vient de recevoir. La destination de la carte (`Hand.lastRect`) était alors inconnue, et le repli
visait la boîte entière de la main : une carte de 1 200 px de large qui « montait » du bas. Les démonstrations n'avaient
pas le défaut, car une pose précédait toujours la pioche et laissait le temps de rendre.

## Décision
- **Cause corrigée** : la couche d'effets place un effet *après* le rendu (`afterNextRender`), et le repli d'une carte
  de ma main dont la place n'est pas connue est une carte de la taille du paquet au milieu de la main. Une carte piochée
  part toujours du paquet.
- **Un seul endroit pour les durées** : `client/src/app/ui/motion.ts` (`MOTION_MS`). `MotionPreferences` les publie en
  propriétés CSS `--motion-*` ; `tokens.scss` porte le multiplicateur global `--motion-scale` (et `--dur-fast` /
  `--dur-base` en dépendent). La TS reste la source : les effets reçoivent leur durée par propriété inline `--dur`.
- **Rythme** : pose ~700 ms avec un rebond, ~380 ms de pause ; effets spéciaux 1 à 1,4 s ; cartes d'une pioche à
  ~150 ms d'écart. Le serveur laisse 1,2 s (au lieu de 700 ms, toujours injectable) avant un coup forcé.
- **Vitesse** : réglage « Normale / Rapide » (×0,6) propre au navigateur (`localStorage`, jamais envoyé au serveur).
  Au-delà de 3 étapes en attente, la file accélère (au plus ×0,4) pour rattraper son retard ; elle est abandonnée
  au-delà de 9 s de retard à vitesse normale.
- **Sens du jeu** : la lueur du liseré a pour tête une flèche, pilotée par la même animation CSS (`@property
  --orbit-angle`) pour rester calée dessus. L'inversion a sa propre animation : une grande flèche circulaire fait un tour
  dans l'ancien sens et se retourne, la lueur et sa flèche repartent à l'envers.
- **Mains adverses** : des dos de cartes en arc inversé, avec perspective CSS ; en face, une grande main ; sur les côtés,
  une rotation autour de l'axe vertical vers le centre (jamais à plat) ; 15 dos au plus, le badge donne le nombre.

## Alternatives écartées
- **Lire les durées dans le CSS depuis la TS** : fragile en test (jsdom) ; la TS publie plutôt ses valeurs.
- **Position d'arrivée par `offset-path`** pour la flèche : rendu incohérent selon le moteur ; trigonométrie CSS
  (`sin`, `cos`, `atan2`) à la place.
- **Retarder le serveur pour laisser voir les animations** : le serveur ne connaît pas l'écran du joueur ; seul le coup
  forcé garde un délai (injectable).

## Conséquences
- Une pioche de plusieurs dizaines de cartes s'anime en plusieurs secondes : la file est abandonnée au-delà de 9 s.
- Les tests E2E tournent en mouvement réduit : ils ne dépendent pas de ces durées.
