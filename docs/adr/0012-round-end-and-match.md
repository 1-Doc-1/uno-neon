# 0012 — Fin de manche et `Match` : score, donneur, fin de partie

- **Statut** : accepté
- **Date** : 2026-10-01

## Contexte
L'étape 1.5 ajoute la fin d'une manche, le score et la fin de partie (SPEC §3, §4, §7.1). Plusieurs points ne sont
pas tranchés par la SPEC : que devient un +4 posé en dernière carte, qui garde les scores, et qui gère le donneur.

## Décision
- **Fin de manche** : poser sa dernière carte fait passer `Round` dans la phase `RoundOver{winner, points}` et émet
  `RoundEnded`. Toute action est ensuite refusée avec `InvalidPhase`, `CallUno` et `CatchUno` compris.
- **Dernière carte +2 ou +4** : le joueur suivant pioche quand même 2 ou 4 cartes (SPEC §3), et ces cartes comptent
  dans les points. Un +4 posé en dernière carte **ne peut pas être contesté** : il n'y a plus de partie à
  poursuivre. Il n'y a ni `PlayerSkipped` ni `TurnChanged` : la manche est finie.
- **Points** : somme de la valeur des cartes des autres mains (`cardPoints`, `handPoints` dans `scoring.hpp`),
  calculée une fois, au moment où la manche se termine.
- **`Match`** : valeur pure comme `Round` (ADR 0010), sans dépendance stockée ; le `RandomSource&` est passé aux
  opérations qui distribuent. Elle tient les scores par siège, le numéro de manche et le gagnant. Seul le
  gagnant d'une manche marque, donc un seul joueur peut franchir l'objectif : le gagnant de la partie est le
  gagnant de la manche qui atteint `targetScore`, ou celui de l'unique manche si `targetScore` est vide
  (`singleRound`). `MatchEnded` suit alors `RoundEnded`.
- **Donneur** : tiré au hasard à la première manche, puis il avance d'un siège dans le sens horaire à chaque
  `startNextRound` (SPEC §3). `startNextRound` n'est accepté qu'entre deux manches d'une partie non terminée.
- Le deck est créé par `createStandardDeck` puis mélangé par `Match` : `Round::start` le reçoit déjà mélangé
  (ADR 0010).

## Alternatives écartées
- **Laisser la couche `app` tenir les scores** : la règle « 500 points » est une règle du jeu, elle doit être
  testée sans réseau.
- **Autoriser la contestation d'un +4 final** : il faudrait une phase de plus et un retour en arrière sur la fin
  de manche ; la SPEC dit que le suivant « pioche quand même ».

## Conséquences
- La projection `PlayerView` (étape 1.7) lit `roundResult` dans `RoundOver` et `matchOver` dans `Match::winner()`.
- La revanche (étape 4.3b) recrée simplement un `Match`.
