# 0015 — Projection des événements : `ClientEvent`, projetés par lot contre l'état d'après

- **Statut** : accepté
- **Date** : 2026-10-01

## Contexte
L'ADR 0013 a reporté à 2.4 la projection `DomainEvent → ClientEvent`. Trois événements du domaine contiennent des
cartes que tous les joueurs ne doivent pas voir : `CardsDrawn` et `PenaltyCardsDrawn` (les cartes piochées, que seul
celui qui pioche connaît) et `ChallengeResolved` (la main du poseur d'un +4, réservée à celui qui conteste, ADR 0007).
Diffuser un `DomainEvent` brut aurait fuité des cartes. Par ailleurs, le schéma `client-event` demande des champs que
les événements du domaine n'ont pas (la carte entière d'un `cardPlayed`, la direction d'un `directionChanged`, le
nombre de cartes d'un `deckReshuffled`).

## Décision
- **`ClientEvent` est dans `uno_core`** (`client_event.hpp`) : un `std::variant` des 19 sortes du schéma, avec un champ
  optionnel pour tout ce qu'un seul joueur peut connaître (`CardsDrawnEvent::cards`,
  `ChallengeResolvedEvent::revealedHand`). Les événements de connexion (`playerDisconnected`, `playerReconnected`,
  `hostChanged`) et ceux des options maison (`penaltyStacked`, `handsSwapped`, `handsRotated`) y sont aussi, pour que
  le codec lise tous les exemples du protocole ; le moteur ne produit pas encore les seconds (étape 1.6), et la
  couche `app` produit les premiers.
- **La projection se fait par lot, contre l'état d'après** : `project(span<DomainEvent>, viewer, roundAfter,
  roundNumber)`. Une action pose au plus une carte et inverse au plus une fois le sens : la carte posée est donc le
  dessus de la défausse, la couleur choisie la couleur courante, la direction celle de la manche, et les cartes
  piochées sont retrouvées dans la main de celui qui a pioché, après l'action. C'est un écart à la signature de la
  SPEC §7.2 (un événement, un joueur), choisi pour ne pas enrichir les événements du domaine : cela aurait réécrit
  une vingtaine de tests du moteur pour un format que seule la sérialisation fixe.
- **Pas de projection pour `TurnPassed`** : le `TurnChanged` qui suit dit déjà qui joue.
- **Test anti-fuite des événements** (invariant 2) : `requireEventsLeakNothing` (`uno_testing`) vérifie, pour un
  joueur donné, qu'aucune carte d'un événement n'est hors de sa main, du dessus de la défausse ou d'une main qu'une
  contestation lui révèle, et que les champs optionnels sont remplis pour lui seul. Il est appelé après chaque action
  de la simulation massive, et des tests ciblés couvrent la contestation et les piochées pour chaque joueur. Au
  niveau de l'application, un test joue des manches entières par requêtes et vérifie le **JSON sérialisé** de chaque
  `game.update` : toute carte qui y apparaît (objet carte ou `playableCardIds`) est dans la main du destinataire, sur
  le dessus de la défausse ou parmi les mains révélées d'une manche finie.

## Conséquences
- Un nouvel événement du domaine oblige à compléter le visiteur (exhaustivité vérifiée à la compilation) : on ne peut
  pas oublier de décider ce que chaque joueur en voit.
- La projection suppose d'être appelée juste après l'action qui a produit les événements ; `Application` le fait
  toujours dans la même fonction.

## Alternatives écartées
- **Enrichir `CardPlayed`, `DirectionReversed`, `DeckReshuffled`** : événements autonomes, mais réécriture massive des
  tests du moteur pour peu de bénéfice.
- **Une fonction par événement et par joueur** (signature de la SPEC) : impossible sans l'état d'après, ou sans
  enrichir les événements (voir ci-dessus).
