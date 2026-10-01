# Avancement

> Tenu à jour par Claude Code à la fin de chaque étape. Les cases cochées = build + tests + lint verts.
> Format d'une entrée de journal : `AAAA-MM-JJ — phase.étape — résumé — branche/PR`.

## Phase en cours : 3 — UI minimale (phases 0, 1 et 2 terminées)

> Cap : un **MVP jouable au plus tôt**. Ordre d'exécution ci-dessous (les numéros d'étape sont conservés pour la traçabilité). Les phases 3 et 4 sont d'abord faites en version minimale ; le polish (animations, galerie, responsive fin, accessibilité poussée, E2E) vient après le MVP.

### Phase 0 — Fondations et contrat
- [x] 0.1 Arborescence du monorepo, `.gitignore`, `.editorconfig`, README
- [x] 0.2 Serveur : CMake + presets Ninja + vcpkg manifest + projet vide qui compile + 1 test Catch2
- [x] 0.3 Vérifier que `compile_commands.json` est généré et que clangd le trouve
- [x] 0.4 Client : `ng new` Angular 22 (zoneless, SCSS, Vitest) + lint + 1 test
- [x] 0.5 Protocole v1 : schémas JSON + exemples de messages + types TS générés
- [x] 0.6 CI GitHub Actions (build + tests serveur et client)
- [x] 0.7 Compléter la table des commandes dans CLAUDE.md

### Chemin vers le MVP

#### Phase 1 — Cœur du jeu (C++, TDD)
- [x] 1.1 Cartes, deck de 108 cartes, `RandomSource`
- [x] 1.2 État de partie, tour, sens, pioche/défausse, remélange
- [x] 1.3a Jouabilité, effets (Skip/Reverse/DrawTwo/Wild), choix de couleur, première carte retournée
- [x] 1.3b Wild Draw Four : légalité stricte et contestation officielle
- [x] 1.4 UNO : annonce, contre-UNO, pénalités
- [x] 1.5 Fin de manche, score, fin de partie (500 pts ou manche unique)
- [x] 1.7 Projection `PlayerView` + test anti-fuite
- [x] 1.8 Simulation aléatoire massive (invariants)

#### Phase 2 — Serveur réseau
- [x] 2.1 Serveur uWebSockets, `/health`, vérification Origin, limites de taille
- [x] 2.2 Codec JSON + validation stricte + erreurs typées
- [x] 2.3 Sessions, salons, codes, hôte, paramètres
- [x] 2.4 Démarrage de partie, diffusion des vues et des événements (inclut la projection `DomainEvent` → `ClientEvent` et son test anti-fuite, reportés de 1.7 : ADR 0013)
- [x] 2.5 Timers (tour, reconnexion, salons inactifs), rate limiting
- [x] 2.6 Tests d'intégration (clients WebSocket de test)

#### Phase 3 — UI minimale (Angular)
- [ ] 3.1 Tokens, typographies, fond, surfaces « verre », glow (version minimale)
- [ ] 3.2 Composant carte (toutes les cartes, états, daltonisme)
- [ ] 3.3 Écrans Accueil et Salon
- [ ] 3.4 Table de jeu (main, adversaires, piles, indicateurs, sélecteur de couleur)

#### Phase 4 — Intégration client ↔ serveur (minimale)
- [ ] 4.1 `GameSocket` (reconnexion, backoff, messages typés)
- [ ] 4.2 `GameStore` branché sur le serveur, réconciliation par `stateVersion`
- [ ] 4.3 Parcours complets : créer, rejoindre, jouer, gagner → **première partie jouable (MVP)**

### Après le MVP

#### Phase 1 (suite)
- [ ] 1.6 Options maison (politiques injectables)

#### Phase 3 (suite) et 4 (suite)
- [ ] 3.5 Animations et `prefers-reduced-motion`
- [ ] 3.6 Galerie `/dev/gallery` avec tous les états
- [ ] 3.7 Responsive 360 → 1920 px, accessibilité clavier et lecteur d'écran
- [ ] 4.3b Revanche
- [ ] 4.4 E2E Playwright : partie complète à 2 puis 4 joueurs, reconnexion

