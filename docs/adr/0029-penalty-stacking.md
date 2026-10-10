# 0029 — Cumul des pénalités : l'échelle +2 < +4 < +5

- **Statut** : accepté
- **Date** : 2026-10-10
- **Complète** : ADR 0017 (pioche guidée), ADR 0019 (UNO obligatoire), ADR 0027 (résolution d'un effet), ADR 0028 (Joker +5)
- **Remplace** : l'ancien réglage `stacking` (`off` / `sameType` / `mixed`) de la SPEC §4, jamais implémenté

## Contexte
Les règles officielles ne permettent pas d'empiler les pénalités : un +2 se pioche tout de suite, un +4 s'accepte ou se
conteste, seul un +5 répond à un +5 (ADR 0028). L'hôte veut une règle maison où une pénalité en attente se renvoie plus
fort, sans fenêtre « Accepter / Contester » qui coupe le jeu.

## Décision
- **Un réglage de salon** `stacking` (hôte seulement) : `official` (défaut, comportement inchangé) ou `ladder`.
  `core::PenaltyStacking`, transmis par `RoomSettings` → `MatchSettings` → `RoundSetup`.
- **L'échelle** : `penaltyLevel` (+2 = 1, +4 = 2, +5 = 3) et `canStackOn(carte, dernière carte de la pile)` : une carte de
  pénalité se pose sur une pénalité en attente de niveau inférieur ou égal. +2 sur +2 seulement ; +4 sur +2 ou +4 ; +5
  sur tout. **La couleur ne compte jamais** pour empiler (un +2 vert sur un +2 bleu). Les montants s'additionnent.
- **Une seule phase pour toutes les piles** : `AwaitingPlusFiveResponse` devient `AwaitingStackResponse{total, top}`,
  où `top` est le rang de la dernière carte posée. En règle officielle seul un +5 y entre (donc seul un +5 répond,
  comme avant) ; avec l'échelle, un +2 et un +4 y entrent aussi. Pas de seconde phase : les départs de joueur, l'UNO
  obligatoire, le minuteur de tour et la pioche guidée passent par le même code.
- **Cible** : +2 et +4 visent le joueur suivant **dans le sens du jeu** au moment de la pose (`turnOrder_.next()`), le
  +5 la cible choisie par le poseur. Le tour passe tout de suite à la cible, qui peut empiler, ou accepter.
- **Pas de fenêtre pour la cible** : ses cartes empilables sont dans `playableCardIds` dès l'arrivée de la pénalité ;
  un clic sur le paquet (`DrawCard`, `canDraw` vrai pendant une pile à l'échelle) prend le total ; `RespondPenalty`
  `accept` reste accepté (minuteur de tour, coup forcé). La cible saute son tour ; le jeu reprend au joueur qui la suit.
- **Pioche automatique** : en pioche guidée, une cible sans carte empilable reçoit le coup forcé `accept` (après
  `actionsOpenAt` + `forcedAction`, rythmé par `drawStepMs`). En pioche `official`, comme pour le +5 (ADR 0028), elle
  choisit toujours : elle clique sur le paquet.
- **Contestation** : désactivée à l'échelle, le +4 est alors toujours accepté (aucune vérification de la main du
  poseur) ; `RespondPenalty::Challenge` est refusé par `CannotChallenge`. En règle officielle, tout est inchangé.
- **Dernière carte** : une pile qui se termine par la dernière carte d'un joueur finit la manche ; la dernière cible
  pioche tout le total, ces cartes comptent dans le score. `endRound` reçoit une `Penalty{cible, total}` calculée par
  `penaltyOfPlay`, qu'il s'agisse d'un +2 officiel ou d'une pile.
- **Erreur** : une carte trop faible ou qui n'est pas une carte de pénalité est refusée (`NotStackable` →
  `ILLEGAL_MOVE` / `CANNOT_STACK`, raison déjà prévue au protocole) ; sur une pile dont la dernière carte est un +5,
  `ONLY_PLUS_FIVE_PLAYABLE` est conservé.
- **Rythme** : la pose d'un +2 ou d'un +4 empilé coûte la carte et le « +2 » / « +4 » (`playStep + effectStep`) ; les cartes
  de la pénalité, `drawStep` chacune, quand elle est prise (ADR 0027).
- **Client** : le serveur dit tout (`playableCardIds`, `canDraw`, `penaltyResponse`). Il n'y a pas de fenêtre quand
  `canDraw` est vrai pendant une pénalité sans contestation ; l'étiquette du paquet devient « Piocher N cartes ». Les
  cartes qui répondent sont mises en évidence **dès l'arrivée de la pénalité**, verrouillées jusqu'à `actionsOpenAt` (le
  serveur refuse avant, ADR 0027) ; la fenêtre du +5 officiel apparaît tout de suite, son bouton attend l'ouverture.

## Alternatives écartées
- **Une phase de plus** (`AwaitingLadderResponse`) : trois phases de pénalité auraient dupliqué les départs, l'UNO et le
  minuteur pour rien. La pile est la même chose à chaque niveau.
- **Garder `off` / `sameType` / `mixed`** : jamais implémentés, et aucun ne décrit l'échelle voulue.
- **Laisser la cible répondre avant `actionsOpenAt`** : ce serait contredire l'ADR 0027 pour un confort d'affichage ;
  le client montre la réponse possible tout de suite, le serveur ouvre à l'heure dite.

## Conséquences
- L'enum de protocole `StackingMode` devient `PenaltyStacking` (`official` / `ladder`), tous les exemples sont mis à jour.
- `holdsWildDrawFive` devient `canStackOnPending`, `AwaitingPlusFiveResponse` devient `AwaitingStackResponse`.
- La simulation joue une partie sur deux à l'échelle ; l'invariant « le total s'additionne » remplace « multiple de 5 »
  (conservé en règle officielle).
