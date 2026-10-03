# 0019 — Règle maison « UNO obligatoire pour gagner »

- **Statut** : accepté
- **Date** : 2026-10-03

## Contexte
Dans la règle officielle, oublier d'annoncer UNO coûte 2 cartes si un adversaire contre à temps (ADR 0018), et une
annonce tardive ne change rien à la victoire. Certains groupes veulent une règle plus nette : on ne gagne pas sans avoir
dit UNO.

## Décision
Réglage de salon `declareUnoToWin` (booléen, **faux par défaut**, comme les autres règles maison).

- **Moteur** : avec la règle, `PlayCard` de la dernière carte est refusé par `DomainError::MustDeclareUno`
  (`ILLEGAL_MOVE` / `MUST_DECLARE_UNO`) tant que le joueur n'a pas annoncé UNO. Le contrôle vient **après** ceux de la
  carte (couleur, jouabilité) : une dernière carte injouable reçoit son propre refus. L'annonce reste celle de
  l'ADR 0018 : possible à 2 cartes pendant son tour (elle suit alors le joueur jusqu'à sa dernière carte) ou à 1 carte.
- **Vue** : `me.mustDeclareUno`, calculé par `Round::mustDeclareUno(joueur)` : règle active, tour du joueur, une seule
  carte en main, **jouable**, pas d'annonce. Le client ne recalcule rien ; il met en avant le bouton UNO.
- **Pioche guidée (ADR 0017)** : une carte jouable bloquée par l'annonce manquante est **toujours** une carte jouable pour
  `canDraw` et `forcedAction()`. Donc `forcedAction()` ne renvoie jamais `DrawCard` dans ce cas (une assertion de la
  simulation le vérifie), et piocher reste refusé si la carte est normale. Si la carte est spéciale (+2, Joker, +4),
  le joueur garde le choix de piocher, comme sans la règle.
- **Minuteur de tour** : un joueur bloqué qui laisse passer son tour, en pioche guidée avec une carte normale, voit le
  serveur annoncer UNO à sa place puis poser la carte (le coup que les règles lui imposaient). Avec une carte spéciale
  ou la pioche officielle, c'est le minuteur habituel (piocher, passer).
- **Tests** : les générateurs de coups légaux proposent `CallUno` (et non la carte refusée) quand le joueur est bloqué ;
  la simulation joue la règle sur une partie sur deux, indépendamment de la règle de pioche.

## Alternatives écartées
- **Annoncer automatiquement** : retire toute la tension de la règle.
- **Pénaliser (piocher) au lieu de refuser** : le refus est lisible (le bouton UNO s'allume) et ne coûte rien à
  un joueur qui a simplement oublié.
- **Compter la carte comme « non jouable » quand elle est bloquée** : le joueur piocherait automatiquement en pioche
  guidée, ce qu'on veut précisément éviter.
