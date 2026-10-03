# 0018 — Fenêtre de contre-UNO : grâce, échéance, plusieurs cibles

- **Statut** : accepté (remplace la règle de fermeture de l'[ADR 0011](0011-uno-window.md))
- **Date** : 2026-10-03

## Contexte
L'ADR 0011 fermait la fenêtre dès qu'un `PlayCard` ou un `DrawCard` était accepté, de n'importe quel joueur. Conséquence
vécue en partie : à trois joueurs ou plus, seul le joueur suivant pouvait contrer, et seulement avant d'agir ; après un
Skip ou un Reverse à deux, le bouton « Contre-UNO ! » disparaissait avant qu'on ait pu l'appuyer. Un adversaire n'avait
« rien sur quoi cliquer » (bug G1 : ni la couche `app` ni le client ne filtraient sur le tour, c'était la fenêtre qui se
fermait trop tôt). Le moteur n'a pas d'horloge, et une connexion lente ne doit pas faire perdre un joueur qui annonce
UNO à temps.

## Décision
**Moteur (`Round`)** : une liste de fenêtres (`unoWindows()`), une par joueur, la plus ancienne d'abord.
- **Ouverture** : un joueur passe à exactement 1 carte sans avoir annoncé UNO (pose de l'avant-dernière carte).
- **Fermeture**, au premier de ces événements : le joueur annonce UNO ; il est contré ; il n'a plus exactement 1 carte
  (il reçoit des cartes) ; la manche se termine ; il est retiré de la manche ; **ou** l'application ferme la fenêtre
  (`closeUnoWindow`) faute de temps. Jouer ou piocher, quel que soit le joueur, ne ferme **plus** rien.
- **Annonce** : possible pendant son tour avec 2 cartes (l'annonce est annulée si le joueur ne descend pas à 1 carte ou
  s'il reçoit des cartes), ou **à tout moment avec 1 carte** tant qu'il n'a pas été contré, même après l'échéance.
- **Contre-UNO** : `CatchUno{target}` est accepté de tout joueur de la manche autre que la cible, tant que la fenêtre de
  la cible est ouverte. Le premier valide inflige 2 cartes et ferme la fenêtre ; les suivants reçoivent `UnoWindowClosed`
  (aucun effet).

**Application (`Application`)** : le temps, comme pour le minuteur de tour (ADR 0016).
- À l'ouverture, `graceEndsAt = ouverture + 2 s` et `expiresAt = ouverture + 15 s` (`Timeouts::unoGrace`, `unoWindow`,
  injectables). Un timer d'échéance appelle `Match::closeUnoWindow`, puis diffuse la vue. Un timer périmé (fenêtre
  fermée puis rouverte) ne fait rien : il porte son `expiresAt`.
- **Grâce** : avant `graceEndsAt`, un `CatchUno` d'un autre joueur est refusé avec `UNO_GRACE_PERIOD` ; seule la cible peut
  encore annoncer. La partie continue pendant ce temps.
- La vue expose `unoWindows: [{targetId, graceEndsAt, expiresAt}]` (heure serveur) à tous les joueurs, cible comprise.
  `me.catchableTargetIds` disparaît : une seule source de vérité. Le client affiche le bouton quand la grâce est passée ;
  le serveur décide.

## Alternatives écartées
- **Mettre le temps dans le moteur** (horloge injectée) : contraire à l'invariant du cœur, résultats non rejouables.
- **Une seule fenêtre à la fois** : deux joueurs peuvent descendre à 1 carte en moins de 15 s ; le second aurait perdu sa
  fenêtre, ou le premier la sienne.
- **Fermer la fenêtre au tour suivant mais l'allonger de 2 s** : ne règle pas les parties à 3 joueurs ou plus.

## Conséquences
- Les tests de l'ADR 0011 « se ferme quand le joueur suivant joue / pioche » sont remplacés par leur contraire.
- L'invariant « la fenêtre ne vise qu'un joueur à 1 carte sans annonce » devient « chaque fenêtre, une par joueur ».
- Un `CatchUno` arrivé pendant la grâce coûte un message d'erreur de plus au client malveillant, rien d'autre : la
  limitation de débit reste inchangée.
