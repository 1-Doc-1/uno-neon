# Prompts à coller dans Claude Code

> Toujours en **plan mode** (`Shift+Tab` jusqu'à `⏸ plan mode on`), après un `/clear`, sur une branche dédiée.
> Les détails sont dans `docs/SPEC.md` : les prompts disent QUOI faire maintenant et comment travailler, la SPEC dit tout le reste.

---

## Prompt 0 — Démarrage du projet (le gros prompt)

```text
Tu démarres le projet « UNO Néon » avec notre groupe d'étudiants en développement d'applications.

CONTEXTE
- Lis d'abord CLAUDE.md et docs/PROGRESS.md, puis dans docs/SPEC.md les sections 1, 2, 6, 8, 9.1, 10.1, 10.2, 17, 18 et 19.
- Jeu de UNO multijoueur en ligne : serveur autoritaire C++23 (uWebSockets) + client Angular 22, communication WebSocket JSON sur /ws.
- Poste : Windows, Build Tools Visual Studio 2026, environnement MSVC x64 chargé par `dev64` (cl, CMake, Ninja dans le PATH), vcpkg dans $env:VCPKG_ROOT (C:\dev\vcpkg, triplet x64-windows), clangd (LLVM), Node 26, Angular CLI 22, GitHub CLI.
- Outils Claude Code disponibles : plugins clangd-lsp, typescript-lsp, frontend-design, feature-dev, code-review ; skills angular-developer, modern-cpp, web-design-guidelines, playwright-cli ; MCP angular-cli.

OBJECTIF DE CETTE SESSION
Réaliser la PHASE 0 « Fondations et contrat » (étapes 0.1 à 0.7 de docs/PROGRESS.md), et rien d'autre : aucune règle de jeu, aucun écran.

AVANT D'ÉCRIRE LE MOINDRE FICHIER
1. Vérifie les versions installées (git, cmake, ninja, cl, clangd, node, npm, ng, gh) et signale tout ce qui manque ou est trop ancien.
2. Relis la SPEC d'un œil critique : liste les incohérences, les risques ou les choix que tu ferais autrement, avec tes arguments. Je trancherai.
3. Propose un plan détaillé, étape par étape. Pour chaque étape : les fichiers créés, les commandes, comment on vérifie que ça marche et le message de commit prévu.
4. Liste les décisions qui méritent une ADR (au minimum : monorepo, Ninja + presets CMake, vcpkg en mode manifest, JSON Schema comme source du protocole, std::variant pour les états).
Puis attends ma validation.

PENDANT L'EXÉCUTION
- Une étape à la fois. À la fin de chaque étape : build + tests + lint verts, docs/PROGRESS.md mis à jour, résumé court avec le POURQUOI des choix (je suis étudiant, je dois pouvoir les expliquer à l'oral), message de commit proposé. Puis arrête-toi et attends mon feu vert.
- Serveur : trois bibliothèques vides mais compilables (uno_core, uno_app, uno_net) + exécutable uno_server + un test Catch2. Presets `dev`, `debug-asan`, `release` avec Ninja et CMAKE_EXPORT_COMPILE_COMMANDS=ON. Fais en sorte que clangd trouve compile_commands.json (fichier .clangd à la racine qui pointe vers le dossier de build ; pas de lien symbolique, on est sous Windows).
- Client : crée l'app avec `npx @angular/cli@latest new client --ai-config=claude --style=scss --ssr=false --routing --skip-git` (vérifie les options avec --help si l'une a changé), configure le proxy de dev /ws → localhost:9001, ESLint, Prettier, un test Vitest qui passe. Consulte les bonnes pratiques via le MCP angular-cli.
- Protocole v1 (§8) : schémas JSON Schema 2020-12 dans protocol/schema, au moins un exemple valide par message et quelques exemples invalides dans protocol/examples, script `npm run protocol:gen` qui génère les types TS dans client/src/app/protocol/generated.
- CI GitHub Actions (§18) : jobs server-linux, server-windows et client. Le job e2e viendra en phase 4.
- Complète la table des commandes dans CLAUDE.md.

INTERDIT
- Commencer la phase 1.
- Pousser sur main ou utiliser --force.
- Ajouter une dépendance non prévue sans me demander.
- Laisser un warning ou un test rouge.
```

---

## Prompt de phase (modèle réutilisable pour les phases 1 à 6)

```text
Phase <N> — <titre>, étape <N.x> de docs/PROGRESS.md.
Lis dans docs/SPEC.md les sections <liste> avant de commencer.
Propose un plan : fichiers touchés, tests écrits EN PREMIER (noms des cas), patterns utilisés et pourquoi, risques. Attends ma validation.
Ensuite : TDD (test rouge → code → test vert → refactor), diagnostics clangd/TypeScript à zéro, build + tests + lint verts, PROGRESS.md à jour, résumé pédagogique, message de commit proposé. Arrête-toi à la fin de l'étape.
```

### Sections à citer par phase
| Phase | Sections de la SPEC |
|---|---|
| 1 — Cœur du jeu | 2, 3, 4, 5, 7, 15 (lignes moteur), 17 (moteur, simulation, relecture) |
| 2 — Serveur réseau | 7.4, 8, 9, 15, 17 (contrat, app/serveur) |
| 3 — Design system et UI | 1, 10, 11, 12, 13, 14, 16 |
| 4 — Intégration | 8.4, 10.3, 10.4, 12, 17 (client, E2E) |
| 5 — Bots et finitions | 13.3, 14, 16, 19 (phase 5) |
| 6 — Déploiement | 15, 18 |

---

## Compléments par phase (à ajouter au modèle)

**Phase 1 — Cœur du jeu**

Pour les grosses étapes (1.3 et 1.6), tu peux passer par la commande du plugin feature-dev plutôt que par le modèle : tape `/feature` dans Claude Code, choisis la commande proposée et donne-lui la description de l'étape. Elle enchaîne exploration, architecture, implémentation et relecture.
```text
Utilise le skill modern-cpp.
Commence par une ADR « std::variant plutôt que des classes virtuelles pour TurnPhase et PlayerAction ».
La simulation (1.8) doit tourner sous le preset debug-asan sans aucune alerte.
```

**Phase 2 — Serveur réseau**
```text
Un seul thread, une seule boucle uWebSockets (§9.2). Aucun std::mt19937 hors des tests : vérifie-le avec une recherche dans le code à la fin de la phase.
Écris les tests d'intégration du rate limiting, de la vérification d'Origin et de la reconnexion avant de les implémenter.
```

**Phase 3 — Design system et UI (sur FixtureTransport)**
```text
Direction artistique : §11 « Néon Crépuscule ». Futuriste, lumineux, coloré, jamais noir, jamais sobre, et toujours lisible.
Utilise les skills frontend-design et angular-developer, et le MCP angular-cli (get_best_practices) avant d'écrire du code Angular.
Ordre : tokens et fond (3.1) → composant carte (3.2) → composants UI → Accueil et Salon (3.3) → table (3.4) → animations (3.5) → galerie (3.6) → responsive et accessibilité (3.7).
Boucle de vérification visuelle OBLIGATOIRE à chaque composant ou écran : lance le serveur de dev, prends des captures avec playwright-cli en 375, 768 et 1440 px, REGARDE-LES, critique-les toi-même par rapport à §11 (contraste, hiérarchie, halos qui ont un sens, pas de texte directement sur le fond), corrige, recommence. Montre-moi les captures finales.
À la fin de la phase : audit avec le skill web-design-guidelines, scan axe, et liste des corrections.
```

**Phase 4 — Intégration**
```text
Remplace FixtureTransport par WebSocketTransport sans modifier les composants (si tu dois en modifier un, c'est que l'abstraction est mauvaise : signale-le).
Écris les scénarios E2E Playwright de §17 : plusieurs contextes navigateur = plusieurs joueurs, serveur lancé avec UNO_TEST_SEED.
```

**Phase 5 — Bots et finitions**
```text
Bots via BotStrategy : ils ne voient que leur PlayerView et passent par les mêmes PlayerAction qu'un humain.
Puis audits (web-design-guidelines, axe, Lighthouse) et corrections, par ordre de gravité.
```

**Phase 6 — Déploiement**
```text
Dockerfile multi-étapes, Caddyfile avec les en-têtes de §18, docker-compose, docs/DEPLOY.md pas à pas pour un VPS.
Vérifie qu'aucune violation CSP n'apparaît dans la console du navigateur.
```

---

## Prompts utiles au quotidien

**Reprendre après une pause**
```text
Où en est-on ? Résume l'état à partir de docs/PROGRESS.md et de `git log --oneline -15`, puis propose la prochaine étape.
```

**Comprendre (pour l'oral)**
```text
Explique-moi <fichier / classe / choix> comme si je devais le défendre devant un jury : le problème résolu, l'alternative écartée et pourquoi, et comment c'est testé. Ne modifie rien.
```

**Corriger un design qui ne va pas** (colle une capture avec Alt+V)
```text
Cette capture ne respecte pas §11 : <ce qui ne va pas>. Propose 2 pistes de correction, avec avant/après en captures playwright-cli, avant de modifier quoi que ce soit.
```

**Déboguer sans solution toute faite**
```text
J'ai cette erreur : <erreur>. Ne corrige pas tout de suite : donne-moi des hypothèses classées par probabilité et comment isoler chacune. Je veux comprendre.
```

**Avant de fusionner une PR**

Une fois la PR ouverte sur GitHub, tape `/code-review` dans Claude Code (commande du plugin : plusieurs agents relisent la PR). Puis colle :
```text
Vérifie les invariants de sécurité de CLAUDE.md un par un et dis-moi lesquels sont concernés par cette PR, avec les preuves (fichiers, tests).
```