#### Phase 5 — Bots, finitions, audits
- [ ] 5.1 Bots (stratégies Aléatoire et Glouton)
- [ ] 5.2 Sons, réactions rapides, écran de fin
- [ ] 5.3 Audit web-design-guidelines + axe + performance, corrections

#### Phase 6 — Déploiement
- [ ] 6.1 Dockerfile serveur multi-étapes, build client
- [ ] 6.2 Caddy (HTTPS, `/ws`, en-têtes de sécurité), docker-compose
- [ ] 6.3 Documentation de déploiement

## Décisions
- [0001 — Un seul dépôt pour le serveur, le client et le protocole](adr/0001-monorepo.md)
- [0002 — CMake presets et générateur Ninja](adr/0002-cmake-presets-ninja.md)
- [0003 — vcpkg en mode manifest avec baseline figée](adr/0003-vcpkg-manifest.md)
- [0004 — Sanitizers : ASan + UBSan sous Linux, ASan seul sous MSVC](adr/0004-sanitizers-per-platform.md)
- [0005 — JSON Schema 2020-12 comme source de vérité du protocole](adr/0005-json-schema-protocol.md)
- [0006 — `std::variant` pour les ensembles fermés, interfaces pour les dépendances injectées](adr/0006-variant-closed-sets.md)
- [0007 — Écarts du protocole v1 par rapport à la SPEC §8](adr/0007-protocol-v1-deviations.md)
- [0008 — Aléatoire déterministe et portable](adr/0008-portable-deterministic-randomness.md)
- [0009 — Modélisation des cartes : `std::optional<Color>` et `createStandardDeck`](adr/0009-card-model.md)
- [0010 — État de la manche : `Round` valeur pure, `RandomSource&` passé en paramètre](adr/0010-round-state.md)
- [0011 — Fenêtre UNO : définition précise](adr/0011-uno-window.md)
- [0012 — Fin de manche et `Match` : score, donneur, fin de partie](adr/0012-round-end-and-match.md)
- [0013 — Projection `PlayerView` : périmètre du cœur et test anti-fuite](adr/0013-player-view-projection.md)
- [0014 — Couche réseau : modèle de messages dans `app`, limites du transport, client de test sans dépendance](adr/0014-network-layer.md)
- [0015 — Projection des événements : `ClientEvent`, projetés par lot contre l'état d'après](adr/0015-event-projection.md)
- [0016 — Le temps et les départs : timers, retrait d'un joueur, limitation de débit, options de test](adr/0016-time-and-departures.md)

## Journal
- 2026-09-30 — 0.1 — arborescence du monorepo, `.editorconfig`, README, modèle d'ADR, BOM retiré de `.gitattributes` — `chore/phase-0-foundations`
- 2026-09-30 — 0.2 — CMake presets `dev`/`debug-asan`/`release` (Ninja), vcpkg manifest (5 dépendances, baseline figée), `uno_core`/`uno_app`/`uno_net`/`uno_server`, 6 tests Catch2, clang-format + clang-tidy propres — `chore/phase-0-foundations`
- 2026-09-30 — 0.3 — `.clangd` à la racine pointant vers `server/build/dev` ; `clangd --check` résout les en-têtes du projet et de vcpkg sans erreur — `chore/phase-0-foundations`
- 2026-09-30 — 0.4 — client Angular 22 (zoneless, SCSS, Vitest, `strict` + `strictTemplates`), angular-eslint (`no-explicit-any`, `no-non-null-assertion` en erreur), Prettier, proxy `/ws` → `localhost:9001`, shell `<router-outlet />` + 2 tests — `chore/phase-0-foundations`
- 2026-09-30 — 0.5 — protocole v1 : 7 schémas JSON Schema 2020-12, 39 exemples valides et 18 invalides, `npm run protocol:gen` (json-schema-to-typescript), test de contrat Vitest + ajv (63 tests verts), `@types/node` ajouté aux specs (validé), écarts SPEC tracés en ADR 0007 — `chore/phase-0-foundations`
- 2026-09-30 — 0.6 — CI GitHub Actions : `server-linux` (GCC 14 + Clang 20, `debug-asan`, clang-format/clang-tidy 23 via apt.llvm.org), `server-windows` (MSVC, `dev`), `client` ; actions épinglées par SHA, cache binaire vcpkg ; corrigés en route : runtime des sanitizers Clang (`libclang-rt-20-dev`), `getenv` justifié pour clang-tidy, chemin du cache sans `..` — `chore/phase-0-foundations` / PR #2
- 2026-09-30 — 0.7 — table des commandes de `CLAUDE.md` complétée et vérifiée (serveur, formatage/lint C++, client, protocole) ; `OnPush` par défaut depuis Angular 22 — `chore/phase-0-foundations` / PR #2
- 2026-09-30 — 1.1 — `Card`/`CardId`/`Color`/`Rank`, interface `RandomSource` + Fisher-Yates `shuffle`, `createStandardDeck` (108 cartes, identifiants permutés aléatoirement), `SeededRandomSource` dans la bibliothèque `uno_testing` (jamais liée au binaire), 20 nouveaux tests dont la valeur de référence de `std::mt19937_64` ([rand.predef]) ; ADR 0008 (déterminisme portable) et 0009 (modèle de carte), SPEC §2/§7.3 alignées ; commande clang-tidy de `CLAUDE.md` alignée sur le CI — `feat/core-deck`
- 2026-09-30 — 1.2 — `PlayerId`, `DomainError`, `TurnOrder` (sièges, joueur courant, sens), `DrawPile`/`DiscardPile` + `drawCards` (remélange de la défausse sauf la carte du dessus, pioche partielle sans erreur), `Round::start` (fabrique qui valide : distribution une carte à la fois à partir de la gauche du donneur, première carte retournée, +4 remis dans la pioche remélangée) ; `Round` valeur pure, `RandomSource&` en paramètre (ADR 0010, ligne §7.2 de la SPEC corrigée) ; fixtures de test partagées dans `uno_testing` ; 41 nouveaux cas de test (67 au total) verts en `dev` et `debug-asan`, clang-tidy propre — `feat/core-round-state`
- 2026-09-30 — 1.3a — `isPlayable` (jouabilité de base, +4 volontairement non spécial-casé, reporté en 1.3b) ; premier usage de `std::variant`+`std::visit` du projet (ADR 0006) : `PlayerAction`, `TurnPhase`, `DomainEvent` (alignés sur SPEC §8.5 : `DeckReshuffled`/`TurnChanged` sont des événements, pas des booléens) et le visiteur `Overloaded` ; `Round::apply` (tour/main/couleur validés, cycle piocher→décider→passer avec fin de tour automatique si la carte piochée n'est pas jouable ou si plus rien n'est piochable, effets Skip/Reverse — y compris « Reverse agit comme Skip » à 2 joueurs —/DrawTwo/Wild) ; `Round::start` renvoie désormais un `RoundStart` (manche + événements, dont `RoundStarted`) et résout l'effet de la première carte retournée ; `requireRoundInvariants` dans `uno_testing` (désormais lié à Catch2), réutilisable par la simulation de l'étape 1.8 ; 36 nouveaux cas de test (103 au total) verts en `dev` et `debug-asan`, clang-tidy propre — `feat/core-rules-basic`
- 2026-10-01 — 1.3b — Joker +4 et contestation officielle (`wildDrawFourMode: officialChallenge`, seul mode de cette étape — `strict` et le cumul restent en 1.6) : `isPlayable` traite désormais le +4 comme le Wild (toujours tentable, y compris en bluff) ; `isWildDrawFourLegal` (nouvelle fonction pure) juge sa légalité à la pose, à partir d'une `Color` définie plutôt qu'un `optional<Color>` (pour ne jamais confondre deux couleurs absentes) ; `PlayerAction::RespondPenalty`, `TurnPhase::AwaitingPenaltyResponse` et `DomainEvent::ChallengeResolved` ajoutés sous les noms déjà prévus par la SPEC §7.3 (aucune ADR nécessaire) ; poser un +4 déplace immédiatement le tour vers le joueur visé (contrairement au +2, sans fenêtre de réponse) et fige dans la phase l'identité du poseur et la légalité calculée, jamais recalculée ; `ChallengeResolved` porte son propre `penaltyAmount` (4 ou 6), distinct des cartes réellement piochées par le `PenaltyCardsDrawn` qui suit (piles proches de l'épuisement, SPEC §5) ; nouvel invariant (le poseur d'un +4 en attente de réponse n'est jamais le joueur courant) ; 9 nouveaux cas de test dédiés + 3 sur `isWildDrawFourLegal` (115 au total) verts en `dev` et `debug-asan`, clang-tidy propre — `feat/core-wild-draw-four`
- 2026-10-01 — chore — passage en workflow rapide : mode autonome, un commit par étape et une PR par lot de 2-3 étapes, une seule revue par phase, `linux-check.sh` silencieux (log dans `~/uno-neon-linux/linux-check.log`), PROGRESS réordonné vers un MVP jouable (1.6 reportée après le MVP) — `chore/fast-workflow`
- 2026-10-01 — 1.3b (correctif) — commentaire de `AwaitingPenaltyResponse` : la légalité du +4 est figée à la pose parce qu'elle se juge contre la couleur courante d'*avant* le choix de couleur, pas par économie ; test « aucune carte rouge, une bleue, choisit bleu : légal » — `feat/core-uno-and-round-end`
- 2026-10-01 — 1.4 — `CallUno`/`CatchUno`, `UnoCalled`/`UnoCaught`, fenêtre UNO (`Round::unoWindow`, `hasCalledUno`) définie dans l'ADR 0011 (ouverture à 1 carte non annoncée, fermeture au prochain `PlayCard`/`DrawCard` accepté, contre-UNO ou annonce ; une annonce non due est sans effet), `giveCards` centralise les ajouts en main, 17 nouveaux cas de test (cas limites : contre-UNO simultané, action refusée, Skip à 2 joueurs, +4 en attente, annonce perdue en piochant ou en passant) et deux invariants — `feat/core-uno-and-round-end`
- 2026-10-01 — 1.5 — `RoundOver` et `RoundEnded` (dernière carte : un +2 ou +4 final fait quand même piocher le suivant, sans contestation, et ces cartes comptent), `scoring.hpp` (`cardPoints`, `handPoints`), classe `Match` (scores par siège, donneur tournant, `targetScore` ou manche unique, `MatchEnded`, `startNextRound`) — ADR 0012 ; `drawPenalty` remplace six copies de la pioche de pénalité ; `legalActionsOfCurrentPlayer` dans `uno_testing` (réutilisée par la simulation 1.8) ; invariants « main vide seulement chez le gagnant » et « points = valeur des mains adverses » ; 18 nouveaux cas de test (151 au total) verts en `dev`, clang-tidy et clang-format propres — `feat/core-uno-and-round-end`
- 2026-10-01 — 1.5 (alignement) — `MatchSettings::matchLength` (`SingleRound`/`To250`/`To500`, `targetPoints()`) remplace `targetScore`, comme le prévoit l'ADR 0007 n°3 ; tests de fin de partie rejoués jusqu'à ce qu'un joueur atteigne 250 points — `feat/core-projection-and-simulation`
- 2026-10-01 — 1.7 — `PlayerView` (partie « jeu » du schéma du protocole : main du joueur, `playableCardIds`, `canDraw`/`canPass`/`canCallUno`/`canChooseColor`, `penaltyResponse`, `catchableTargetIds`, joueurs par siège avec nombre de cartes, `roundResult` avec mains révélées en fin de manche) et `project(Round, viewer, MatchProgress)` / `project(Match, viewer)` ; `Match` regroupe son état dans `MatchProgress` ; `Round::canCallUno` ; test anti-fuite à trois niveaux (non-interférence : mains adverses, ordre de la pioche et légalité d'un +4 ne changent pas la vue ; inventaire des identifiants de cartes ; parties aléatoires) via `requireViewLeaksNothing` et `deckFromHands` dans `uno_testing` ; la projection des événements est reportée en 2.4 (ADR 0013) ; 16 nouveaux cas de test (167 au total) verts en `dev`, clang-tidy et clang-format propres — `feat/core-projection-and-simulation`
- 2026-10-01 — 1.8 — simulation massive (`tests/simulation_test.cpp`) : des parties entières à 2-10 joueurs (manche unique, 250, 500), actions légales aléatoires mêlées à 15 % d'actions hors tour ou arbitraires (contre-UNO, annonce, cartes inconnues…) ; après chaque action, soit elle est refusée et l'état est strictement inchangé, soit elle est acceptée et `requireRoundInvariants`, la conservation des 108 cartes et `requireViewLeaksNothing` tiennent ; scores = somme des points de manche ; même graine = même partie. Nombre de parties réglé par `UNO_SIMULATION_GAMES` (défaut 50, 500 dans le CI Linux sous sanitizers) ; **10 000 parties vertes en `release` (555 millions d'assertions, environ 2 minutes)** ; invariants du cœur condensés (une assertion par table plutôt que par siège) — `feat/core-projection-and-simulation`
- 2026-10-01 — chore — CI des PR et `linux-check.sh` : 100 parties simulées au lieu de 500 ; nouveau workflow `simulation.yml` (manuel + chaque lundi) : 10 000 parties en release — `chore/ci-simulation` / PR #13
- 2026-10-01 — 2.2 — modèle de messages dans `uno_app` (`request::Envelope` à 19 corps, `response::Message`, `RoomSettings` et son patch, `ErrorCode`), codec de `uno_net` : décodage strict sans exception (version → type → forme, `replyTo` repris quand l'`id` est lisible, entiers stricts, pseudo borné en points de code), encodage client et serveur, tables de noms de fil partagées ; test de contrat sur `protocol/examples` (39 valides client/ack/error rejoués à l'identique, 18 invalides rejetés avec le bon code), 11 tests de cas hostiles ; `CryptoRandomSource` (libsodium) pour le mélange, les jetons et les codes ; ADR 0014 — `feat/net-server-and-codec`
- 2026-10-01 — 2.1 — `WebSocketServer` uWebSockets mono-thread (`/ws`, `GET /health`, 404 ailleurs), `OriginPolicy` (liste exacte, `Origin` absent refusé), messages > 4 Kio : 1009, > 8 Kio coupés par le transport, binaire : 1003, 10 messages mal formés : 1008, `UNO_ALLOWED_ORIGINS`/`UNO_LOG_LEVEL`, arrêt propre sur SIGINT/SIGTERM, port exclusif ; client HTTP/WebSocket de test sur sockets bruts (aucune dépendance vcpkg, ADR 0014) et 12 tests d'intégration sur un vrai serveur ; un test a révélé qu'un entier non signé sous le minimum passait la validation (corrigé) — `feat/net-server-and-codec`
- 2026-10-01 — 2.3 — ports de `uno_app` (`ConnectionHandler` déplacé depuis `uno_net`, `MessageSink`, `Clock`) et `Application` : sessions (jeton de 132 bits, reprise, la connexion la plus récente gagne, ancienne fermée en 4000), salons (code non ambigu, hôte, prêt, exclusion, paramètres, départ avec passage de l'hôte au joueur connecté suivant, `RoomRepository` en mémoire), pseudos (UTF-8 strict, 2 à 16 caractères, collision insensible à la casse), réactions limitées à 1 par 2 s ; l'`ack` part toujours avant les messages qu'il provoque, une requête refusée ne notifie personne ; codec de `session.welcome`, `room.update`, `reaction`, `room.closed` (les 4 exemples serveur rejoués), `SystemClock`, `ServerMessageSink`, câblage de `main.cpp`. Décisions : l'hôte compte comme prêt (lancer la partie est sa façon de l'être) ; les options maison sont refusées en `INVALID_SETTINGS` tant que 1.6 n'existe pas ; quitter un salon pendant une partie est refusé (`MATCH_IN_PROGRESS`) jusqu'à 2.5, qui saura retirer un joueur d'une manche — `feat/rooms-and-game-broadcast`
- 2026-10-01 — 2.4 — démarrage de partie (hôte, ≥ 2 joueurs, tous prêts), actions → `Match::apply` avec traduction des `DomainError` en codes du protocole, diffusion à chaque joueur d'un `game.update` (événements projetés pour lui + sa vue, `stateVersion` consécutif), manche suivante quand tous les connectés sont prêts, fin de partie → salon `matchOver`, revanche (mêmes joueurs connectés, scores à zéro, `stateVersion` poursuivi), déconnexion/reconnexion annoncées et reprise par une vue complète ; cœur : `ClientEvent` (19 sortes) et `project(span<DomainEvent>, viewer, roundAfter, round)` — `ChallengeResolved.revealedHand` pour le seul contestataire, cartes de `CardsDrawn`/`PenaltyCardsDrawn` pour le seul joueur qui pioche (ADR 0015) ; test anti-fuite des événements dans la simulation massive, tests ciblés, et test de manches entières par requêtes qui vérifie le JSON sérialisé de chaque `game.update` ; codec complet des vues et événements (les 10 exemples `server.game.update.*` rejoués : tous les exemples du protocole sont désormais couverts) — `feat/rooms-and-game-broadcast`
- 2026-10-01 — chore — `linux-check.sh` borne son parallélisme (`CMAKE_BUILD_PARALLEL_LEVEL`, `VCPKG_MAX_CONCURRENCY` = min(cœurs, mémoire disponible / 2 Go), au moins 1) : Ninja lançait un job par cœur, la compilation ASan saturait la mémoire — `feat/rooms-and-game-broadcast`
- 2026-10-01 — 2.5 — retrait d'un joueur dans le moteur (`TurnOrder::remove`, `Round::removePlayer`, `Match::removePlayer` : cartes sous la pioche, tour/pénalité/couleur gérés, forfait à deux joueurs ; la simulation retire des joueurs au hasard) ; port `Scheduler` (`UwsScheduler`, `ManualScheduler`) et `Timeouts` ; délai de grâce de 60 s (lobby et partie), expiration des salons (15 min en lobby, 5 min après la partie) et des sessions (10 min), minuteur de tour avec action automatique par `Match::apply` (pénalité acceptée, couleur au hasard, piocher puis passer ; la fenêtre UNO se ferme), manche suivante d'office après 30 s, `turnDeadline`/`nextRoundDeadline` dans les vues, quitter/exclure/expirer par un seul `removeFromRoom` (hôte transmis, `hostChanged`) ; limites de débit dans `uno_net` (seau par connexion, fenêtres par adresse pour création et jonction de salon, `UNO_TRUSTED_PROXY`) ; `UNO_ENABLE_TEST_HOOKS`/`UNO_TEST_SEED` à la compilation seulement, avec vérification en CI ; ADR 0016 — `feat/timers-and-integration`
- 2026-10-01 — 2.6 — tests d'intégration sur un vrai serveur (câblage de production : aléatoire cryptographique, horloge système, timers uWebSockets) : parties complètes à 2 et à 3 joueurs jouées par sockets, chaque `game.update` reçu vérifié contre les fuites de cartes ; reprise de session avec fermeture de l'ancienne connexion (4000) ; déconnexion montrée au salon ; `SESSION_REQUIRED` ; `/health` ; limites de débit (par connexion, création, jonction) ; **la phase 2 est terminée** — `feat/timers-and-integration`
