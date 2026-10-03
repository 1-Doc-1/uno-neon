# 0017 — Pioche guidée : le moteur dit quoi, l'application dit quand

- **Statut** : accepté
- **Date** : 2026-10-03

## Contexte
La règle officielle (SPEC §3) laisse un joueur piocher même quand il peut jouer, puis jouer la carte piochée ou la
garder. En pratique, cela ajoute des clics sans décision : quand on n'a rien à jouer on pioche (un clic), quand une
seule carte convient on la joue. On veut une pioche **guidée** : le jeu ne demande un choix que lorsqu'il y en a un.
La règle officielle doit rester disponible et testée.

## Décision
Réglage du salon `drawRule` : `"guided"` (défaut des salons) ou `"official"`. Dans le cœur, `MatchSettings::drawRule`
et `RoundSetup::drawRule` valent `Official` par défaut : le moteur pur garde son comportement historique, c'est la
couche `app` qui choisit `guided` pour les salons.

**Cartes spéciales** : `DrawTwo`, `Wild`, `WildDrawFour`. `Skip` et `Reverse` comptent comme des cartes normales.

**Règle guidée** (le moteur valide, par `DomainError::MustPlay` → `ILLEGAL_MOVE` / `MUST_PLAY`) :
- aucune carte jouable → piocher est permis, et c'est le seul coup ;
- des cartes jouables mais aucune spéciale → piocher est **refusé**, il faut jouer ;
- au moins une carte spéciale jouable → le joueur choisit : jouer, ou piocher ;
- carte piochée non jouable → fin du tour ;
- carte piochée jouable et normale → elle doit être jouée (passer est refusé) ;
- carte piochée jouable et spéciale → la jouer, ou la garder (`Pass`).

**Le moteur dit QUOI** : `Round::canDraw(joueur)`, `Round::canKeepDrawnCard(joueur)` (utilisés par la validation, la
projection et les générateurs de coups de test : une seule définition) et `Round::forcedAction()`, qui renvoie le seul
coup que le joueur courant a quand il n'a pas le choix (`DrawCard` sans carte jouable, `PlayCard` de la carte piochée si
elle est normale et jouable), et rien dans la règle officielle.

**L'application dit QUAND** : comme le minuteur de tour (ADR 0016), un timer `Timeouts::forcedAction` (700 ms) est armé
à chaque changement d'état quand `forcedAction()` existe ; il porte le `stateVersion` et applique le coup par
`Match::apply`, la même `PlayerAction` qu'un humain, donc les mêmes règles et événements. Le délai laisse voir la
situation (« je n'ai rien »). Le joueur peut aussi cliquer avant l'échéance : le timer devient périmé.

**Minuteur de tour** : à l'expiration, une pénalité est acceptée, une couleur tirée au hasard ; un joueur qui peut piocher
pioche (et passe, sauf si la règle l'oblige à jouer la carte piochée) ; un joueur qui doit jouer joue la première
carte jouable de sa main (forcément normale : sinon piocher aurait été permis).

**Vue** : `me.canDraw` et `me.canKeepDrawnCard` (qui remplace `canPass`) sont calculés par le serveur, comme
`playableCardIds` : le client ne recalcule aucune règle.

## Alternatives écartées
- **Que le moteur joue lui-même** (`apply` enchaîne la pioche automatique) : il faudrait une horloge ou des événements
  sans pause, et le cœur resterait ignorant du temps seulement en apparence.
- **Une seule règle** : la règle officielle reste utile pour les joueurs qui la préfèrent, et pour la simulation.
- **Refuser `DrawCard` hors des cas permis par un code d'erreur propre** : `ILLEGAL_MOVE` avec une raison suffit, comme
  les autres coups interdits.

## Conséquences
- La simulation massive joue les deux règles (une graine sur deux) et vérifie que tout `forcedAction()` est accepté.
- La simulation ne retire plus de joueur entre deux manches : l'invariant « les points de la manche = les mains
  restantes » ne tient plus après un départ, et ce n'est pas un défaut du moteur.
- `legalActionsOfCurrentPlayer` (tests) n'offre `DrawCard` / `Pass` que si le moteur les accepte.
