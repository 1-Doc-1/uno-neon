# UNO Néon — règles permanentes du projet

Jeu de UNO multijoueur en ligne. **Serveur autoritaire en C++23** (WebSocket) + **client Angular 22** (navigateur).
Projet personnel : le code doit être exemplaire ET compréhensible.

- Spécification complète : `docs/SPEC.md` (source de vérité). Elle est longue : lis les sections utiles à l'étape en cours plutôt que tout le fichier.
- Avancement et décisions : @docs/PROGRESS.md (importé automatiquement ; mets-le à jour à la fin de chaque étape)
- Décisions d'architecture : `docs/adr/NNNN-titre.md` (une ADR courte par choix structurant)

## Méthode de travail (obligatoire)

1. Au début d'une session : lis `docs/PROGRESS.md`, identifie le lot et l'étape en cours. Suis l'ordre de PROGRESS.md (visé : un MVP jouable au plus tôt).
2. Mode autonome par défaut : pour chaque lot d'étapes, un plan de 10 lignes maximum dans ta réponse (fichiers, tests, risques), puis exécution directe sans attendre de validation. Arrête-toi seulement si tu t'écartes de la SPEC ou d'une ADR, si une décision n'est pas couverte, ou après 2 échecs sur le même problème.
3. Travaille par petits incréments vérifiables : test rouge → code → test vert → refactor → commit.
4. Une étape n'est « terminée » que si : build OK, tests OK, lint OK, et `docs/PROGRESS.md` mis à jour. Pour toute étape qui touche `server/` : lancer `scripts/linux-check.sh` (WSL) avant de pousser (voir `docs/SETUP.md`, étape 10).
5. Si la SPEC est ambiguë ou contradictoire : pose la question, ne devine pas. Si tu t'écartes de la SPEC, écris une ADR.
6. Un commit par étape, une PR par lot de 2 ou 3 étapes. Une seule revue (`/code-review`) en fin de phase, pas à chaque PR.
7. Fin de lot : résumé de 10 lignes maximum : ce qui est fait, le **pourquoi** des choix d'architecture en 2 phrases, ce qui reste.
8. Ne modifie jamais `docs/SPEC.md` sans demande explicite.

## Commandes (garder à jour)

Serveur : depuis `server/`, dans un terminal où `dev64` a été lancé. Client : depuis `client/`.

