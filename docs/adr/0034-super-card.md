# 0034 — Le Joker +5 devient la « super carte » : aucune réplique, pioche automatique

- **Statut** : accepté
- **Date** : 2026-10-11
- **Modifie** : ADR 0028 (réplique par un autre +5, fenêtre « Accepter »), ADR 0029 (le +5 en haut d'une pile)

## Contexte
Dans l'ADR 0028, la cible d'un +5 pouvait répliquer par un autre +5 (le total montait de 5) ou accepter dans une
fenêtre. En partie réelle, cette fenêtre coupait le rythme et la chaîne de +5 rendait la carte trop volatile. On veut
une carte qui ne se contre jamais.

## Décision
- **Rien ne répond à un +5** : `canStackOn(_, WildDrawFive)` est faux, y compris pour un autre +5. Une carte posée sur
  un +5 en attente est refusée par `NotStackable` (`ILLEGAL_MOVE` / `CANNOT_STACK`). La raison
  `ONLY_PLUS_FIVE_PLAYABLE` n'a plus de sens : elle est supprimée du protocole.
- **Pioche automatique, rythmée** : quand la pile en attente se termine par un +5, `Round::forcedAction()` renvoie
  l'acceptation **quelle que soit la règle de pioche** et quelle que soit la main de la cible. Le serveur la joue après
  `actionsOpenAt` + `forcedAction`, cartes piochées une à une (ADR 0026, 0027). Un clic sur le paquet (`canDraw`) prend
  le total plus tôt ; il n'y a plus de fenêtre « Accepter » côté client.
- **Échelle** (ADR 0029) : un +5 se pose toujours sur un +2 ou un +4 en attente ; le total s'ajoute et va à la cible
  choisie. Il clôt la chaîne : la cible pioche tout et passe son tour. Le reste de l'échelle est inchangé.
- La phase `AwaitingStackResponse` est conservée : elle porte le total jusqu'à la pioche, ce qui garde intacts la
  cadence de présentation, les départs de joueur, l'UNO obligatoire et le minuteur de tour.

## Alternatives écartées
- **Résoudre le +5 dans `apply`** (comme le +2 officiel) : plus direct, mais la pioche rythmée du serveur (ADR 0026)
  et l'ouverture du tour (ADR 0027) reposent sur le coup forcé ; on aurait dupliqué ce mécanisme.
- **Garder la fenêtre pour la pioche officielle** : le but est justement qu'aucun choix ne reste à la cible.

## Conséquences
- Plus de composant d'invite « +5 contre toi » ni de scénario de démo associé.
- Tests : moteur, application, simulation (un coup forcé est aussi permis pour la cible d'un +5), E2E et page
  « Comment jouer » mis à jour.
