# Avancement

> Tenu à jour par Claude Code à la fin de chaque étape. Les cases cochées = build + tests + lint verts.
> Format d'une entrée de journal : `AAAA-MM-JJ — phase.étape — résumé — branche/PR`.

## Phase en cours : 0 — Fondations

### Phase 0 — Fondations et contrat
- [x] 0.1 Arborescence du monorepo, `.gitignore`, `.editorconfig`, README
- [ ] 0.2 Serveur : CMake + presets Ninja + vcpkg manifest + projet vide qui compile + 1 test Catch2
- [ ] 0.3 Vérifier que `compile_commands.json` est généré et que clangd le trouve
- [ ] 0.4 Client : `ng new` Angular 22 (zoneless, SCSS, Vitest) + lint + 1 test
- [ ] 0.5 Protocole v1 : schémas JSON + exemples de messages + types TS générés
- [ ] 0.6 CI GitHub Actions (build + tests serveur et client)
- [ ] 0.7 Compléter la table des commandes dans CLAUDE.md

### Phase 1 — Cœur du jeu (C++, TDD)
- [ ] 1.1 Cartes, deck de 108 cartes, `RandomSource`
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

## Journal
- 2026-09-30 — 0.1 — arborescence du monorepo, `.editorconfig`, README, modèle d'ADR, BOM retiré de `.gitattributes` — `chore/phase-0-foundations`
