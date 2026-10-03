# 0011 — Fenêtre UNO : définition précise

- **Statut** : remplacé en partie par l'[ADR 0018](0018-uno-catch-window.md) (règle de fermeture, plusieurs cibles, durées)
- **Date** : 2026-10-01

## Contexte
La SPEC §3 dit qu'on peut annoncer UNO « juste avant de poser sa carte, ou juste après, tant que le joueur suivant
n'a pas commencé son tour », et qu'un contre-UNO est possible « avant que le joueur suivant ne commence son tour
(jouer ou piocher) ». Il faut préciser les cas limites pour que le moteur soit déterministe, sans ambiguïté et
testable.

## Décision
`Round` garde deux informations : un drapeau « a annoncé UNO » par siège et une **fenêtre UNO** optionnelle
(`Round::unoWindow()`), ouverte sur un joueur.

- **Ouverture** : poser une carte qui laisse exactement **1 carte** en main sans avoir annoncé UNO. La fenêtre est
  ouverte une fois les effets de la carte résolus.
- **Fermeture**, au premier de ces événements :
  - un `PlayCard` ou un `DrawCard` **accepté** (de n'importe quel joueur, y compris le fautif lui-même, par
    exemple à 2 joueurs après un Skip) : c'est « le joueur suivant commence son tour ». Une action refusée ne ferme
    rien ;
  - un `CatchUno` valide : le premier traité gagne, le suivant reçoit `UnoWindowClosed` (SPEC §5) ;
  - un `CallUno` du joueur concerné ;
  - le joueur concerné reçoit des cartes (pénalité) : il n'a plus 1 carte.
- `RespondPenalty` ne ferme **pas** la fenêtre : accepter ou contester un +4 n'est pas « jouer ou piocher ».
  Le joueur visé par un +4 posé en avant-dernière carte peut donc encore attraper le poseur.
- **Annonce avant de jouer** : `CallUno` est accepté pour le joueur courant qui a exactement 2 cartes (en
  `AwaitingPlay` ou `AwaitingDrawnCardDecision`). Le drapeau tient jusqu'à ce que le joueur reçoive des cartes ou
  passe son tour : une annonce ne vaut que pour un seul coup.
- **Annonce non due** (aucune fenêtre sur soi, pas 2 cartes en main, déjà annoncé) : aucun effet, succès sans
  événement (SPEC §3 : « le bouton n'est simplement pas proposé »). Un joueur inconnu est rejeté.
- **Contre-UNO** : `CatchUno{target}` n'est accepté que si la fenêtre est ouverte sur `target`
  (`UnoWindowClosed` sinon) ; se cibler soi-même donne `CannotCatchSelf`. Le fautif pioche 2 cartes
  (`kUnoPenaltyCards`), événements `UnoCaught` puis `PenaltyCardsDrawn`.
- Un contre-UNO est possible pendant la fenêtre de réponse à un +4 et sur un joueur déconnecté (c'est la couche
  `app` qui gère la connexion, pas le moteur).

## Alternatives écartées
- **Fenêtre de durée fixe (timer)** : le cœur n'a pas d'horloge (invariant du projet) et le résultat dépendrait
  du réseau ; l'événement « le joueur suivant joue ou pioche » est déterministe et rejouable.
- **Fermer la fenêtre dès `RespondPenalty`** : le joueur visé par le +4 n'a pas commencé son tour, il subit une
  pénalité.

## Conséquences
- La projection `PlayerView` (étape 1.7) en déduit `catchableTargetIds` (la cible de la fenêtre, pour tous les
  autres joueurs) et `canCallUno`.
- L'invariant « la fenêtre ne vise qu'un joueur à 1 carte sans annonce » est vérifié par `requireRoundInvariants`.
