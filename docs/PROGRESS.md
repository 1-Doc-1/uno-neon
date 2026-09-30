# Avancement

> Tenu à jour par Claude Code à la fin de chaque étape. Les cases cochées = build + tests + lint verts.
> Format d'une entrée de journal : `AAAA-MM-JJ — phase.étape — résumé — branche/PR`.

## Phase en cours : 1 — Cœur du jeu (phase 0 terminée : PR #2 fusionnée)

### Phase 0 — Fondations et contrat
- [x] 0.1 Arborescence du monorepo, `.gitignore`, `.editorconfig`, README
- [x] 0.2 Serveur : CMake + presets Ninja + vcpkg manifest + projet vide qui compile + 1 test Catch2
- [x] 0.3 Vérifier que `compile_commands.json` est généré et que clangd le trouve
- [x] 0.4 Client : `ng new` Angular 22 (zoneless, SCSS, Vitest) + lint + 1 test
- [x] 0.5 Protocole v1 : schémas JSON + exemples de messages + types TS générés
- [x] 0.6 CI GitHub Actions (build + tests serveur et client)
- [x] 0.7 Compléter la table des commandes dans CLAUDE.md

### Phase 1 — Cœur du jeu (C++, TDD)
- [x] 1.1 Cartes, deck de 108 cartes, `RandomSource`
- [ ] 1.2 État de partie, tour, sens, pioche/défausse, remélange
- [ ] 1.3 Règles officielles : jouabilité, effets, cartes Joker, première carte retournée
- [ ] 1.4 UNO : annonce, contre-UNO, pénalités
- [ ] 1.5 Fin de manche, score, fin de partie (500 pts ou manche unique)
- [ ] 1.6 Options maison (politiques injectables)
- [ ] 1.7 Projection `PlayerView` + test anti-fuite
- [ ] 1.8 Simulation aléatoire massive (invariants)

### Phase 2 — Serveur réseau
- [ ] 2.1 Serveur uWebSockets, `/health`, vérification Origin, limites de taille
- [ ] 2.2 Codec JSON + validation stricte + erreurs typées
- [ ] 2.3 Sessions, salons, codes, hôte, paramètres
- [ ] 2.4 Démarrage de partie, diffusion des vues et des événements
- [ ] 2.5 Timers (tour, reconnexion, salons inactifs), rate limiting
- [ ] 2.6 Tests d'intégration (clients WebSocket de test)

### Phase 3 — Design system et UI sur données simulées (Angular)
- [ ] 3.1 Tokens, typographies, fond, surfaces « verre », glow
- [ ] 3.2 Composant carte (toutes les cartes, états, daltonisme)
- [ ] 3.3 Écrans Accueil et Salon
- [ ] 3.4 Table de jeu (main, adversaires, piles, indicateurs, sélecteur de couleur)
- [ ] 3.5 Animations et `prefers-reduced-motion`
- [ ] 3.6 Galerie `/dev/gallery` avec tous les états
- [ ] 3.7 Responsive 360 → 1920 px, accessibilité clavier et lecteur d'écran

### Phase 4 — Intégration client ↔ serveur
- [ ] 4.1 `GameSocket` (reconnexion, backoff, messages typés)
- [ ] 4.2 `GameStore` branché sur le serveur, réconciliation par `stateVersion`
- [ ] 4.3 Parcours complets : créer, rejoindre, jouer, gagner, revanche
- [ ] 4.4 E2E Playwright : partie complète à 2 puis 4 joueurs, reconnexion

### Phase 5 — Bots, finitions, audits
- [ ] 5.1 Bots (stratégies Aléatoire et Glouton)
- [ ] 5.2 Sons, réactions rapides, écran de fin
- [ ] 5.3 Audit web-design-guidelines + axe + performance, corrections

### Phase 6 — Déploiement
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

## Journal
- 2026-09-30 — 0.1 — arborescence du monorepo, `.editorconfig`, README, modèle d'ADR, BOM retiré de `.gitattributes` — `chore/phase-0-foundations`
- 2026-09-30 — 0.2 — CMake presets `dev`/`debug-asan`/`release` (Ninja), vcpkg manifest (5 dépendances, baseline figée), `uno_core`/`uno_app`/`uno_net`/`uno_server`, 6 tests Catch2, clang-format + clang-tidy propres — `chore/phase-0-foundations`
- 2026-09-30 — 0.3 — `.clangd` à la racine pointant vers `server/build/dev` ; `clangd --check` résout les en-têtes du projet et de vcpkg sans erreur — `chore/phase-0-foundations`
- 2026-09-30 — 0.4 — client Angular 22 (zoneless, SCSS, Vitest, `strict` + `strictTemplates`), angular-eslint (`no-explicit-any`, `no-non-null-assertion` en erreur), Prettier, proxy `/ws` → `localhost:9001`, shell `<router-outlet />` + 2 tests — `chore/phase-0-foundations`
- 2026-09-30 — 0.5 — protocole v1 : 7 schémas JSON Schema 2020-12, 39 exemples valides et 18 invalides, `npm run protocol:gen` (json-schema-to-typescript), test de contrat Vitest + ajv (63 tests verts), `@types/node` ajouté aux specs (validé), écarts SPEC tracés en ADR 0007 — `chore/phase-0-foundations`
- 2026-09-30 — 0.6 — CI GitHub Actions : `server-linux` (GCC 14 + Clang 20, `debug-asan`, clang-format/clang-tidy 23 via apt.llvm.org), `server-windows` (MSVC, `dev`), `client` ; actions épinglées par SHA, cache binaire vcpkg ; corrigés en route : runtime des sanitizers Clang (`libclang-rt-20-dev`), `getenv` justifié pour clang-tidy, chemin du cache sans `..` — `chore/phase-0-foundations` / PR #2
- 2026-09-30 — 0.7 — table des commandes de `CLAUDE.md` complétée et vérifiée (serveur, formatage/lint C++, client, protocole) ; `OnPush` par défaut depuis Angular 22 — `chore/phase-0-foundations` / PR #2
- 2026-09-30 — 1.1 — `Card`/`CardId`/`Color`/`Rank`, interface `RandomSource` + Fisher-Yates `shuffle`, `createStandardDeck` (108 cartes, identifiants permutés aléatoirement), `SeededRandomSource` dans la bibliothèque `uno_testing` (jamais liée au binaire), 20 nouveaux tests dont la valeur de référence de `std::mt19937_64` ([rand.predef]) ; ADR 0008 (déterminisme portable) et 0009 (modèle de carte), SPEC §2/§7.3 alignées ; commande clang-tidy de `CLAUDE.md` alignée sur le CI — `feat/core-deck`
