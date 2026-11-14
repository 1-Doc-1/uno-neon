# 0040 — L'interception (`jumpIn`) est retirée du produit

- **Statut** : accepté
- **Date** : 2026-11-14
- **Écart par rapport à la SPEC** : §4 (option `jumpIn`, interactions), §7 (`JumpInPolicy`), §8.3 (`game.playCard`, « interception si `jumpIn` »). La SPEC n'est pas modifiée (règle du dépôt) : cette ADR en tient lieu.
- **Concerne** : ADR 0017, 0018, 0020, 0027, 0029

## Contexte
L'étape 1.6 prévoyait trois options maison jamais implémentées : `jumpIn`, `sevenZero` et `wildDrawFourMode: strict`.
Leur réglage existait dans le protocole et le salon, mais le serveur les refusait (`INVALID_SETTINGS`) faute de moteur.
Depuis, le jeu a pris un autre chemin que la SPEC d'origine, et `jumpIn` ne s'y accorde plus.

`jumpIn` est une **course** : n'importe quel joueur pose, hors de son tour, une carte strictement identique à celle du
dessus, et le premier message reçu gagne. Cela suppose un instant où tout le monde peut réagir immédiatement.

## Décision
**L'option est retirée**, du protocole (`RoomSettings.jumpIn`, `CardPlayedEvent.isJumpIn`, raison `JUMP_IN_TOO_LATE`), du
serveur et du client. Les deux autres options sont implémentées (ADR 0041).

Pourquoi elle n'a plus de sens avec les règles actuelles :
1. **Il n'y a plus d'instant.** Depuis l'ADR 0027, personne ne peut agir avant `actionsOpenAt`, au moins 1,5 s après
   chaque action : la table a besoin de ce temps pour montrer ce qui vient de se passer. L'interception ne peut donc
   plus être un réflexe. Elle se joue au moment où le tour s'ouvre, en concurrence directe avec le joueur légitime :
   ce qui décide alors est la latence réseau, pas l'attention.
2. **Le moteur et l'application supposent un seul acteur : le joueur courant.** L'horloge du tour (ADR 0020), le coup
   forcé de la pioche guidée (ADR 0017), les fenêtres de contre-UNO (ADR 0018), les pénalités en attente (ADR 0029) et
   la réflexion des bots sont tous accrochés à lui. Une interception déplace le tour en pleine résolution et invalide
   les minuteurs armés et le budget de présentation.
3. **La SPEC laisse des cas sans réponse** : interception pendant une pénalité sans cumul, sur la cible d'un +5, ou
   pendant une fenêtre UNO ouverte. Les trancher, c'est inventer des règles pour une option que personne n'a demandée.

## Alternatives écartées
- **L'implémenter telle quelle** : elle contredirait l'ADR 0027 ou la viderait de son sens (1).
- **Laisser le réglage, refusé par le serveur** : du code et une interface qui promettent une option absente.
- **Une version « lente »** (interception seulement pendant une fenêtre dédiée) : c'est une autre règle, à concevoir
  avec ses propres tests, pas un reste de celle-ci.

## Conséquences
- Le protocole perd un champ obligatoire de `RoomSettings` et un champ de `CardPlayedEvent` : tous les exemples et
  types sont mis à jour.
- Si l'interception revient un jour, elle repartira d'une règle écrite pour le rythme actuel (fenêtre explicite après
  `actionsOpenAt`, gestion des minuteurs), pas de cette ancienne ligne de la SPEC.