| Action | Commande |
|---|---|
| Configurer le serveur | `cmake --preset dev` (autres presets : `debug-asan`, `release`) |
| Compiler le serveur | `cmake --build --preset dev` |
| Tests C++ | `ctest --preset dev` (sortie détaillée en cas d'échec déjà activée) |
| Simulation massive (parties aléatoires, invariants) | `UNO_SIMULATION_GAMES=10000 build/release/tests/uno_tests "[simulation]"` depuis `server/` après `cmake --preset release` et `cmake --build --preset release` (défaut : 20 parties avec `ctest`, 100 dans le CI des PR et `linux-check.sh` ; 10 000 chaque semaine via `simulation.yml`) |
| Lancer le serveur | `build/dev/uno_server` (variables : `UNO_PORT`, défaut 9001 ; `UNO_ALLOWED_ORIGINS`, défaut les origines du serveur de dev Angular ; `UNO_LOG_LEVEL` ; `UNO_TRUSTED_PROXY`, défaut faux) |
| Formater le C++ | `git ls-files '*.cpp' '*.hpp' '*.cpp.in' | xargs clang-format -i` (depuis `server/`) |
| Vérifier le C++ (clang-tidy) | `git ls-files '*.cpp' | xargs clang-tidy -p build/dev --quiet` (depuis `server/`, comme le CI) |
| Jouer / développer (serveur + client ensemble) | `./scripts/dev.ps1` depuis la racine (`-Build` pour recompiler le serveur ; ouvrir deux onglets sur http://localhost:4200) |
| Client : dev (proxy `/ws` → `localhost:9001`) | `npm start` |
| Client : tests (unitaires + contrat du protocole) | `npm test` (mode watch : `npm run test:watch`) |
| Client : lint | `npm run lint` |
| Client : formatage (client + `protocol/`) | `npm run format` / `npm run format:check` |
| Client : build de production | `npm run build` |
| Régénérer les types du protocole | `npm run protocol:gen` (après toute modification de `protocol/schema`) |
| E2E | Prérequis : `cmake --preset e2e && cmake --build --preset e2e` (depuis `server/`, binaire AVEC crochets de test, jamais en production) et `npm run build` (depuis `client/`). Puis `npx playwright test` depuis `e2e/` (`npm ci` et `npx playwright install chromium` la première fois) |
| Trouver des graines pour les E2E | `npm run seeds -- <première> <nombre>` (depuis `e2e/`) |
| Vérifier le serveur sous Linux (WSL, avant push serveur) | `wsl -d Ubuntu-24.04 -- bash scripts/linux-check.sh` (depuis la racine du repo ; installation unique : `scripts/linux-setup.sh`, voir `docs/SETUP.md` étape 10) |

Environnement : Windows 10/11, Build Tools Visual Studio 2026, PowerShell initialisé avec la fonction `dev64` (MSVC **x64**, CMake, Ninja dans le PATH ; `VCPKG_ROOT=C:\dev\vcpkg`, notre clone, pas le vcpkg intégré à VS). Triplet vcpkg : `x64-windows`. Générateur CMake : **Ninja** (nécessaire pour `compile_commands.json` → clangd). Le CI (`.github/workflows/ci.yml`) tourne sous Linux (GCC + Clang, sanitizers : la référence) et Windows (MSVC). clang-format et clang-tidy : version majeure 23 sur les postes comme en CI.

## Standards de code

**Général** : SOLID, DRY, KISS. Noms explicites. Identifiants et commits en anglais, UI et docs en français.
Commentaires uniquement pour la logique métier complexe (règles UNO non évidentes) — le code doit se lire seul.
Pas de code mort, pas de TODO sans ticket/étape dans PROGRESS.md.

**C++ (server/)**
- C++23, CMake ≥ 3.28, dépendances via **vcpkg en mode manifest** (`vcpkg.json` + baseline figée).
- RAII partout, zéro `new`/`delete` nus, `std::unique_ptr` si possession, références/`std::span` sinon.
- `const` par défaut, `[[nodiscard]]` sur les fonctions dont le retour compte, `enum class`.
- Ensembles fermés (actions, états, événements) : `std::variant` + `std::visit` (exhaustivité vérifiée à la compilation).
- Erreurs métier : `std::expected<T, DomainError>` — pas d'exceptions pour le flux normal.
- La bibliothèque `uno_core` ne dépend d'AUCUNE bibliothèque réseau, d'aucune horloge ni d'aucun aléatoire global : tout est injecté (interfaces `RandomSource`, `Clock`).
- Warnings au max (`/W4` MSVC, `-Wall -Wextra -Wpedantic -Wconversion` GCC/Clang), traités comme erreurs. clang-format + clang-tidy.
- Sanitizers (ASan + UBSan) activés dans le preset `debug-asan` et dans le CI Linux.

**TypeScript / Angular (client/)**
- `strict: true`, jamais de `any` (utiliser `unknown` + garde de type), pas de `!` non justifié.
- Composants standalone, **zoneless**, **signals** pour l'état, `OnPush` (mode par défaut depuis Angular 22 : ne pas l'écrire), control flow `@if/@for`.
- Séparation conteneurs (injectent le store) / composants de présentation (inputs/outputs uniquement).
- Animations : CSS natif + `animate.enter` / `animate.leave` (le package `@angular/animations` est déprécié).
- Jamais de `innerHTML` / `bypassSecurityTrust*` avec une donnée venant d'un joueur.
- Styles : SCSS + design tokens en CSS custom properties (`client/src/styles/tokens.scss`). Aucune couleur/ombre codée en dur dans un composant.

## Dépôt public (non négociable)

Le dépôt est **public** : tout ce qui est poussé est visible immédiatement et pour toujours (l'historique git et les
caches y survivent même après suppression). Aucun secret, aucun jeton, aucune donnée personnelle (adresse, chemin
absolu, nom réel) dans un commit, un test, un exemple, un log ou une capture. Si la protection contre les secrets
(GitHub push protection, gitleaks) bloque un push, **arrête-toi et préviens-moi** : ne la contourne jamais (pas de
`--no-verify`, pas de lien « autoriser le secret », pas de réécriture d'historique pour la dissimuler).
Les faux positifs connus sont dans `.gitleaksignore`, chacun avec la raison.

## Invariants de sécurité (non négociables)

1. Le serveur est la seule autorité : le client envoie des **intentions**, le serveur valide TOUT (tour, possession de la carte, légalité, phase).
2. Un joueur ne reçoit JAMAIS : les cartes des autres, l'ordre de la pioche, la graine aléatoire. Seule la projection `PlayerView` est sérialisée. Un test automatisé le vérifie.
3. Toute entrée réseau est hostile : taille max, schéma strict, type inconnu = rejet, rate limiting, vérification de l'`Origin`.
4. Jetons de session et codes de salon générés par un générateur cryptographique, jamais par `std::mt19937`.
5. Aucun secret dans le repo (`.env` ignoré, `.env.example` fourni).

## Git (projet personnel)

- `main` protégée : on n'y pousse jamais directement, jamais de `--force`.
- Une branche et une pull request par lot de 2 ou 3 étapes : `feat/core-deck`, `feat/server-rooms`, `fix/…`, `chore/…`.
- **Un seul push par lot**, à la fin, une fois build, tests et lint verts en local (et `scripts/linux-check.sh` pour `server/`). Le CI (déclenché par la PR seulement, jamais par un push de branche) confirme ; il n'est pas un banc d'essai.
- Conventional Commits (`feat(core): add wild draw four legality check`).
- Une pull request fusionnée seulement si le CI est vert ; `/code-review` (plugin) une fois en fin de phase ; pas de relecture humaine — je travaille seul.
- Ne jamais committer : `build/`, `node_modules/`, `.env`, `.claude/settings.local.json`.

## Outils Claude Code disponibles dans ce repo

- **clangd-lsp** / **typescript-lsp** : lis les diagnostics après chaque édition et corrige-les avant de continuer.
- **Skills** : `angular-developer` (code Angular moderne), `modern-cpp` (C++ moderne et sûr), `web-design-guidelines` (audit UI), `playwright-cli` (navigateur, captures, tests).
- **Plugins** : `frontend-design` (qualité visuelle), `feature-dev` (grosses fonctionnalités), `code-review` (en fin de phase).
- **MCP angular-cli** : `get_best_practices` et la documentation officielle avant d'écrire du code Angular non trivial.
- Vérification visuelle : après toute modification d'UI, prends des captures avec playwright-cli en 375 px, 768 px et 1440 px et regarde-les avant de dire que c'est terminé.
