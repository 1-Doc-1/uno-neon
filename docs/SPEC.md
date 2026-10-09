# UNO Néon — Spécification complète

> **Pour Claude Code** : ce document est la source de vérité du projet. Il décrit le QUOI et le POURQUOI de chaque choix, et fixe le COMMENT quand il est structurant. Travaille phase par phase (§19), une étape à la fois, en suivant la méthode de `CLAUDE.md`. Si quelque chose ici te semble faux, risqué ou sous-optimal, **dis-le et propose une alternative argumentée** avant de coder : ce document peut évoluer via une ADR.

## Sommaire
1. Vision et périmètre
2. Glossaire
3. Règles du jeu (officielles)
4. Options maison
5. Cas limites de jeu
6. Architecture globale
7. Cœur du jeu (C++) et design patterns
8. Protocole client ↔ serveur
9. Serveur réseau (C++)
10. Client Angular
11. Design system
12. Écrans et parcours
13. Animations et son
14. Accessibilité
15. Sécurité
16. Performance
17. Tests et qualité
18. CI/CD et déploiement
19. Phases de réalisation et définition du « terminé »
20. Hors périmètre v1

---

## 1. Vision et périmètre

Un UNO en ligne jouable **dans le navigateur, sans installation, sans compte** : on choisit un pseudo, on crée un salon, on partage un lien ou un code de 6 caractères, on joue de 2 à 10 joueurs.

Objectifs de qualité, par ordre de priorité :
1. **Juste et inviolable** : le serveur décide de tout, aucune triche possible via le navigateur.
2. **Magnifique** : direction artistique « Nuit », l’esprit du jeu de cartes classique en plus sombre : quatre couleurs franches, une table de jeu, de gros chiffres lisibles, du néon seulement sur les cartes et quelques actions (§11).
3. **Agréable** : animations fluides, retours clairs à chaque action, jouable au clavier et au tactile.
4. **Exemplaire techniquement** : architecture en couches, patterns justifiés, tests automatisés, CI. Des étudiants doivent pouvoir lire le code et comprendre les choix.

