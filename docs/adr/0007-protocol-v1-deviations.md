# 0007 — Écarts du protocole v1 par rapport à la SPEC §8

- **Statut** : accepté
- **Date** : 2026-09-30

## Contexte
En écrivant les schémas du protocole v1, la relecture de la SPEC a révélé des manques et une contradiction. Chaque
point ci-dessous a été arbitré par l'équipe avant d'écrire le contrat. La SPEC n'est pas modifiée : cette ADR fait
foi pour ces points.

## Décisions

| # | Sujet | SPEC | Protocole v1 | Pourquoi |
|---|---|---|---|---|
| 1 | Joker retourné en première carte | « le premier joueur choisit la couleur » (§3), mais aucun moyen de le faire sans jouer (§7.3) ; §4 évoque un « choix de couleur en attente » | phase `awaitingColorChoice`, message `game.chooseColor { color }` réservé à ce cas, `currentColor: Color \| null`, `me.canChooseColor` | Règle officielle respectée, contradiction levée, état explicite dans le contrat |
| 2 | Passage à la manche suivante | non spécifié (§12.3 prévoit un bouton « Manche suivante ») | message `match.readyForNextRound` ; la manche démarre quand tous les joueurs **connectés** l'ont envoyé, ou à `nextRoundDeadline` (30 s, porté de 15 à 30 s à l'étape 2.5 ; le bouton « Prêt » permet de démarrer plus tôt) ; `players[].isReadyForNextRound` | Le bouton de l'UI a un effet, et un joueur absent ne bloque jamais la table |
| 3 | Durée de partie | `scoreTarget: 250 \| 500 \| "singleRound"` | `matchLength: "singleRound" \| "to250" \| "to500"` (C++ : `enum class MatchLength` + `targetPoints()`) | Pas d'union nombre/chaîne : un seul type énuméré des deux côtés |
| 4 | `RoomView`, `RoomSettings` | utilisés mais non décrits | définis dans `room-view.schema.json` et `common.schema.json` | Manque de la SPEC |
| 5 | Main révélée lors d'une contestation du +4 | « seul le contestataire voit la main » | champ `revealedHand` de l'événement `challengeResolved`, présent seulement dans la projection du contestataire | Même principe que `cardsDrawn.cards` : la projection filtre |
| 6 | `MESSAGE_TOO_LARGE` | code d'erreur | code conservé, mais transmis comme raison de fermeture WebSocket (1009) | uWebSockets ferme la connexion avant qu'on puisse répondre |
| 7 | Codes d'erreur manquants | — | `ALREADY_IN_ROOM`, `NOT_IN_ROOM`, `INVALID_SETTINGS`, `CANNOT_KICK_SELF` ; raisons `COLOR_NOT_ALLOWED`, `SWAP_TARGET_INVALID`, `ONLY_DRAWN_CARD_PLAYABLE`, `CANNOT_CHALLENGE` | Conditions de §8.3 sans code associé |
| 8 | `stateVersion` | dans `game.update` **et** dans `view` | uniquement dans `view` | Une seule source (DRY), pas d'incohérence possible |
| 9 | Horloge | `turnDeadline` en heure serveur | + `serverTime` dans chaque `game.update` | Le client corrige le décalage de son horloge pour afficher le bon compte à rebours |
| 10 | Début de manche | pas d'événement | événement `roundStarted { round, dealerId }` | Le client anime la distribution, notamment en enchaînement automatique |
| 11 | Vainqueur de la partie | seulement dans l'événement `matchEnded` | + `matchWinnerId` dans la vue | Un joueur qui se reconnecte en fin de partie doit le voir sans avoir reçu l'événement |
| 12 | Payload vide | `{}` | objet sans aucune propriété autorisée (`Record<string, never>` en TS) | Un champ inattendu est une erreur, comme ailleurs |

## Conséquences
- Le moteur (phase 1) doit gérer `AwaitingColorChoice`, le délai de 30 s entre deux manches (15 s à l'origine, voir la ligne 2) et `MatchLength`.
- Les écrans (phase 3) disposent de `canChooseColor`, `isReadyForNextRound` et `nextRoundDeadline`.
- Toute autre évolution du protocole passe par une nouvelle ADR et une nouvelle version d'exemple.
