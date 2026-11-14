# 0041 — Options maison : Joker +4 strict et échange des mains (7-0)

- **Statut** : accepté
- **Date** : 2026-11-14
- **Complète** : ADR 0040 (l'interception est retirée), ADR 0029 (cumul des pénalités)
- **Concerne** : SPEC §4 (`wildDrawFourMode`, `sevenZero`), §5 (cas limites du 7-0)

## Contexte
Avec l'ADR 0040, l'étape 1.6 se réduit à deux options dont le réglage et le protocole existaient déjà mais dont le
moteur manquait : `wildDrawFourMode: strict` et `sevenZero`. Chacune est désactivée par défaut.

## Décision : Joker +4 strict (`wildDrawFourMode`)
- **Un seul endroit juge si une carte est jouable** : `Round::canPlayCard(joueur, carte)` (et `isPlayableFrom` en
  interne) remplace tous les appels à `isPlayable`, qui ne connaît pas la main. Le moteur, la vue (`playableCardIds`),
  le coup forcé de l'application, les bots (qui ne lisent que la vue) et le générateur de coups légaux des tests s'en
  servent : le mode strict ne demande aucun cas particulier ailleurs.
- **En `strict`, un +4 dont la main contient une carte de la couleur courante est refusé** (`ILLEGAL_MOVE` /
  `WILD_DRAW_FOUR_ILLEGAL`, état inchangé). Il n'existe donc plus de bluff : le +4 posé est légal et la cible le pioche
  tout de suite, sans fenêtre de contestation (même résolution qu'un +2 : 4 cartes, tour perdu). La phase
  `AwaitingPenaltyResponse` n'existe jamais dans ce mode.
- **Une réponse à une pénalité en attente ignore cette légalité** : à l'échelle (ADR 0029), un +4 se pose sur un +2
  quelle que soit la couleur ; la légalité de la main ne s'applique qu'à un coup « libre ».
- Une main dont le seul +4 est illégal a toujours une autre carte jouable (la carte de la couleur courante qui le rend
  illégal) : la pioche guidée et « jusqu'à pouvoir jouer » ne changent pas.

## Décision : échange des mains (`sevenZero`)
- **Un 7 échange la main de son joueur avec celle d'un joueur de son choix, un 0 fait passer chaque main au joueur
  suivant dans le sens du jeu** (`Round::exchangeHands`, avant le calcul de l'effet de la carte : le tour passe
  ensuite normalement). À deux joueurs, un 0 est un échange. Les événements `HandsSwapped` et `HandsRotated`
  n'apprennent aucune carte à personne : chacun reçoit simplement sa nouvelle main dans sa vue.
- **La cible d'un 7 voyage dans `swapTargetId`** (déjà prévu par le protocole), traduite en la cible du moteur ; une
  requête qui porte les deux cibles est refusée. Le moteur dit quelle cible il attendait d'après la carte
  (`SWAP_TARGET_REQUIRED` / `SWAP_TARGET_INVALID`, ou `TARGET_NOT_ALLOWED` pour une carte qui n'en a pas).
- **Pas de cible quand il n'y a pas de choix** : à deux joueurs, ou pour la dernière carte. Une dernière carte 7 ou 0
  termine la manche sans rien échanger (SPEC §5), et un 7 ou un 0 retourné au départ n'a aucun effet.
- **Une annonce UNO ne survit pas à un échange** : un joueur dont la main change perd son annonce et sa fenêtre de
  contre-UNO, et un échange n'en ouvre aucune (personne ne s'est « laissé » une carte). Un joueur qui se retrouve à une
  carte doit donc annoncer UNO s'il le faut, comme après une pioche.
- **Le 7 et le 0 deviennent des cartes « spéciales » de la pioche guidée** (ADR 0017) : jamais jouées à la place du
  joueur, puisque le 7 demande une cible et que le 0 bouleverse la table.
- **Présentation** : un échange ou une rotation prend le pas d'un effet (`effectStep`) dans le budget du serveur ;
  le client montre des cartes de dos qui volent d'un siège à l'autre (`swap`, `rotate`).
- **Les bots** reçoivent le réglage (les règles du salon sont publiques) : le bot Facile choisit au hasard, le Normal
  vise le joueur qui a le moins de cartes et préfère poser un 7 quand sa main est plus grande que la leur.

## Alternatives écartées
- **Un 2e prédicat `isPlayableStrict` appelé là où il faut** : on aurait oublié un appelant (la vue, les bots, le coup
  forcé). Un seul point d'entrée qui connaît la main est vérifiable par la simulation.
- **Refuser le +4 illégal seulement à la pose (`validatePlay`)** : l'interface l'aurait montré jouable puis refusé.

## Conséquences
- `WildDrawFourMode` passe de `uno_app` à `uno_core` ; `MatchSettings` et `RoundSetup` portent le mode.
- La simulation tire le mode au hasard et vérifie qu'aucune contestation n'existe jamais en `strict`, et joue le 7-0 sur
  un tiers des parties (les cartes se conservent, aucune fuite).
- Les E2E du 7-0 se jouent à deux joueurs ; le choix d'un joueur parmi plusieurs est couvert par les tests de la table et du
  serveur (le chercheur de graines ne sait chercher qu'à deux).
