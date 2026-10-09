# 0028 — Carte +5 (`WildDrawFive`) et composition réglable du paquet

- **Statut** : accepté
- **Date** : 2026-10-09
- **Complète** : ADR 0006 (ensembles fermés), ADR 0017 (pioche guidée), ADR 0019 (UNO obligatoire), ADR 0027 (résolution d'un effet)

## Contexte
On veut une carte rare, « joker doré », qui désigne une cible et qui peut se répliquer : le +5. Elle se joue sur
n'importe quelle carte comme un Joker, le poseur choisit la couleur **et** un adversaire, et la cible ne peut
répliquer qu'avec un autre +5 (les pénalités s'additionnent). L'hôte règle aussi le nombre de +2, de +4 et de +5 du
paquet. Le moteur n'avait ni cible dans `PlayCard`, ni pénalité qui se cumule, ni paquet réglable.

## Décision
- **Une carte, un rang** : `Rank::WildDrawFive` (nom de fil `wildDrawFive`), sans couleur propre, **toujours jouable**,
  donc comptée par `isWild` : couleur requise, 50 points en fin de manche, « spéciale » pour la pioche guidée. Il y en
  a 2 par paquet standard (paquet de **110** cartes). Une carte retournée en début de manche qui est un +5 retourne
  dans la pioche, comme le +4.
- **La cible est dans l'intention** : `PlayCard::target` est obligatoire pour un +5 et interdit sinon. Le moteur la
  valide (joueur de la manche, différent du poseur) : `TargetRequired`, `TargetNotAllowed`, `InvalidTarget`. Sur le fil,
  `game.playCard.payload.targetId`.
- **Une phase qui cumule** : `AwaitingPlusFiveResponse{total}`. Poser un +5 déplace le tour **tout de suite** chez la
  cible (comme le +4 ; elle peut ne pas être le joueur suivant) et annonce `PlusFiveTargeted{poseur, cible, total}`.
  La cible a deux choix : **accepter** (`RespondPenalty::Accept` : elle pioche `total`, est sautée, le jeu reprend au
  joueur qui la suit dans le sens du jeu) ou **répliquer** en posant un +5 (`PlayCard`, autre couleur, autre cible,
  éventuellement le poseur précédent) : le total augmente de 5. Tout autre `PlayCard` est refusé
  (`OnlyPlusFivePlayable`), `RespondPenalty::Challenge` aussi (`CannotChallenge` : un +5 ne se conteste pas).
- **Pioche guidée** : `forcedAction()` renvoie l'acceptation quand la cible n'a aucun +5 (le serveur la joue comme les
  autres coups forcés). Avec un +5 en main, la cible choisit. Règle `official` : elle choisit toujours.
- **Interactions** (toutes testées) :
  - **Dernière carte** : un +5 posé en dernière carte (ou en réplique) termine la manche ; la cible pioche le total
    (5 si c'était un +5 simple), ces cartes comptent dans le score, comme pour un dernier +2 ou +4.
  - **UNO** : la fenêtre de contre-UNO s'ouvre sur le poseur resté à une carte, comme après toute pose ; la cible qui
    reçoit des cartes voit sa fenêtre fermée (`giveCards`). `CallUno` est accepté avec deux cartes avant de répliquer.
  - **UNO obligatoire** (ADR 0019) : répliquer avec sa dernière carte, un +5, sans avoir annoncé UNO est refusé
    (`MUST_DECLARE_UNO`), et `mustDeclareUno` le dit à la cible.
  - **Pioche épuisée** : la pénalité passe par `drawPenalty` : remélange de la défausse (sauf la carte du dessus), puis
    on pioche ce qui reste, sans erreur.
  - **Départ d'un joueur** : si la cible part, la chaîne est annulée et le jeu reprend chez le joueur suivant ; si c'est
    un autre joueur qui part, la chaîne continue.
  - **Minuteur de tour** : la cible qui laisse expirer son tour accepte (comme pour un +4).
- **Information** : tout le monde voit qui a posé, qui est visé et le total (`PlusFiveTargeted`, `pendingDraw`) ;
  seule la cible voit les cartes qu'elle pioche, comme pour toute pioche. La vue de la cible porte
  `penaltyResponse{amount: total, canChallenge: false, canStack: possède un +5}` et ses `playableCardIds` sont ses +5.
- **Composition du paquet** : `DeckSettings{drawTwo, wildDrawFour, wildDrawFive}`, chacun un `CardMultiplier`
  (×1, ×2, ×3 ou ×5 ; les autres valeurs sont refusées par le schéma). `createDeck(DeckSettings, RandomSource&)` est le
  seul endroit qui calcule la composition (fonction pure, testée : total et répartition par couleur) ;
  `createStandardDeck` en est le cas ×1. Réglages de salon `drawTwoMultiplier`, `wildDrawFourMultiplier`,
  `wildDrawFiveMultiplier`, modifiables par l'hôte avant la partie.
- **Rythme** : la pose d'un +5 coûte la carte, la roue des couleurs et l'effet « +5 » (`playStep` + 2 × `effectStep`) ;
  les cartes de la pénalité, `drawStep` chacune (ADR 0026, 0027).

## Alternatives écartées
- **Réutiliser `AwaitingPenaltyResponse`** pour le +5 : le +4 y fige l'identité du poseur et la légalité, le +5 un
  total ; une seule phase aurait mélangé deux jeux de champs et rendu des états impossibles représentables.
- **Réutiliser `swapTargetId`** (échange de main du 7) pour la cible : même nom, deux sens ; un champ par sens.
- **Cibler seulement le joueur suivant** : pas de choix, pas de jeu. La cible choisie est tout l'intérêt de la carte.
- **Une carte +5 par couleur** : un joker doré rare vaut mieux que huit cartes banales.

## Conséquences
- Le paquet standard passe de 108 à 110 cartes : `kStandardDeckSize`, les tests de conservation et les graines E2E sont
  mis à jour (les graines changent, le paquet n'est plus le même).
- La carte porte un dessin et une couleur de glow propres côté client (tokens `--gold-*`, jamais de valeur en dur).
- Le choix de la cible est une fenêtre **non modale** (les sièges restent cliquables) : `<dialog>.show()` plutôt que `showModal()`.
- Les E2E ont une option de salon `×5` pour le Joker +5 et deux graines (`plusFiveSimple`, `plusFiveAnswered`) ; la politique des tests vise le premier adversaire.