**Marque** : « UNO » est une marque de Mattel. Le design des cartes, le logo et les icônes doivent être **originaux** (pas de reproduction de l'ovale, du logo ou de la mise en page des cartes officielles). Le nom affiché est une constante unique `APP_NAME` (par défaut « UNO Néon ») pour pouvoir le changer en une ligne avant un déploiement public.

## 2. Glossaire (nommage dans le code)

| Français (UI, docs) | Anglais (code) | Remarque |
|---|---|---|
| Partie (jusqu'à 500 pts) | `Match` | contient plusieurs manches |
| Manche | `Round` | se termine quand un joueur n'a plus de cartes |
| Salon | `Room` | identifié par un `RoomCode` |
| Hôte | `Host` | joueur qui règle les paramètres et lance la partie |
| Pioche | `DrawPile` | |
| Défausse | `DiscardPile` | |
| Main | `Hand` | |
| Carte | `Card` | identifiée par un `CardId` unique dans la partie |
| Couleur | `Color` | `Red`, `Yellow`, `Green`, `Blue` ; un Joker non posé n'a pas de couleur (`std::optional<Color>` vide, voir ADR 0009) |
| Valeur | `Rank` | `Zero`…`Nine`, `Skip`, `Reverse`, `DrawTwo`, `Wild`, `WildDrawFour` |
| Passe ton tour | `Skip` | |
| Inversion | `Reverse` | |
| +2 | `DrawTwo` | |
| Joker | `Wild` | |
| Joker +4 | `WildDrawFour` | |
| Pénalité de pioche en attente | `PendingDraw` | utile pour le cumul |
| Vue d'un joueur | `PlayerView` | seule donnée de partie envoyée à un client |

## 3. Règles du jeu (officielles, activées par défaut)

**Matériel** : 108 cartes.
- Par couleur (×4) : un 0, deux de chaque de 1 à 9, deux `Skip`, deux `Reverse`, deux `DrawTwo` → 25 cartes.
- 4 `Wild` et 4 `WildDrawFour`.

**Mise en place d'une manche**
- Le donneur de la première manche est tiré au hasard, puis il tourne d'un siège à chaque manche.
- 7 cartes par joueur. Le reste forme la pioche. On retourne la première carte de la pioche sur la défausse.
- Effet de la première carte retournée :
  - `WildDrawFour` : remise dans la pioche, pioche remélangée, on retourne une nouvelle carte.
  - `Wild` : le premier joueur choisit la couleur puis joue normalement.
  - `DrawTwo` : le premier joueur pioche 2 cartes et passe son tour.
  - `Skip` : le premier joueur passe son tour.
  - `Reverse` : le donneur joue en premier, puis le jeu part dans l'autre sens.
- Le premier joueur est à gauche du donneur (sens horaire = ordre des sièges croissant).

**Tour de jeu**
- Une carte est jouable si elle a la même couleur que la couleur courante, ou la même valeur/symbole que la carte du dessus, ou si c'est un `Wild`/`WildDrawFour`.
- **Pioche** (règle `drawRule`, §4). Règle officielle (`official`) : un joueur peut toujours choisir de ne pas jouer : il pioche alors 1 carte. Si cette carte est jouable, il **peut** la jouer immédiatement (et seulement celle-là), sinon son tour se termine (action « Passer »). Règle **guidée** (`guided`, défaut des salons, ADR 0017) : on ne propose un choix que s'il y en a un (voir §4).
- `Skip` : le joueur suivant passe son tour.
- `Reverse` : le sens de jeu s'inverse. **À 2 joueurs, `Reverse` agit comme `Skip`.**
- `DrawTwo` : le joueur suivant pioche 2 cartes et passe son tour.
- `Wild` : le joueur choisit la nouvelle couleur courante.
- `WildDrawFour` : le joueur choisit la couleur ; le suivant pioche 4 cartes et passe son tour. Il n'est **légal** que si le joueur n'a aucune carte de la **couleur courante** (avoir une carte de même valeur d'une autre couleur, ou un autre Joker, est permis).
- **Contestation du +4** (règle officielle, mode par défaut `officialChallenge`) : le +4 peut être joué illégalement (bluff). Le joueur visé choisit « Accepter » (pioche 4, passe) ou « Contester ». Le serveur vérifie la main du poseur au moment où il a joué :
  - bluff avéré → le poseur pioche 4, le contestataire ne pioche rien et joue normalement ;
  - +4 légal → le contestataire pioche 6 et passe son tour.
  - Pendant la contestation, seul le contestataire voit la main du poseur (information révélée par la règle elle-même), le temps d'afficher le verdict.

**UNO**
- Un joueur qui pose son avant-dernière carte doit annoncer « UNO ». Il peut l'annoncer pendant son tour quand il a 2 cartes (avant de poser ; l'annonce est annulée s'il ne descend pas à 1 carte), ou à 1 carte tant qu'il n'a pas été contré.
- **Fenêtre de contre-UNO** (ADR 0018) : elle s'ouvre quand un joueur passe à 1 carte sans avoir annoncé. Pendant **2 s de grâce**, lui seul peut encore annoncer (une connexion lente ne le pénalise pas). Ensuite, tout autre joueur peut le « prendre » (bouton « Contre-UNO ») jusqu'à **15 s** après l'ouverture, même si la partie continue ; le fautif pioche 2 cartes. Le premier contre-UNO valide ferme la fenêtre, les suivants sont ignorés. Elle se ferme aussi quand le joueur annonce, n'a plus exactement 1 carte, ou à l'échéance.
- Annoncer UNO sans y avoir droit n'a aucun effet (le bouton n'est simplement pas proposé).

**Pioche vide** : on remélange la défausse sauf la carte du dessus pour reformer la pioche. Si pioche + défausse ne suffisent pas pour une pénalité, le joueur pioche ce qui reste (pas d'erreur).

**Pioche multiple** (`drawAmount: untilPlayable`, ADR 0024) : pendant la boucle, la défausse est remélangée (sauf la carte du dessus) dès que la pioche est vide, avec un événement `deckReshuffled` à cet endroit ; si pioche et défausse sont vides, la boucle s'arrête et le tour passe. Les adversaires ne voient que le nombre de cartes piochées.

**Fin de manche** : dès qu'un joueur pose sa dernière carte. Si c'est un `DrawTwo` ou un `WildDrawFour`, le joueur suivant pioche quand même (ces cartes comptent au score).

**Score** : le gagnant de la manche marque la somme des cartes restant dans les mains adverses : chiffres = valeur faciale, `Skip`/`Reverse`/`DrawTwo` = 20, `Wild`/`WildDrawFour` = 50. La partie se termine quand un joueur atteint l'objectif (500 par défaut).

**Joueurs** : 2 à 10 par salon.

## 4. Options maison (réglées par l'hôte dans le salon)

Chaque option est une **politique injectable** dans le moteur (voir §7, Strategy) : ajouter une option ne doit jamais demander de modifier le code des règles existantes (principe ouvert/fermé).

| Option | Valeurs | Défaut | Effet |
|---|---|---|---|
| `stacking` | `off`, `sameType`, `mixed` | `off` | `sameType` : +2 sur +2 et +4 sur +4. `mixed` : +2 ou +4 sur +2, et +4 sur +4 (jamais +2 sur +4). La pénalité s'accumule dans `PendingDraw` jusqu'à ce qu'un joueur ne puisse/veuille plus empiler : il pioche le total et passe. |
| `jumpIn` | bool | `false` | Interception : n'importe quel joueur peut poser **hors de son tour** une carte strictement identique (même couleur ET même valeur, Jokers exclus) à celle du dessus. Le jeu reprend depuis ce joueur. |
| `sevenZero` | bool | `false` | Poser un 7 : échange de main avec un joueur choisi. Poser un 0 : toutes les mains tournent d'un joueur dans le sens du jeu. |
| `drawAmount` | `untilPlayable`, `one` | `untilPlayable` | **Pioche jusqu'à pouvoir jouer** (ADR 0024), indépendante de `drawRule`. `untilPlayable` : un joueur qui n'a **aucune** carte jouable pioche, en une seule action, jusqu'à obtenir une carte jouable (même 12 cartes) ; une pioche volontaire (avoir une carte jouable et choisir de piocher) reste d'une carte. Le moteur émet un `cardsDrawn` par carte, le client rythme l'animation. Ensuite le comportement habituel s'applique à la dernière carte (posée automatiquement si elle est normale en pioche guidée, choix si elle est spéciale ou en règle officielle). Pioche et défausse épuisées : on s'arrête et le tour passe. `one` : une carte (règle du §3). Libellé dans le salon : « Pioche : jusqu'à pouvoir jouer / 1 carte ». |
| `wildDrawFourMode` | `officialChallenge`, `strict` | `officialChallenge` | `strict` : le serveur refuse un +4 illégal, pas de contestation. |
| `drawRule` | `guided`, `official` | `guided` | **Pioche guidée** (ADR 0017). Cartes « spéciales » : +2, Joker, +4 (Passe et Inversion sont normales). Aucune carte jouable → pioche automatique après ~1,2 s. **Chaque carte piochée prend `drawStepMs` (1 s) à s'afficher** : le coup forcé qui suit une pioche de N cartes attend N × `drawStepMs` de plus, et l'horloge du tour qui commence alors démarre après ce temps (ADR 0026). Des cartes jouables mais aucune spéciale → piocher est refusé (`ILLEGAL_MOVE` / `MUST_PLAY`), il faut jouer. Au moins une spéciale jouable → le joueur choisit : poser, ou piocher en cliquant sur le paquet. Carte piochée non jouable → fin du tour ; jouable et normale → posée automatiquement après ~1,2 s ; jouable et spéciale → posée (clic sur la carte) ou gardée (clic sur le paquet). `official` : règle du §3. |
| `declareUnoToWin` | bool | `false` | **UNO obligatoire pour gagner** (ADR 0019) : on ne peut pas poser sa dernière carte sans avoir annoncé UNO ; le serveur refuse avec `ILLEGAL_MOVE` / `MUST_DECLARE_UNO`. La vue expose `me.mustDeclareUno` (calculé par le serveur) et le client met en avant le bouton UNO. Une carte jouable bloquée par l'annonce manquante ne déclenche jamais la pioche automatique de la pioche guidée. |
| `turnTimerSeconds` | `0` (off), 15, 30, 60 | 30 | À l'expiration : pénalité en attente acceptée, sinon pioche 1 + passe (en pioche guidée : le joueur qui doit jouer joue sa première carte jouable) ; choix de couleur en attente → couleur tirée au hasard. |
| `scoreTarget` | 250, 500, `singleRound` | 500 | |
| `maxPlayers` | 2–10 | 6 | |

Interactions à gérer explicitement (et à tester) :
- `stacking` + contestation : le joueur visé par un +4 choisit entre Accepter, Contester ou Empiler (si autorisé).
- `jumpIn` + `stacking` : une interception sur un +2 empile la pénalité si le cumul est actif.
- `jumpIn` est une **course** : le serveur traite les messages dans l'ordre d'arrivée sur un seul thread (§9), le premier coup valide gagne, les autres reçoivent `ILLEGAL_MOVE` avec la raison `JUMP_IN_TOO_LATE`.

## 5. Cas limites de jeu (tous testés)

- Joueur déconnecté pendant son tour : le timer continue ; à l'expiration, action automatique (§4). Délai de grâce de reconnexion : 60 s, puis retrait du salon ; ses cartes retournent sous la pioche.
- Départ de l'hôte : l'hôte passe au joueur connecté suivant dans l'ordre des sièges.
- Il ne reste qu'un joueur connecté dans une partie en cours : la partie se termine, il gagne par forfait.
- Pénalité plus grande que les cartes disponibles : on pioche ce qui existe.
- `sevenZero` à 2 joueurs : le 7 échange forcément avec l'adversaire ; le 0 échange aussi les deux mains.
- Un 7 posé comme dernière carte : pas d'échange, la manche se termine.
- Contre-UNO sur un joueur qui s'est déconnecté : autorisé.
- Deux contre-UNO simultanés : le premier traité gagne, le second reçoit `UNO_WINDOW_CLOSED`. Un contre-UNO pendant la grâce de 2 s reçoit `UNO_GRACE_PERIOD`. Plusieurs joueurs peuvent avoir une fenêtre ouverte en même temps.
- Revanche : même salon, mêmes joueurs connectés, scores remis à zéro.
- Un joueur part entre deux manches : les points de la manche terminée restent acquis (ils sont comptés quand elle se termine) ; il emporte sa main et son score ; la manche suivante se joue sans lui (forfait s'ils étaient deux).
- `declareUnoToWin` : un joueur à 1 carte jouable qui n'a pas annoncé UNO reçoit `MUST_DECLARE_UNO` s'il tente de la poser. Si le minuteur de tour expire dans cet état et qu'il ne peut pas piocher (pioche guidée, carte normale), le serveur annonce UNO puis pose la carte à sa place.
- Le minuteur de tour n'est pas relancé par un événement qui ne change pas le tour (contre-UNO, annonce, fin d'une fenêtre de contre-UNO) : ADR 0020.

## 6. Architecture globale

```
uno-neon/
├── CLAUDE.md
├── docs/            SPEC.md, PROGRESS.md, SETUP.md, PROMPTS.md, adr/, DEPLOY.md
├── protocol/        schema/*.schema.json (source de vérité), examples/*.json (valides et invalides)
├── server/          C++23, CMake, vcpkg
│   ├── core/        bibliothèque uno_core : domaine pur, aucune E/S
│   ├── app/         bibliothèque uno_app : salons, sessions, cas d'usage, timers abstraits
│   ├── net/         bibliothèque uno_net : uWebSockets, codec JSON, validation, rate limiting
│   ├── tests/       Catch2 (unit, simulation, contrat, intégration)
│   └── main.cpp     racine de composition (câblage + config)
├── client/          Angular 22
├── e2e/             Playwright (multi-joueurs, accessibilité, captures)
├── deploy/          Dockerfile, Caddyfile, docker-compose.yml
└── .github/workflows/ci.yml
```

**Règle de dépendance (architecture hexagonale / ports et adaptateurs)** : `net → app → core`, jamais l'inverse. `core` ne connaît ni le réseau, ni le JSON, ni le temps, ni l'aléatoire global.
*Pourquoi* : les règles du UNO sont la partie la plus riche et la plus risquée ; les isoler permet de les tester à 100 % sans lancer de serveur, et de changer de transport (WebSocket, bots, tests) sans les toucher.

**Pas de règles dans le client** : le serveur calcule et envoie `playableCardIds`, `canDraw`, `canCallUno`… Le client affiche, il ne décide pas. *Pourquoi* : une seule source de vérité (DRY), aucune divergence possible entre deux implémentations des règles, et un client impossible à « tricher ».

**Contrat d'abord** : le protocole (§8) est défini en phase 0, avant le serveur et l'UI. Les membres du groupe peuvent ensuite travailler en parallèle (C++ d'un côté, Angular sur données simulées de l'autre).

## 7. Cœur du jeu (C++) et design patterns

### 7.1 Modèle
- **Types forts** : `PlayerId`, `CardId`, `RoomCode` sont des structs distinctes (pas des `int`/`std::string` nus) → impossible de passer un `CardId` là où on attend un `PlayerId`.
- `Card { CardId id; std::optional<Color> color; Rank rank; }` (les Jokers n'ont pas de couleur propre).
- Les `CardId` sont attribués **aléatoirement** à chaque partie : un identifiant ne révèle rien sur la carte.
- `Round` est l'agrégat : pioche, défausse, mains, sièges, sens, couleur courante, joueur courant, phase de tour, `PendingDraw`, fenêtre UNO.
- `Match` enchaîne les `Round`, gère le donneur et les scores.

### 7.2 API du moteur (« functional core, imperative shell »)
```cpp
[[nodiscard]] std::expected<std::vector<DomainEvent>, DomainError>
Round::apply(PlayerId actor, const PlayerAction& action, RandomSource& random); // RandomSource en paramètre : ADR 0010

[[nodiscard]] PlayerView project(const Round& round, PlayerId viewer);
[[nodiscard]] std::optional<ClientEvent> project(const DomainEvent& event, PlayerId viewer);
```
Le moteur ne notifie personne : il **retourne** des événements. La couche `app` décide quoi en faire (diffuser, planifier un timer…). *Pourquoi* : pas de callbacks cachés, tout est testable en comparant des valeurs.

### 7.3 Patterns attendus (et pourquoi)
| Pattern | Où | Pourquoi |
|---|---|---|
| **Command** | `PlayerAction = std::variant<PlayCard, DrawCard, Pass, RespondPenalty, CallUno, CatchUno, TimeoutExpired>` | Chaque intention est un objet validable, journalisable et rejouable. Avec la graine + la liste des actions, on rejoue une partie à l'identique pour déboguer. |
| **State** | `TurnPhase = std::variant<AwaitingPlay, AwaitingDrawnCardDecision, AwaitingPenaltyResponse, RoundOver>` + `std::visit` | Chaque phase n'accepte que ses actions ; les états invalides sont irreprésentables. Variante moderne du State du GoF : ensemble fermé, sémantique de valeur, pas d'allocation, exhaustivité vérifiée par le compilateur. Documenter dans une ADR pourquoi on préfère `std::variant` aux classes virtuelles ici. |
| **Strategy / Policy** | `StackingPolicy`, `WildDrawFourPolicy`, `DrawPolicy`, `JumpInPolicy`, `SevenZeroPolicy`, construites depuis `RoomSettings` | Principe ouvert/fermé : une nouvelle option maison = une nouvelle politique, sans modifier les règles existantes. |
| **Factory** | fonction libre `createStandardDeck(RandomSource&)` (voir ADR 0009) | Centralise la composition des 108 cartes et l'attribution aléatoire des identifiants. |
| **Observer** (via événements retournés) | `app` diffuse les `DomainEvent` projetés à chaque joueur | Découple le moteur de la diffusion réseau. |
| **Repository** | `RoomRepository` (interface) + `InMemoryRoomRepository` | La persistance peut changer sans toucher aux cas d'usage. |
| **Injection de dépendances** | `RandomSource`, `Scheduler`, `RoomRepository` injectés par constructeur, câblés dans `main.cpp` | Inversion des dépendances : les tests injectent des implémentations déterministes. |
| **Projection** | `project(...)` → `PlayerView` / `ClientEvent` | Seul point où l'information est filtrée par joueur : c'est là que vit l'anti-triche, testé exhaustivement. |

Les choix de couleur (Joker) et de cible (7 en `sevenZero`) font **partie de l'action** `PlayCard` : le client ouvre le sélecteur avant d'envoyer. Cela supprime des états intermédiaires côté serveur.

### 7.4 Aléatoire
```cpp
class RandomSource {
public:
    virtual ~RandomSource() = default;
    [[nodiscard]] virtual std::uint32_t uniform(std::uint32_t upperExclusive) = 0;
};
```
- `CryptoRandomSource` (libsodium `randombytes_uniform`) : production, pour le mélange, les codes de salon et les jetons.
- `SeededRandomSource` (`std::mt19937_64`) : **tests et simulation uniquement**, jamais compilé dans le binaire de production sauf avec l'option CMake `UNO_ENABLE_TEST_HOOKS`.
- Mélange : Fisher-Yates avec `uniform()` (sans biais de modulo).

## 8. Protocole client ↔ serveur

### 8.1 Transport et source de vérité
- WebSocket, trames texte JSON UTF-8, chemin **`/ws` sur la même origine que le site** (en dev, le serveur de dev Angular proxifie `/ws` vers `localhost:9001`). *Pourquoi* : même URL relative en dev et en prod, pas de CORS, la vérification de l'`Origin` reste simple.
- Source de vérité : `protocol/schema/*.schema.json` (JSON Schema 2020-12).
- Types TypeScript générés dans `client/src/app/protocol/generated/` (`npm run protocol:gen`, avec `json-schema-to-typescript`). Ne jamais les éditer à la main.
- DTO C++ écrits à la main dans `server/net/protocol/` avec `to_json`/`from_json` (nlohmann).
- **Tests de contrat** : chaque fichier de `protocol/examples/valid/` doit être accepté et re-sérialisé à l'identique par le C++ ; chaque fichier de `examples/invalid/` doit être rejeté avec le bon code d'erreur.

### 8.2 Enveloppe
Client → serveur : `{ "v": 1, "id": "c-42", "type": "game.playCard", "payload": { … } }` (`id` unique par connexion, pour corréler la réponse).
Serveur → client : réponses `{ "v": 1, "type": "ack", "replyTo": "c-42" }` ou `{ "v": 1, "type": "error", "replyTo": "c-42", "payload": { "code": "NOT_YOUR_TURN", "message": "…" } }`, et messages poussés `{ "v": 1, "type": "game.update", "payload": { … } }`.

### 8.3 Messages client → serveur
| Type | Payload | Conditions |
|---|---|---|
| `session.hello` | `{ sessionToken?, clientVersion }` | premier message obligatoire |
| `room.create` | `{ nickname, settings? }` | pas déjà dans un salon |
| `room.join` | `{ code, nickname }` | salon existant, non plein, en lobby |
| `room.leave` | `{}` | |
| `room.updateSettings` | `{ settings }` (partiel) | hôte, en lobby |
| `room.setReady` | `{ ready }` | en lobby |
| `room.kick` | `{ playerId }` | hôte, en lobby |
| `room.addBot` | `{ strategy: "random" \| "greedy" }` | hôte, en lobby (phase 5) |
| `match.start` | `{}` | hôte, ≥ 2 joueurs, tous prêts |
| `match.rematch` | `{}` | hôte, partie terminée |
| `game.playCard` | `{ cardId, chosenColor?, swapTargetId? }` | son tour, ou interception si `jumpIn` |
| `game.drawCard` | `{}` | son tour, phase `AwaitingPlay`, et si la règle de pioche l'autorise (`me.canDraw`) |
| `game.pass` | `{}` | phase `AwaitingDrawnCardDecision` (« garder la carte » : interdit en pioche guidée si la carte piochée est normale) |
| `game.respondPenalty` | `{ response: "accept" \| "challenge" }` | visé par une pénalité (empiler = `game.playCard`) |
| `game.callUno` | `{}` | 2 cartes et son tour, ou 1 carte dans la fenêtre UNO |
| `game.catchUno` | `{ targetId }` | fenêtre UNO ouverte sur la cible |
| `reaction.send` | `{ emote: "gg" \| "wow" \| "lol" \| "ouch" \| "think" \| "fire" }` | limité à 1 toutes les 2 s |

Pas de chat libre : uniquement des réactions prédéfinies. *Pourquoi* : aucune modération à gérer, aucun contenu utilisateur arbitraire affiché, surface d'attaque minimale.

### 8.4 Messages serveur → client
| Type | Payload |
|---|---|
| `session.welcome` | `{ sessionToken, playerId, resumedRoomCode? }` |
| `room.update` | `{ roomVersion, room: RoomView }` |
| `game.update` | `{ stateVersion, events: ClientEvent[], view: PlayerView }` |
| `reaction` | `{ playerId, emote }` |
| `room.closed` | `{ reason: "expired" \| "kicked" \| "hostClosed" }` |
| `ack` / `error` | voir §8.2 |

**Résolution d'un effet** (ADR 0027) : après chaque action, le serveur calcule ce que les clients mettent à la montrer (carte posée, effets spéciaux, cartes piochées au rythme `drawStepMs`) et en tire `actionsOpenAt`. Avant cette heure, il refuse les actions de tour (`EFFECT_IN_PROGRESS`), l'horloge du tour et le coup forcé n'ont pas démarré, et le client garde la main neutre et le paquet muet.

**Synchronisation** : après chaque action acceptée, chaque joueur reçoit un `game.update` contenant les événements (pour animer) ET sa vue complète (pour la vérité). L'état d'une partie de UNO fait quelques Ko : envoyer la vue entière à chaque fois coûte peu et évite toute logique de resynchronisation. Le client anime les événements puis s'aligne sur `view`. S'il reçoit un `stateVersion` non consécutif (message manqué, reconnexion), il saute l'animation et applique directement `view`.

### 8.5 Formes de données principales
```ts
type Color = "red" | "yellow" | "green" | "blue";
type Rank = "0"|"1"|"2"|"3"|"4"|"5"|"6"|"7"|"8"|"9"|"skip"|"reverse"|"drawTwo"|"wild"|"wildDrawFour";
interface Card { id: number; color: Color | null; rank: Rank; }

interface PlayerView {
  stateVersion: number;
  phase: "awaitingPlay" | "awaitingDrawnCardDecision" | "awaitingPenaltyResponse" | "roundOver" | "matchOver";
  me: {
    playerId: string; hand: Card[]; playableCardIds: number[];
    canDraw: boolean; canKeepDrawnCard: boolean; canCallUno: boolean; mustDeclareUno: boolean; // calculés par le serveur (ADR 0017)
    penaltyResponse: { amount: number; canChallenge: boolean; canStack: boolean } | null;
  };
  players: Array<{ playerId: string; nickname: string; seat: number; cardCount: number; score: number;
                   isConnected: boolean; isBot: boolean; isHost: boolean; hasCalledUno: boolean }>;
  currentPlayerId: string; direction: "clockwise" | "counterClockwise";
  currentColor: Color; discardTop: Card; drawPileCount: number;
  pendingDraw: number; turnDeadline: number | null; // epoch ms, horloge serveur
  actionsOpenAt: number; // epoch ms, horloge serveur : le serveur refuse toute action de tour reçue avant (ADR 0027)
  drawStepMs: number; // rythme des cartes piochées, dicté par le serveur (ADR 0026)
  unoWindows: Array<{ targetId: string; graceEndsAt: number; expiresAt: number }>; // fenêtres de contre-UNO ouvertes (ADR 0018), heure serveur
  round: number; settings: RoomSettings;
  roundResult: { winnerId: string; points: number; revealedHands: Record<string, Card[]> } | null;
}
```
`ClientEvent` (union discriminée par `kind`) : `cardPlayed`, `cardsDrawn` (`cards` présent seulement pour celui qui pioche, sinon `count`), `turnChanged`, `playerSkipped`, `directionChanged`, `colorChosen`, `penaltyStacked`, `challengeResolved`, `unoCalled`, `unoCaught`, `handsSwapped`, `handsRotated`, `deckReshuffled`, `roundEnded`, `matchEnded`, `playerDisconnected`, `playerReconnected`, `hostChanged`.

### 8.6 Codes d'erreur
`MALFORMED_MESSAGE`, `UNKNOWN_TYPE`, `UNSUPPORTED_VERSION`, `MESSAGE_TOO_LARGE`, `RATE_LIMITED`, `SESSION_REQUIRED`, `SESSION_EXPIRED`, `NICKNAME_INVALID`, `NICKNAME_TAKEN`, `ROOM_NOT_FOUND`, `ROOM_FULL`, `MATCH_IN_PROGRESS`, `NOT_HOST`, `NOT_ENOUGH_PLAYERS`, `PLAYERS_NOT_READY`, `NOT_YOUR_TURN`, `INVALID_PHASE`, `CARD_NOT_IN_HAND`, `ILLEGAL_MOVE` (avec `details.reason` : `COLOR_MISMATCH`, `WILD_DRAW_FOUR_ILLEGAL`, `COLOR_REQUIRED`, `SWAP_TARGET_REQUIRED`, `JUMP_IN_TOO_LATE`, `CANNOT_STACK`, `MUST_PLAY`, `MUST_DECLARE_UNO`…), `UNO_WINDOW_CLOSED`, `UNO_GRACE_PERIOD`, `EFFECT_IN_PROGRESS` (une action de tour reçue avant `actionsOpenAt` : l'effet en cours n'est pas terminé ; annoncer UNO et contrer ne sont jamais concernés).
Les messages d'erreur sont en anglais technique ; **le client traduit chaque code en message français clair**.

## 9. Serveur réseau (C++)

### 9.1 Pile technique
- **uWebSockets** (WebSocket + petit HTTP pour `/health`), **nlohmann-json**, **libsodium** (aléatoire cryptographique), **spdlog** (logs), **Catch2 v3** (tests). Toutes via `vcpkg.json` avec une `builtin-baseline` figée.
- CMake ≥ 3.28, `CMakePresets.json` avec au minimum : `dev` (Debug, Ninja, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`), `debug-asan` (sanitizers), `release`.
- Trois bibliothèques (`uno_core`, `uno_app`, `uno_net`) + un exécutable `uno_server`. Les tests se lient aux bibliothèques, pas à l'exécutable.

### 9.2 Modèle d'exécution
- **Une seule boucle d'événements uWebSockets, un seul thread** pour toutes les connexions et tous les salons. Aucun mutex dans `core`/`app`.
  *Pourquoi* : le UNO est au tour par tour et peu gourmand ; un seul thread sérialise naturellement les actions concurrentes (interceptions, contre-UNO simultanés) et élimine toute une classe de bugs (data races). Si un jour il faut monter en charge, on répartira les salons par thread (sharding), sans changer le moteur.
- Les timers passent par une interface `Scheduler` (`schedule(delay, callback) → TimerHandle`, `cancel`) : `UwsScheduler` en prod, `ManualScheduler` (avance le temps à la main) dans les tests.

### 9.3 Sessions, salons, reconnexion
- `session.hello` sans jeton → création d'une session, jeton de 128 bits aléatoires cryptographiques encodé en base64url. Avec jeton valide → reprise de la session et du salon éventuel (`resumedRoomCode`).
- Côté client, le jeton est stocké en `sessionStorage` (un onglet = un joueur : pratique pour tester à plusieurs sur une seule machine).
- Code de salon : 6 caractères parmi `ABCDEFGHJKMNPQRSTUVWXYZ23456789` (sans I, L, O, 0, 1 pour éviter les confusions), généré par `CryptoRandomSource`, unicité vérifiée.
- Pseudo : 2 à 16 caractères après `trim`, lettres Unicode, chiffres, espace, `_`, `-` ; unique dans le salon (insensible à la casse).
- Expiration : salon en lobby inactif 15 min, salon en fin de partie 5 min, session sans connexion 10 min.
- Déconnexion en cours de partie : grâce de 60 s (§5).

### 9.4 Protection des entrées
- Taille max d'un message : 4 Kio (`maxPayloadLength`), `idleTimeout` avec ping/pong.
- JSON invalide, champ manquant/en trop, type inconnu → `error` typé ; après 10 erreurs de format sur une connexion → fermeture (code 1008).
- **Rate limiting** par connexion (seau à jetons : capacité 20, recharge 10/s) ; par IP : 5 créations de salon/min, 20 tentatives de `room.join`/min (anti-force brute des codes). Derrière Caddy, l'IP réelle vient de `X-Forwarded-For` **uniquement** si le proxy est déclaré de confiance dans la config.
- **Vérification de l'`Origin`** au handshake contre `UNO_ALLOWED_ORIGINS` (protection contre le détournement de WebSocket depuis un autre site).

### 9.5 Configuration (variables d'environnement)
`UNO_PORT` (défaut 9001), `UNO_ALLOWED_ORIGINS` (liste séparée par des virgules), `UNO_TRUSTED_PROXY` (bool), `UNO_LOG_LEVEL`, et uniquement si compilé avec `UNO_ENABLE_TEST_HOOKS` : `UNO_TEST_SEED` (partie déterministe pour les tests E2E) et `UNO_TEST_RECONNECT_GRACE_MS` (grâce de reconnexion raccourcie).

### 9.6 Observabilité
- Logs structurés spdlog : connexion/déconnexion, création/fermeture de salon, erreurs, avec `playerId` et `roomCode` (jamais le jeton de session).
- `GET /health` → `{ "status": "ok", "rooms": n, "connections": n, "uptimeSeconds": n }`.

## 10. Client Angular

### 10.1 Pile technique
- Angular 22 créé depuis la racine du repo avec `npx @angular/cli@latest new client --ai-config=claude --style=scss --ssr=false --routing --skip-git` (`--skip-git` car le repo git existe déjà à la racine) : standalone, **zoneless**, signals, OnPush, Vitest. Si une option n'existe plus dans la version installée, vérifie avec `ng new --help` et adapte.
- **Angular CDK** : `a11y` (`LiveAnnouncer`, `FocusTrap`, `ListKeyManager`), `overlay` (modales, bottom sheets).
- Formulaires (pseudo, code) : **Signal Forms**.
- Pas de bibliothèque de composants UI (Material, PrimeNG…) : tout le design est sur mesure (§11).
- Polices auto-hébergées via `@fontsource-variable` (Fredoka, Inter), licence OFL (pas de CDN : performance, confidentialité, CSP plus simple).
- ESLint (`angular-eslint`) + Prettier.

### 10.2 Structure
```
client/src/app/
├── core/            GameTransport (interface), WebSocketTransport, FixtureTransport, SessionService, ErrorMessages (code → FR)
├── protocol/        generated/ (types générés, ne pas éditer)
├── state/           GameStore, RoomStore (signals), sélecteurs dérivés (computed)
├── features/
│   ├── home/        page Accueil
│   ├── room/        page Salon (lobby) et table de jeu (même route, vue selon la phase)
│   └── results/     fin de manche / fin de partie
├── ui/              composants de présentation réutilisables (card, card-back, neon-button, glass-panel, avatar, badge, modal, toast, timer-ring…)
└── dev/             galerie /dev/gallery (chargée seulement en dev)
client/src/styles/   tokens.scss, themes.scss, base.scss, typography.scss, backgrounds.scss, motion.scss
```

### 10.3 Découplage transport / état (inversion de dépendances)
- `GameTransport` (interface, **Adapter**) : `connect()`, `send(message)`, `messages$`/signal, `status` (`connecting` | `open` | `reconnecting` | `closed`).
  - `WebSocketTransport` : vraie connexion, **reconnexion automatique avec backoff exponentiel plafonné et jitter** (0,5 s → 8 s), renvoie `session.hello` avec le jeton à chaque reconnexion.
  - `FixtureTransport` : rejoue des scénarios écrits à la main (`dev/fixtures/*.ts`) et répond aux intentions de façon scriptée. Sert à la galerie, au développement de l'UI sans serveur et aux tests unitaires du store.
- Fourni via un `InjectionToken<GameTransport>` : les stores et composants ne savent pas lequel est utilisé.
- *Pourquoi* : l'équipe UI avance en parallèle de l'équipe serveur, et le vrai `GameStore` est testé (pas un faux store).

### 10.4 Stores (signals)
- `GameStore` expose des signals en lecture seule (`view`, `isMyTurn`, `playableIds`, `pendingAnimations`…) et des méthodes d'intention (`playCard(cardId, color?, target?)`, `draw()`, `pass()`, `callUno()`, `catchUno(id)`, `respondPenalty(r)`).
- **Façade** : les composants n'appellent jamais le transport directement.
- File d'animations : les `events` d'un `game.update` sont joués dans l'ordre, puis la vue est appliquée. Si l'onglet est masqué ou si `prefers-reduced-motion` est actif, on applique la vue directement.
- Erreurs : chaque `error` devient un toast en français via `ErrorMessages`. Les erreurs attendues (clic trop tard sur contre-UNO) sont discrètes, les erreurs graves (session expirée) redirigent vers l'accueil avec une explication.

### 10.5 Routage
- `/` Accueil — `/r/:code` Salon + table (lien partageable) — `/dev/gallery` (dev uniquement) — `**` page 404 dans le style du jeu.
- Arriver sur `/r/:code` sans pseudo → petite modale de pseudo, puis `room.join`.

## 11. Design system

### 11.1 Intention
**L'esprit du jeu de cartes classique, en plus sombre** : quatre couleurs franches (rouge, jaune, vert, bleu), une table de jeu, de gros chiffres lisibles, une ambiance conviviale.
- **Le néon est rare** : il est réservé aux **cartes** (bord de la couleur de la carte, carte jouable, dessus de la défausse) et à quelques **actions** (boutons primaires en vert, destructifs en rouge, UNO en jaune, Contre-UNO en rouge). Tout le reste est sobre.
- **Panneaux en verre sombre** : fond translucide avec flou, liseré clair très fin en haut, ombre portée douce, aucun halo, **un seul niveau d'élévation**. Pas de flou sur les cartes ni sur de grandes surfaces animées (coût GPU, surtout sur mobile) ; repli opaque sans `backdrop-filter`.
- **Chaque lueur a un sens** : ce qui brille est jouable, actif ou urgent.
- **Original** : ni le logo ni les cartes officielles ne sont repris (§1).
- Le fond est un dégradé radial anthracite / bleu nuit très sombre.

### 11.2 Tokens (`client/src/styles/tokens.scss`, propriétés CSS)
Un composant n'écrit **aucune couleur en dur** : tout passe par les tokens. Tous les tokens sont centralisés dans `:root` (§11.3).

| Famille | Tokens |
|---|---|
| Fond | `--page-bg`, `--panel` (verre translucide), `--panel-solid` (repli et pastilles), `--panel-rim` (liseré), `--field` (champ, puits) |
| Texte | `--text`, `--text-dim` (≥ 4,5:1 sur `--panel`), `--text-on-neon` (texte sombre sur aplat néon) |
| Couleurs du jeu | `--game-red`, `--game-yellow`, `--game-green`, `--game-blue` |
| Actions néon | `--primary` (vert), `--danger` (rouge), `--uno` (jaune), `--glow-edge`, `--glow-strong` |
| Cartes | `--card-red/yellow/green/blue` (fonds pleins), `--card-body` (jokers, dos), `--card-ink` (contour des chiffres), `--back-neon` (bord du dos) |
| Forme, espace | `--radius-*`, `--space-1…6`, `--elevation` (l'unique ombre des panneaux) |
| Typographie | `--font-display`, `--font-ui`, `--font-mono`, `--fs-*` |
| Mouvement | `--dur-fast/base`, `--ease-out` |

**Typographie** (auto-hébergée, licence OFL, `@fontsource-variable`) : **Fredoka** (ronde et grasse) pour les titres, le logo, les chiffres et les boutons ; **Inter** pour l'interface ; **JetBrains Mono** pour le code de salon.

**Contrastes** : texte courant ≥ 4,5:1 sur les panneaux ; texte sombre (`--text-on-neon`) sur tous les aplats néon ; les chiffres blancs des cartes ont un contour sombre. Mesuré avec axe (WCAG 2.2 AA) sur l'accueil, le salon et la table : aucune violation. Jamais de texte directement sur le fond : un panneau, une carte ou un bouton.

### 11.3 Thème « Nuit »
Un seul thème : anthracite / bleu nuit très sombre, panneaux en verre sombre. Tous les tokens vivent dans `:root` (`tokens.scss`) : ajouter un thème plus tard, c'est redéfinir ces variables sous un sélecteur (`[data-theme]`), sans toucher aux composants. (Le thème « Tapis », rouge très sombre, a été écarté : ADR 0021.)

### 11.4 Les cartes (composant `ui/card`, un seul SVG paramétré)
- Proportions 5:7. Tailles proportionnelles à la hauteur de l'écran (lisibles dès 1280 × 720) : main de 100 à 136 px (64 px en mobile), défausse et pioche de 96 à 168 px (88 px en mobile), dos d'adversaire de 36 à 60 px.
- **Face** : **fond plein de la couleur** de la carte (saturée mais pas fluo), **bord néon** de la couleur du jeu, ovale incliné plus sombre au centre, grand chiffre blanc à contour sombre en Fredoka, petite valeur et symbole de couleur (blanc cerné) dans deux coins opposés. Les jokers ont un corps sombre et l'anneau des quatre couleurs.
- **Symbole de forme par couleur** (daltonisme, obligatoire partout où une couleur de jeu apparaît) : rouge = triangle ▲, jaune = cercle ●, vert = carré ■, bleu = losange ◆. Présent dans les coins, le sélecteur de couleur, l'indicateur de couleur active et les textes (« Bleu ◆ »).
- **Icônes d'action** originales : `Skip` = cercle barré ; `Reverse` = deux flèches courbes ; `DrawTwo` = « +2 » et deux mini-cartes ; `Wild` = anneau à quatre quadrants ; `WildDrawFour` = anneau + « +4 ». Un Joker posé montre la couleur choisie dans son anneau.
- **Dos** : fond uni sombre, wordmark penché au centre, bord néon (`--back-neon`).
- **États** : jouable = nettement surélevée (34 px), bord néon et halo de sa couleur, curseur main, et encore plus haute au survol, devant ses voisines ; non jouable = **opaque**, atténuée par un **voile sombre** (`--card-veil`) et une légère baisse de luminosité, **sans désaturer** (le jaune reste jaune, le vert reste vert) et jamais par `opacity` (qui laisserait voir la carte du dessous), abaissée, **toujours lisible** ; dessus de la défausse = bord qui brille ; pas mon tour = toute la main neutre et un peu abaissée, aucune réaction.
- **Le rendu SVG est la seule source** : pas de fichier image par carte.

### 11.5 Logo, icônes, composants
- **Logo** (`ui/logo`) : wordmark « UNO » en **SVG inline**, une tuile arrondie par lettre de `APP_NAME` (`core/app-name.ts`), aux couleurs du jeu, légèrement penchées. Le nom du jeu vient d'une seule constante.
- **Icônes** (`ui/icon`) : composant SVG maison (porte, couronne, copier, coche, fermer, flèches de sens, palette), `currentColor`, décoratives (`aria-hidden`), sans `innerHTML` ni dépendance.
- **Boutons** (`button[appButton]`) : `primary` (vert néon plein), `danger` (contour rouge néon, plein au survol), `uno` (jaune), `neutral`, `ghost`. Un bouton **atténué** pose `aria-disabled` (et non `disabled`) quand son survol ou son focus doit expliquer pourquoi.
- Autres : `avatar` (initiale dans un disque teinté), `segmented-control`, `modal` (`<dialog>` natif), `toast`, `connection-banner`.

## 12. Écrans et parcours

### 12.1 Accueil `/`
Le logo en haut, **un seul panneau sobre** : pseudo, « Créer un salon » (primaire), « Rejoindre » avec le champ code. Rien d'autre.

### 12.2 Salon (lobby) `/r/:code`
- **Barre du haut** : logo à gauche ; à droite le code du salon (pastille mono) avec un bouton copier qui répond « Copié », puis « Quitter » (icône porte, rouge néon).
- **Colonne gauche — joueurs** : couronne pour l'hôte (`aria-label="Hôte"`), « toi » discret, coche verte quand le joueur est prêt, « Exclure » en rouge néon (hôte seulement, avec confirmation).
- **Colonne droite — réglages** : formulaire pour l'**hôte seulement** (durée, minuteur, joueurs maximum, règle de pioche, dernière carte) ; les autres voient un **résumé en lecture seule** sur deux lignes. Le serveur n'a pas à se fier à cet affichage : un non-hôte qui tente de modifier les réglages ou d'exclure reçoit `NOT_HOST` et rien ne change (test).
- **Bas, au centre, un grand bouton** : « Prêt » (vert néon, bascule, `aria-pressed`) pour un joueur ; « Lancer la partie » (vert néon) pour l'hôte. Il n'y a pas de texte « En attente de… » : tant que la partie ne peut pas démarrer, « Lancer » est **atténué avec `aria-disabled`** (jamais `disabled`, pour garder survol et focus) et une **info-bulle**, au survol, au focus et au toucher, donne la raison (« En attente de : Loïc », « Il faut au moins 2 joueurs »), reliée par `aria-describedby`.
- **Mobile** : colonnes empilées, bouton principal collé en bas.

### 12.3 Table de jeu `/r/:code` (phase de jeu)
- **Disposition d'une vraie table** : moi en bas au centre (ma main centrée, **ma pastille en bas à gauche** avec le nombre de cartes et le minuteur) ; les adversaires utilisent toute la largeur et la hauteur de l'écran. **L'ellipse de la table prend la place que les sièges lui laissent** : `tableGeometry` (fonction pure, `table-geometry.ts`) modélise la boîte de chaque siège (éventail, pastille, badges), cherche la plus grande ellipse qui ne touche aucun siège (marge de 14 px) et qui contienne les deux piles, et réduit les sièges d'un cran si la place manque ; un test géométrique vérifie l'absence de chevauchement de 1 à 9 adversaires en 1280×720, 1440×900, 1920×1080 et 375×812. Positions des sièges calculées par une **fonction pure** `seatLayout(nombreAdversaires)` (positions et rotations sur une ellipse, testée) : 1 adversaire en face, en haut au centre ; 2 en haut à gauche et en haut à droite ; 3 à gauche, en haut, à droite ; 4 à gauche, en haut à gauche, en haut à droite, à droite ; 5 et plus à intervalles égaux sur l'arc gauche → haut → droite, sièges compacts à partir de 6. Les sièges suivent l'ordre de jeu dans le sens horaire à partir de ma gauche, pour que le sens affiché soit cohérent.
- **Chaque adversaire** : une vraie **main tenue face à lui, vue de dos**, au-dessus de la pastille : des dos de cartes UNO en **arc inversé** (le pivot est du côté du joueur, comme vu de l'autre côté de la table), avec une **perspective CSS** (légère inclinaison en profondeur) et une taille proportionnelle à la place disponible. L'adversaire **en face** (`seatLayout` : `facing`) a une grande main centrée ; les sièges latéraux la tournent autour de l'axe vertical vers le centre de la table (`yaw`, 32° au plus, **jamais de rotation à plat**). Dessinée d'après le seul nombre de cartes, **plafonnée à 15 dos** (le badge donne le nombre exact, pas de « +N ») ; une pastille avec l'avatar (initiale), le **nombre de cartes en badge**, le pseudo et l'état de connexion ; **lueur et anneau de minuteur** quand c'est son tour. Le badge « **UNO oublié !** » reste sur le siège de la cible tant que sa fenêtre de contre-UNO est ouverte.
- **Portrait étroit (375 px)** : jusqu'à 4 adversaires en arc compact en haut (avatar, nombre de cartes, mini-éventail de 5 dos au plus) ; au-delà, une bande défilante (focalisable au clavier). Ma pastille passe au-dessus de ma main, et ma main tient dans la largeur (chevauchement qui se resserre, la carte touchée passe devant).
- **Centre** : une grande ellipse douce (léger dégradé) dont le **liseré fin prend la couleur active** ; une lueur discrète glisse lentement le long du liseré **dans le sens du jeu** (coupée avec `prefers-reduced-motion`) ; la lueur a pour tête une petite flèche qui montre le sens du jeu (une seule animation CSS pilote la lueur et la flèche, qui restent calées l'une sur l'autre ; elles repartent à l'envers après une inversion). La pioche (cliquable quand le serveur dit `canDraw` ou `canKeepDrawnCard`, info-bulle « Piocher » / « Garder la carte ») et la défausse au milieu, la défausse entourée d'une **lueur de la couleur active** (indispensable après un Joker), et un **seul libellé** sous elle (« ▲ Rouge », forme et nom) ; la pénalité en attente.
- **Journal** : en haut à droite, les deux dernières lignes, petit, `aria-live="polite"`.
- **Ma main** : en bas, en éventail, qui **ne montre que les cartes arrivées** : pendant une pioche rythmée chaque carte prend sa place à son arrivée et l'éventail se réorganise en douceur (transition) ; main neutre et paquet muet jusqu'à `actionsOpenAt` ; pendant mon tour les cartes jouables sautent aux yeux (surélevées, bord néon et halo, curseur main) et les autres sont atténuées et abaissées ; hors de mon tour toute la main est neutre et un peu abaissée ; le chevauchement se resserre pour que la main tienne toujours dans la largeur.
- **Bouton UNO** : grand, rond, jaune, centré entre le centre de la table et ma main, **visible seulement quand il sert** ; mis en avant (taille, pulsation) quand `mustDeclareUno` est vrai, avec une phrase d'explication.
- **Contre-UNO** : plusieurs fenêtres peuvent coexister (`unoWindows`), donc **un bouton « Contre-UNO ! » par cible**, avec son nom, empilés au même endroit que le bouton UNO, en rouge néon. Chacun est **grisé avec un compte à rebours pendant la grâce**, puis actif jusqu'à l'échéance (heure serveur).
- **Mouvement réduit** : avec `prefers-reduced-motion`, toutes les pulsations et animations sont coupées.
- **Contestation du +4** : modale façon jeu de cartes : la carte, une ligne d'explication de la règle, « **Contester** » (rouge) et « **Accepter, piocher 4** », puis un **bandeau de résultat** une fois le verdict rendu.
- **Choix de couleur** (Joker et +4) : quatre grandes tuiles avec le nom de la couleur écrit, utilisables au clavier (touches 1 à 4).
- **Fin de manche** : mains révélées, scores, « Manche suivante » (départ automatique après 30 s). **Fin de partie** : vainqueur, classement, « Quitter ».
- **Perte de connexion** : panneau « Connexion perdue — reconnexion… (tentative n) » par-dessus la table. Session expirée : retour à l'accueil avec un message clair.

### 12.4 Galerie `/dev/gallery` (développement uniquement)
Toutes les cartes (108 faces + dos) et tous les composants dans tous leurs états ; la table de jeu alimentée par `FixtureTransport` avec des scénarios nommés : début de partie à 2, table pleine à 10, cumul de +4, contestation, fenêtre UNO, interception, fin de manche, reconnexion. Chaque scénario est accessible par une URL (`/dev/gallery?scenario=stacking`) pour que Playwright en prenne des captures.

## 13. Animations et son

### 13.1 Principes
- **Les événements du serveur pilotent les effets**, jamais une comparaison de vues (ADR 0022). Un `AnimationDirector` les consomme dans l'ordre (file) ; l'état affiché vient toujours de la dernière vue. Purement cosmétique : aucune animation ne retarde l'envoi d'une action ni ne bloque la saisie (la couche d'effets ne reçoit aucun clic). Si la file prend du retard (plus de 9 s en attente, onglet en arrière-plan) ou si une vue complète arrive (première vue, saut de `stateVersion`, reconnexion), on la vide et on montre l'état final.
- Seules `transform` et `opacity` sont animées (pas de `top/left/width`, pas de `box-shadow` animé : pour faire varier un halo, on anime l'`opacity` d'un pseudo-élément qui porte l'ombre).
- Technique **FLIP** avec l'API Web Animations pour les déplacements de cartes (main ou siège → défausse, pioche → main ou siège) : mesurer la position de départ et d'arrivée, animer l'écart en `transform`. Aucune dépendance.
- **Le jeu prend son temps** (ADR 0025) : une carte posée glisse jusqu'à la défausse en ~700 ms, s'y pose avec un léger rebond et reste visible ~400 ms avant l'action suivante ; les effets spéciaux (+2, +4, Passe, Inversion, Joker) durent 1 à 1,4 s ; les cartes d'une pioche partent du paquet l'une après l'autre, **au rythme `drawStepMs` annoncé par le serveur** (1 s par défaut, ADR 0026 : le compteur du joueur monte à chaque arrivée ; l'écart ne change pas en vitesse Rapide ; une pioche multiple compte pour une seule étape de la file et n'est jamais accélérée). **Toutes les durées sont définies au même endroit** (`client/src/app/ui/motion.ts`, publiées en propriétés CSS `--motion-*`) avec un multiplicateur global `--motion-scale`, réglé par le joueur : « **Vitesse des animations : Normale / Rapide** » (`localStorage`, jamais envoyé au serveur). Si plus de 3 étapes attendent, la file accélère pour rattraper son retard au lieu d'accumuler du décalage. Courbes d'accélération naturelles, **jamais plus d'un effet plein écran à la fois** (les petits effets sur un siège se chevauchent).
- **`prefers-reduced-motion: reduce`** : chaque effet est remplacé par un fondu de 160 ms ; aucune rotation continue (la lueur du sens du jeu est coupée), aucune pulsation.

### 13.2 Catalogue

**Qui voit quoi** (lot N) : un effet qui vise **un seul joueur** (+2, +4, Passe, Contre-UNO, résultat de la contestation du +4) s'affiche **en grand au centre chez lui seulement**, avec une phrase à la deuxième personne (« +2 : tu pioches 2 », « Tu passes ton tour ») ; chez les autres il se joue **sur le siège de la cible**, avec son nom (« +2 : Zoé pioche 2 »), et les cartes volent vers ce siège. Un effet qui concerne **tout le monde** (Inversion, roue de couleur du Joker et du +4) s'affiche au centre chez tous. Un effet prend la **couleur de la carte jouée** (+2 bleu → effet bleu ; blanc pour une carte noire), toujours par les tokens `--game-*`.

| Événement | Effet |
|---|---|
| `cardPlayed` | la carte vole de la main (ou du siège, face cachée puis retournée) jusqu'à la défausse, avec une légère rotation à l'arrivée ; la défausse montre les 5 dernières cartes en pile désordonnée (rotation dérivée de l'identifiant) |
| `cardsDrawn` | une carte piochée part **toujours du paquet** ; les cartes (6 au plus par événement ; une pioche « jusqu'à pouvoir jouer » envoie un événement par carte) volent une à une de la pioche vers le siège (face cachée pour un adversaire, retournée en arrivant dans ma main, où elle prend directement sa place : l'éventail ne contient que les cartes arrivées et se réorganise à chaque arrivée) ; pioche infligée : le siège tremble et clignote |
| +2 / +4 | « +2 » / « +4 » en grand au centre **chez le joueur visé**, sur son siège chez les autres, puis les cartes de pénalité |
| `playerSkipped` | symbole « interdit » : en grand au centre chez le joueur sauté (« Tu passes ton tour »), sur son siège chez les autres (« Zoé passe son tour ») |
| `directionChanged` | une grande flèche circulaire apparaît au centre, fait un tour dans l'ancien sens et se retourne ; la lueur et sa flèche s'éteignent le temps de l'effet, puis repartent dans l'autre sens |
| Joker, +4 (`colorChosen`) | une roue des quatre couleurs au centre, le quart choisi grossit, puis le liseré et la lueur de la défausse prennent la nouvelle couleur |
| `turnChanged` | le siège actif s'illumine et grossit légèrement ; un éclat glisse du siège précédent au suivant |
| `unoCalled` | éclat « UNO ! » sur le siège |
| `unoCaught` | tampon « Contre-UNO » en grand chez la cible, sur son siège chez les autres, puis ses cartes de pénalité |
| `challengeResolved` | verdict (bluff découvert / contestation ratée) en grand chez le joueur pénalisé, sur son siège chez les autres, puis cartes de pénalité vers lui |
| `roundEnded` | projecteur sur le gagnant (la modale de fin de manche attend sa fin) |
| Échange de mains (7/0) | les éventails glissent d'un siège à l'autre (avec les options maison, étape 1.6) |

### 13.3 Son (phase 5)
Effets courts et originaux ou sous licence libre (licence notée dans `client/src/assets/sounds/LICENSES.md`) : poser, piocher, mon tour, UNO, contre-UNO, victoire. **Coupé par défaut** jusqu'à ce que le joueur l'active (politique d'autoplay des navigateurs + respect de l'utilisateur). Volume et état mémorisés en `localStorage`.

## 14. Accessibilité (cible WCAG 2.2 AA)

- **Contraste** : texte ≥ 4,5:1 (≥ 3:1 au-dessus de 24 px), composants et anneaux de focus ≥ 3:1. Vérifié automatiquement avec `@axe-core/playwright` sur chaque écran et chaque scénario de la galerie.
- **Jamais la couleur seule** : symbole de forme + libellé texte pour chaque couleur de jeu (§11.4).
- **Clavier complet** :
  - `Tab` parcourt les zones (main, piles, actions, adversaires) ; dans la main, `←`/`→` déplacent le focus (tabindex mobile via `ListKeyManager`), `Entrée`/`Espace` jouent la carte ;
  - raccourcis actifs uniquement quand la table a le focus (jamais dans un champ de saisie) et désactivables dans les réglages : `P` piocher, `U` UNO, `1`-`4` couleur dans le sélecteur, `?` aide ;
  - focus visible partout (anneau cyan 3 px + décalage), jamais supprimé.
- **Lecteurs d'écran** : `lang="fr"` ; chaque carte a un libellé (« +2 rouge triangle, jouable ») ; `LiveAnnouncer` annonce les événements en `polite` (« Léa pose un 7 bleu ») et « À toi de jouer » en `assertive` ; les compteurs (cartes des adversaires, pioche) sont lisibles.
- **Modales** : focus piégé, `Échap` pour fermer quand c'est permis, focus rendu à l'élément d'origine.
- Après avoir joué une carte, le focus va à la carte voisine (pas de perte de focus en haut de page).
- Zoom texte 200 % sans perte d'information ; mise en page utilisable à 320 px de large.
- Aucun contenu qui clignote plus de 3 fois par seconde.
- Minuteur de tour : le réglage « Désactivé » existe (critère 2.2.1, temps réglable).

## 15. Sécurité (récapitulatif, voir aussi CLAUDE.md)

| Menace | Contre-mesure | Test |
|---|---|---|
| Lire les cartes des autres dans l'onglet Réseau | projection `PlayerView` par joueur, `CardId` aléatoires | test anti-fuite sur chaque vue et chaque événement, pour chaque joueur, pendant des milliers de parties simulées |
| Jouer hors de son tour, une carte qu'on n'a pas, un coup illégal | validation serveur de chaque `PlayerAction` | tests unitaires par règle + tests de contrat |
| Prédire le mélange ou deviner un jeton | libsodium pour les jetons, les codes et le mélange | revue de code : aucun `mt19937` hors tests |
| Force brute des codes de salon | alphabet de 31 caractères sur 6 positions (≈ 887 millions de codes) + limite de `room.join` par IP | test d'intégration du rate limiting |
| Flood / déni de service applicatif | taille max, seau à jetons, fermeture après abus, expiration des salons | tests d'intégration |
| Injection XSS via pseudo | validation serveur + interpolation Angular (échappée) + interdiction de `innerHTML` + CSP | test E2E avec pseudo `<img src=x onerror=alert(1)>` |
| WebSocket appelé depuis un autre site | vérification de l'`Origin` | test d'intégration |
| Interception réseau | HTTPS/WSS obligatoire en prod (Caddy), HSTS | vérification manuelle au déploiement |
| Bugs mémoire C++ | RAII, ASan/UBSan en CI, clang-tidy, warnings en erreur | CI |
| Dépendances compromises | versions figées (baseline vcpkg, `package-lock.json`), `npm audit` en CI | CI |

## 16. Performance

- Client : bundle initial < 250 Ko gzip (routes chargées à la demande pour la table et la galerie) ; Lighthouse mobile ≥ 90 en performance et ≥ 95 en accessibilité sur l'accueil.
- 60 images/s sur un mobile milieu de gamme pendant une pose de carte : `backdrop-filter` réservé aux grands panneaux (jamais sur chaque carte), `will-change` seulement pendant une animation, pas plus de ~40 éléments animés en même temps.
- Signals + OnPush + `track` dans les `@for` : une pose de carte ne re-rend que les composants concernés.
- Serveur : un `game.update` traité en < 1 ms côté moteur ; 200 salons simultanés sans dégradation (test de charge simple en phase 6, optionnel).

## 17. Tests et qualité

| Niveau | Outil | Contenu attendu |
|---|---|---|
| Moteur C++ | Catch2 v3 + `SeededRandomSource` | chaque règle et chaque option maison ; un test nommé d'après la règle (`"WildDrawFour is illegal when holding a card of the current color"`) |
| Simulation | Catch2 | 10 000 parties aléatoires (2 à 10 joueurs, options aléatoires, coups légaux au hasard) ; invariants vérifiés après chaque action : 108 cartes au total, `CardId` uniques, un seul joueur courant valide, scores ≥ 0, la manche se termine (garde-fou sur le nombre de tours) ; **test anti-fuite** sur toutes les projections |
| Relecture | Catch2 | graine + liste d'actions → état final identique (déterminisme) |
| Contrat | Catch2 + TypeScript | exemples valides/invalides du protocole (§8.1) |
| App/serveur | Catch2 + `ManualScheduler` | salons, sessions, reconnexion, timers, rate limiting, Origin — sans vrai réseau quand c'est possible |
| Client | Vitest | `GameStore` (file d'animations, `stateVersion`, erreurs), `WebSocketTransport` (backoff), composants de présentation clés |
| E2E | Playwright (`e2e/`, ADR 0023) | une pile par test (serveur lancé avec `UNO_TEST_SEED` + client construit), un contexte de navigateur par joueur. Couverts : salon, Prêt, Lancer, carte posée, pioche guidée, Joker, +4 contesté (à raison, à tort, accepté), UNO, contre-UNO, UNO obligatoire, rechargement, forfait, fin de manche unique. À écrire : partie à 4 avec cumul, code invalide, salon plein, partie jouée **uniquement au clavier**, scan axe sur chaque écran, captures de référence en 375, 768 et 1440 px |

Qualité : clang-format, clang-tidy, ESLint, Prettier. Aucune étape n'est terminée avec un test rouge ou un warning.

## 18. CI/CD et déploiement

**CI (GitHub Actions, `.github/workflows/ci.yml`)**, sur chaque PR :
- `server-linux` : GCC et Clang, preset `debug-asan`, build + ctest ; cache binaire vcpkg.
- `server-windows` : MSVC, preset `dev` (vérifie la compatibilité avec les postes de l'équipe).
- `client` : `npm ci`, lint, tests Vitest, build de prod, `npm audit --audit-level=high`.
- **Déclencheurs** : `pull_request` vers `main` et lancement manuel, jamais un push de branche ; PR en brouillon ignorées ; `concurrency` par PR avec annulation. Un job `changes` (script `git diff`) décide quels jobs tournent (`if:` au niveau des jobs, jamais `paths:` au niveau du workflow : un check requis jamais déclenché bloquerait la fusion) : `docs/` seul → rien ; `client/` → client + e2e ; `server/` → serveur + e2e ; `protocol/`, `.github/` et le reste → tout. Permissions `contents: read`, aucun secret, jamais `pull_request_target`, actions épinglées par SHA.
- `e2e` : après les deux précédents ; publie le rapport Playwright en artefact en cas d'échec.

**Déploiement (phase 6)**
- `deploy/Dockerfile.server` multi-étapes : build (Ubuntu + vcpkg, preset `release`) → exécution (Debian slim, utilisateur non-root, seulement le binaire).
- Client : `ng build` → fichiers statiques servis par Caddy.
- `deploy/Caddyfile` : HTTPS automatique ; `/ws*` → `reverse_proxy server:9001` ; le reste → fichiers statiques avec `try_files {path} /index.html` ; en-têtes `Strict-Transport-Security`, `X-Content-Type-Options: nosniff`, `Referrer-Policy: strict-origin-when-cross-origin`, `Permissions-Policy` minimale, **CSP stricte** (`default-src 'self'; connect-src 'self' wss://<domaine>; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; frame-ancestors 'none'`), en s'appuyant sur l'option `autoCsp` d'Angular pour les scripts, et en vérifiant qu'aucune violation n'apparaît dans la console.
- `deploy/docker-compose.yml` : services `caddy` et `server`, `.env` (non versionné) + `.env.example`.
- `docs/DEPLOY.md` : procédure pas à pas sur un VPS Linux avec Docker et un nom de domaine.

## 19. Phases de réalisation et définition du « terminé »

Chaque phase = une ou plusieurs branches + PR. Détail des étapes dans `docs/PROGRESS.md`. Ne commence jamais une phase avant la validation humaine de la précédente.

| Phase | Contenu | Terminé quand |
|---|---|---|
| **0 — Fondations et contrat** | monorepo, CMake/vcpkg/presets, Angular, lint/format, CI squelette, **protocole v1** (schémas + exemples + types TS générés), commandes dans CLAUDE.md | tout compile sur Windows (MSVC) et en CI Linux, clangd voit `compile_commands.json`, 1 test par côté passe, types générés à jour |
| **1 — Cœur du jeu** | §3 à §5 et §7 en TDD | toutes les règles et options testées, simulation de 10 000 parties verte, test anti-fuite vert, ASan/UBSan propres |
| **2 — Serveur réseau** | §9 | tests d'intégration verts ; une partie peut être jouée avec un petit script client de test |
| **3 — Design system et UI** | §10 à §14 sur `FixtureTransport` | galerie complète, tous les scénarios capturés en 3 tailles et relus visuellement, axe sans erreur, navigation clavier complète |
| **4 — Intégration** | `WebSocketTransport` branché, parcours complets | E2E verts (2 et 4 joueurs, reconnexion, clavier) |
| **5 — Bots et finitions** | bots (`BotStrategy` : `RandomBot`, `GreedyBot` qui garde ses Jokers et vide ses grosses cartes ; un bot ne voit que sa `PlayerView` et agit via les mêmes `PlayerAction` qu'un humain, donc aucun cas particulier dans le moteur), son, réactions, audit `web-design-guidelines` + Lighthouse, corrections | audits sans problème majeur, objectifs §16 atteints |
| **6 — Déploiement** | §18 | site accessible en HTTPS, partie jouée entre deux machines réelles |

## 20. Hors périmètre v1

Comptes et mots de passe, chat libre, mode spectateur, classement persistant, application mobile native, autres langues que le français (mais tous les textes de l'UI sont centralisés pour permettre une traduction plus tard).

