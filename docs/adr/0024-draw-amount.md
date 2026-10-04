# 0024 — Pioche « jusqu'à pouvoir jouer » : `drawAmount`, une action atomique, un événement par carte

- **Statut** : accepté
- **Date** : 2026-10-04

## Contexte
Un joueur qui n'a rien à jouer pioche une carte, qui ne convient souvent pas : le tour passe et la partie s'étire.
La SPEC §4 prévoyait pour cela une option `drawUntilPlayable` (booléen, jamais implémentée, refusée par le serveur).
Elle se combine avec la règle de pioche guidée (ADR 0017) sans en dépendre : `drawRule` dit **si** on peut piocher,
la nouvelle règle dit **combien**.

## Décision
- Réglage de salon `drawAmount` : `"untilPlayable"` (défaut des salons) ou `"one"` (règle officielle). Dans le cœur,
  `MatchSettings` et `RoundSetup` valent `One` par défaut, comme `drawRule` vaut `Official` (ADR 0017) : le moteur pur
  garde son comportement historique, la couche `app` choisit `untilPlayable`. Le booléen `drawUntilPlayable` du
  protocole disparaît, remplacé par `drawAmount` (jamais utilisé : aucune migration).
- Avec `untilPlayable`, un joueur qui n'a **aucune** carte jouable pioche jusqu'à obtenir une carte jouable. Une pioche
  **volontaire** (une carte était jouable et il choisit de piocher, par exemple pour garder un +2) reste d'une carte.
- Toute la pioche est **une seule action** `DrawCard` : une seule version d'état et une seule diffusion. Le moteur émet
  **un `CardsDrawn` par carte** (et un `DeckReshuffled` à l'endroit où la défausse est remélangée). Le client rythme
  l'animation ; le serveur ne dort pas entre deux cartes.
- La boucle se termine toujours : chaque tour de boucle retire une carte de la pioche (remélangée à partir de la
  défausse sauf la carte du dessus si besoin), ou constate que pioche et défausse n'ont plus rien à donner et s'arrête.
  Dans ce cas le tour passe, comme dans la règle à une carte. Un `CardsDrawn` à zéro carte est émis si rien n'a pu être
  pioché du tout (comportement historique).
- Après la dernière carte, le comportement habituel s'applique : jouable → `AwaitingDrawnCardDecision` (posée par le
  serveur si elle est normale en pioche guidée, choix du joueur si elle est spéciale ou en règle officielle) ; sinon
  fin du tour.
- **Projection** : inchangée (ADR 0015). Chaque `CardsDrawn` est projeté séparément : les cartes de l'événement ne
  sont données qu'à celui qui pioche, les autres reçoivent `count: 1`.
- Le minuteur de tour et `forcedAction` n'ont rien à changer : ils appliquent `DrawCard`, qui fait toute la pioche.

## Alternatives écartées
- **Un message ou une action par carte** : plusieurs versions d'état, des états intermédiaires visibles par les
  autres joueurs, et un client qui pourrait en demander trop.
- **Garder le booléen `drawUntilPlayable`** en plus de `drawAmount` : deux réglages pour la même idée.
- **Le moteur attend entre deux cartes** : il n'a pas d'horloge (CLAUDE.md) ; le rythme est affaire de présentation.

## Conséquences
- La simulation joue les deux valeurs (une graine sur deux, indépendamment des autres règles) et vérifie qu'après une
  pioche forcée le joueur a une carte jouable à décider **ou** que pioche et défausse sont vides ; une pioche
  volontaire est d'au plus une carte.
- Les E2E fixent explicitement `drawAmount` (`one` par défaut dans la fixture `lobby`) : leurs graines ne dépendent
  pas de la valeur par défaut des salons. Le chercheur de graines prend `UNO_FIND_DRAW_AMOUNT`.
- Une pioche peut atteindre plusieurs dizaines de cartes dans des cas extrêmes : le client doit plafonner ses
  animations (lot suivant, `feat/game-feel`).
