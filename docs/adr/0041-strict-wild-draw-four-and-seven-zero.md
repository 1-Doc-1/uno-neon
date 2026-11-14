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

## Alternatives écartées
- **Un 2e prédicat `isPlayableStrict` appelé là où il faut** : on aurait oublié un appelant (la vue, les bots, le coup
  forcé). Un seul point d'entrée qui connaît la main est vérifiable par la simulation.
- **Refuser le +4 illégal seulement à la pose (`validatePlay`)** : l'interface l'aurait montré jouable puis refusé.

## Conséquences
- `WildDrawFourMode` passe de `uno_app` à `uno_core` ; `MatchSettings` et `RoundSetup` portent le mode.
- La simulation tire le mode au hasard et vérifie qu'aucune contestation n'existe jamais en `strict`.
