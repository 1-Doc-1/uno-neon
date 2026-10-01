# 0013 — Projection `PlayerView` : périmètre du cœur et test anti-fuite

- **Statut** : accepté
- **Date** : 2026-10-01

## Contexte
La SPEC §7.2 donne `project(const Round&, PlayerId) -> PlayerView` et `project(const DomainEvent&, PlayerId) ->
optional<ClientEvent>`. Le schéma du protocole (`player-view.schema.json`) contient des champs que le moteur ne
connaît pas (pseudo, connexion, hôte, échéances, `stateVersion`, `settings`) et d'autres qu'une `Round` seule ne
connaît pas (scores, numéro de manche, gagnant de la partie).

## Décision
- **Le cœur projette la partie « jeu » de la vue** : `PlayerView` (dans `uno_core`) ne contient que ce que le moteur
  sait. La couche réseau (2.4) y ajoute pseudo, connexion, hôte, échéances et `stateVersion` en la sérialisant.
- **Signature** : `project(const Round&, viewer, const MatchProgress&)`, où `MatchProgress` (numéro de manche,
  scores par siège, gagnant) est l'état que `Match` accumule hors de la manche ; `project(const Match&, viewer)` en
  est le raccourci. C'est un écart mineur à §7.2, qui ignorait que `PlayerView` porte des scores.
- **Les « peut-il… » sont calculés par le moteur** (`playableCardIds`, `canDraw`, `canPass`, `canCallUno`,
  `canChooseColor`, `penaltyResponse`, `catchableTargetIds`) : le client n'a aucune règle à connaître.
  `canStack` vaut toujours `false` tant que le cumul n'existe pas (étape 1.6).
- **Mains adverses** : jamais dans la vue, seulement leur nombre. À la fin d'une manche, `roundResult` révèle toutes
  les mains restantes, puisque la règle les compte dans le score (SPEC §3).
- **Légalité d'un +4 en attente** : absente de la vue (`wasLegal` reste dans la phase) ; seule la contestation la
  révèle, par l'événement.
- **Test anti-fuite** (invariant de sécurité n°2), trois niveaux :
  1. *non-interférence* : deux manches qui ne diffèrent que par ce qu'un joueur ne doit pas savoir (mains des
     autres, ordre de la pioche, légalité d'un +4) donnent exactement la même `PlayerView` (`==`). Un champ qui
     fuirait casserait l'égalité, même s'il est ajouté plus tard ;
  2. *inventaire* : `requireViewLeaksNothing` vérifie qu'aucun identifiant de carte de la vue n'est hors de la main
     du joueur, du dessus de la défausse et, en fin de manche, des mains révélées ;
  3. *aléatoire* : l'inventaire est vérifié pour chaque joueur après chaque action de parties simulées (étape 1.8).
- **Projection des événements reportée en 2.4.** `project(DomainEvent) -> ClientEvent` suppose des événements qui
  portent les cartes entières (`cardPlayed.card`, `cardsDrawn.cards`), alors que les événements du domaine ne
  portent que des `CardId`. La changer maintenant ferait réécrire les événements et leurs tests pour un format
  (`ClientEvent`) que seule la sérialisation réseau fixe ; le test anti-fuite des événements (cartes piochées, main
  révélée par une contestation) sera écrit avec elle.

## Alternatives écartées
- **`PlayerView` complète dans le cœur** (pseudo, connexion…) : le cœur n'a aucune notion de connexion ni
  d'horloge (invariant du projet).
- **Un seul test d'inventaire** : il ne couvre que les champs qu'on a pensé à lister ; la non-interférence, elle,
  couvre tout champ futur.

## Conséquences
- 2.4 doit ajouter la projection des événements et son test anti-fuite (PROGRESS.md).
- Les écrans (phase 3) lisent les booléens `can…` sans réimplémenter la moindre règle.
