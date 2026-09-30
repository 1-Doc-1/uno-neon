# Mise en route — Claude Code sur UNO Néon (Windows)

> À suivre dans l'ordre. Les étapes 1 et 3 se font **une fois par PC** (chaque membre du groupe). Les étapes 2 et 4 à 7 se font **une seule fois pour le projet** (par la personne qui crée le repo). Les coéquipiers font ensuite l'étape 8.

## Ce que tu vas installer, en 30 secondes

| Élément | C'est quoi | Où ça vit |
|---|---|---|
| **Claude Code** | l'agent qui lit ton code, lance des commandes et modifie des fichiers depuis un terminal | sur ton PC |
| **CLAUDE.md** | les règles permanentes du repo, lues automatiquement à chaque session | racine du repo (versionné) |
| **docs/SPEC.md** | la spécification complète (le « giga prompt »), lue à la demande | repo (versionné) |
| **Plugins** | paquets installables (skills, agents, serveurs de langage…) via `/plugin` | activés dans `.claude/settings.json` (versionné), installés sur chaque PC |
| **Skills** | instructions spécialisées que Claude charge quand la tâche correspond | `.claude/skills/` (versionné) |
| **MCP** | serveurs d'outils (ici : la CLI Angular) que Claude peut appeler | `.mcp.json` (versionné) |

**Pourquoi une SPEC dans un fichier plutôt qu'un énorme prompt collé dans le chat ?** Un prompt collé disparaît au premier `/clear` ou quand la conversation est résumée ; un fichier reste dans le repo, versionné, identique pour tout le groupe, et Claude peut en relire la bonne section à chaque étape.

---

## Étape 1 — Installer les outils (une fois par PC)

Ouvre **PowerShell** (pas en administrateur) et installe :

```powershell
winget install --id Git.Git -e
winget install --id OpenJS.NodeJS.LTS -e
winget install --id LLVM.LLVM -e          # fournit clangd (serveur de langage C++)
winget install --id GitHub.cli -e
```

Puis :

1. **Compilateur C++** : télécharge « Build Tools for Visual Studio » (dernière version) sur visualstudio.microsoft.com/downloads, et coche la charge de travail **« Développement Desktop en C++ »**. Elle contient MSVC, CMake et Ninja. Ces outils ne sont dans le PATH que dans un terminal « développeur ». Le raccourci « Developer PowerShell for VS » du menu Démarrer configure un compilateur **x86 (32 bits)** par défaut : on utilisera plutôt la commande `dev64` du point 8.
2. **LLVM dans le PATH** : si `clangd --version` ne répond pas, ajoute `C:\Program Files\LLVM\bin` au PATH (Paramètres → Système → Informations système → Paramètres avancés → Variables d'environnement).
3. **vcpkg** (gestionnaire de paquets C++) :
   ```powershell
   git clone https://github.com/microsoft/vcpkg C:\dev\vcpkg
   C:\dev\vcpkg\bootstrap-vcpkg.bat -disableMetrics
   setx VCPKG_ROOT C:\dev\vcpkg
   ```
4. **Outils Node** utilisés par les plugins et skills :
   ```powershell
   npm install -g @angular/cli typescript-language-server typescript @playwright/cli@latest
   ```
5. **Claude Code** :
   ```powershell
   irm https://claude.ai/install.ps1 | iex
   ```
   Il faut un abonnement Claude **Pro, Max, Team ou Enterprise** (le plan gratuit n'inclut pas Claude Code).
6. **Ferme et rouvre** le terminal (pour recharger le PATH), puis vérifie :
   ```powershell
   git --version; node -v; clangd --version; gh --version; claude --version
   claude doctor
   ```
7. Connecte GitHub : `gh auth login` (choisis GitHub.com → HTTPS → navigateur).
8. **Terminal de développement x64** : dans un PowerShell normal, ouvre ton profil (script exécuté à l'ouverture de chaque PowerShell) :
   ```powershell
   if (-not (Test-Path $PROFILE)) { New-Item -ItemType File -Path $PROFILE -Force }
   notepad $PROFILE
   ```
   Colle-y cette fonction, enregistre, puis rouvre PowerShell :
   ```powershell
   # Charge l'environnement MSVC x64 (cl, cmake, ninja) dans le PowerShell courant.
   function dev64 {
       $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
       Import-Module "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
       Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
       $env:VCPKG_ROOT = 'C:\dev\vcpkg'   # notre clone, pas le vcpkg intégré à Visual Studio
   }
   ```
   Désormais : ouvre PowerShell, `cd` dans ton projet, tape `dev64`. `cl` doit afficher « … pour x64 ».
   *Pourquoi x64* : les bibliothèques vcpkg sont compilées en x64 (triplet `x64-windows`). Avec un compilateur x86, l'édition de liens échoue (`LNK1112 : conflit de type d'ordinateur`).
   *Pourquoi forcer `VCPKG_ROOT`* : le terminal développeur le fait pointer vers le vcpkg intégré à Visual Studio, qui se met à jour avec VS et se trouve dans `Program Files`. Notre propre clone reste stable et identique pour tout le groupe.

> **WebStorm** : son terminal intégré est un PowerShell qui charge ton profil, donc tape `dev64` puis `claude` et c'est prêt.

---

## Étape 2 — Créer le repo (une seule personne)

Dans un PowerShell normal (pas en administrateur) :

```powershell
mkdir C:\dev\uno-neon
cd C:\dev\uno-neon
git init -b main
```

Copie le contenu du kit dans ce dossier, en gardant l'arborescence :

```
C:\dev\uno-neon\
├── CLAUDE.md
├── .gitignore
├── .mcp.json
├── docs\ SPEC.md, PROGRESS.md, SETUP.md, PROMPTS.md
└── scripts\ setup-claude.ps1
```

Premier commit et création du repo GitHub (privé) :

```powershell
git add .
git commit -m "docs: add project spec, Claude Code rules and setup guide"
gh repo create uno-neon --private --source=. --remote=origin --push
```

Sur GitHub, dans le repo :
- **Settings → Collaborators** : invite tes coéquipiers.
- **Settings → Branches (ou Rules)** : protège `main` (pull request obligatoire). Sur un repo privé, cette option demande GitHub Pro, gratuit avec le **GitHub Student Developer Pack**. Sinon, passez le repo en public ou appliquez la règle entre vous.

---

## Étape 3 — Premier lancement de Claude Code (une fois par PC)

```powershell
cd C:\dev\uno-neon
dev64
claude
```

1. Un navigateur s'ouvre pour te connecter à ton compte Claude.
2. Claude Code te demande si tu fais confiance à ce dossier → **Oui**.
3. Il détecte `.mcp.json` et te demande d'approuver le serveur MCP `angular-cli` → **approuve**. Vérifie ensuite avec la commande `/mcp` (le serveur doit apparaître comme connecté).

**L'essentiel de l'interface :**

| Action | Comment |
|---|---|
| Écrire une demande | tape du texte et Entrée |
| Changer de mode | `Shift+Tab` fait défiler : normal → acceptation auto des modifications → **plan mode** (`⏸ plan mode on` : Claude lit et propose un plan sans rien modifier) |
| Interrompre Claude | `Échap` |
| Revenir en arrière (conversation et fichiers) | `Échap` deux fois |
| Repartir d'un contexte vide | `/clear` (CLAUDE.md et PROGRESS.md sont rechargés automatiquement) |
| Reprendre la dernière session | `claude --continue` |
| Mentionner un fichier | `@docs/SPEC.md` |
| Coller une capture d'écran | `Alt+V` |
| Voir les plugins / serveurs MCP | `/plugin` / `/mcp` |

**Permissions** : quand Claude veut lancer une commande, il te demande l'autorisation. Lis la commande avant d'accepter. « Oui, et ne plus demander » est pratique pour les commandes sûres (`cmake --build`, `npm test`), jamais pour `git push`, les suppressions ou quelque chose que tu ne comprends pas.

---

## Étape 4 — Installer les plugins (une seule personne, portée « projet »)

Dans la session Claude Code, pour chaque plugin :

```
/plugin install clangd-lsp@claude-plugins-official
```

Le panneau du plugin s'ouvre et affiche ce qu'il installe (skills, agents, hooks, serveurs de langage…) et son coût en contexte. **Lis-le**, puis choisis **« Install for all collaborators on this repository (project scope) »**.

Répète pour :

```
/plugin install typescript-lsp@claude-plugins-official
/plugin install frontend-design@claude-plugins-official
/plugin install feature-dev@claude-plugins-official
/plugin install code-review@claude-plugins-official
```

Si Claude Code affiche `Run /reload-plugins to activate`, lance `/reload-plugins`. Vérifie dans `/plugin` → onglet **Installed**.

| Plugin | Pourquoi dans ce projet |
|---|---|
| `clangd-lsp` | Claude voit les vraies erreurs de compilation C++ après chaque modification, au lieu de deviner |
| `typescript-lsp` | pareil pour le TypeScript d'Angular |
| `frontend-design` | skill officiel d'Anthropic pour des interfaces soignées et originales |
| `feature-dev` | déroulé en étapes pour les grosses fonctionnalités (explorer → concevoir → implémenter → relire) |
| `code-review` | relecture de PR par plusieurs agents avant la relecture humaine |

**Les trois portées, pour comprendre :**
- **project** : activé pour tout le groupe via `.claude/settings.json` (versionné). Chaque coéquipier doit quand même l'installer une fois sur son PC (étape 8).
- **user** : pour toi, dans tous tes projets (`~/.claude/settings.json`).
- **local** : pour toi, dans ce repo seulement (`.claude/settings.local.json`, jamais versionné).

C'est le « d'abord le projet, ensuite en global » dont on avait parlé : si un plugin te plaît, réinstalle-le en portée *user*.

> **Pas de taste-skill ici** : il ferait doublon avec `frontend-design` (deux skills de génération de design qui se contrediraient) et il vise surtout React.

---

## Étape 5 — Installer les skills (une seule personne)

Quitte Claude Code (`/exit`) et, **à la racine du repo** dans PowerShell :

```powershell
npx skills add https://github.com/angular/skills --skill angular-developer -a claude-code --copy -y
npx skills add https://github.com/trailofbits/skills --skill modern-cpp -a claude-code --copy -y
npx skills add vercel-labs/agent-skills --skill web-design-guidelines -a claude-code --copy -y
playwright-cli install --skills
```

- `--copy` : de vrais fichiers plutôt que des liens symboliques. Sous Windows, git gère mal les liens symboliques, et les coéquipiers récupèrent ainsi les skills par un simple `git pull`.
- **Avant de committer, ouvre chaque `SKILL.md` dans `.claude/skills/` et lis-le** : un skill est un prompt qui s'exécute avec tes droits.
- `web-design-guidelines` télécharge ses règles depuis GitHub à chaque audit (il faut Internet, et le contenu peut évoluer).

| Skill | Rôle |
|---|---|
| `angular-developer` (officiel Angular) | code Angular moderne : signals, standalone, zoneless |
| `modern-cpp` (Trail of Bits) | C++20/23 idiomatique et orienté sécurité |
| `web-design-guidelines` (Vercel) | audit accessibilité / UX / performance de l'UI |
| `playwright-cli` | ouvrir le site, cliquer, prendre des captures pour vérifier le rendu |

---

## Étape 6 — Vérifier le MCP Angular

Le fichier `.mcp.json` du kit déclare le serveur MCP officiel de la CLI Angular (bonnes pratiques et documentation à jour). Il utilise `cmd /c npx …`, la forme fiable sous Windows natif. Si un membre du groupe est sur macOS ou Linux, remplacez par `"command": "npx", "args": ["-y", "@angular/cli", "mcp"]`.

Relance `claude` puis tape `/mcp` : `angular-cli` doit être **connected**.

---

## Étape 7 — Committer la configuration

```powershell
git checkout -b chore/claude-setup
git add .claude .mcp.json
git status        # vérifie qu'aucun settings.local.json n'est ajouté
git commit -m "chore: add Claude Code plugins, skills and MCP config"
git push -u origin chore/claude-setup
gh pr create --fill
```

Fusionne la PR sur GitHub, puis `git checkout main` et `git pull`.

---

## Étape 8 — Sur un nouveau poste

1. Faire l'**étape 1** (outils).
2. `git clone` le repo dans `C:\dev\uno-neon`, lancer `claude` une première fois (connexion, confiance, approbation du MCP), puis `/exit`.
3. Dans PowerShell, à la racine du repo, après `dev64` :
   ```powershell
   .\scripts\setup-claude.ps1
   ```
   Le script installe les plugins du projet et signale les outils manquants. Les skills et le MCP arrivent déjà avec le repo.

Si PowerShell refuse d'exécuter le script : `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`, une seule fois.

---

## Étape 9 — Lancer le projet

1. `git checkout -b chore/phase-0-foundations`
2. `dev64` puis `claude`
3. `Shift+Tab` jusqu'à `⏸ plan mode on`
4. Colle le **Prompt 0** de `docs/PROMPTS.md`
5. Lis le plan proposé. Pose des questions (« pourquoi Ninja plutôt que le générateur Visual Studio ? »), demande des modifications, puis accepte.
6. Claude avance étape par étape et s'arrête à chaque fin d'étape. Toi : lis le diff (`git diff` ou WebStorm), vérifie que les tests passent, commit.

## Le rythme ensuite (pour chaque étape de `PROGRESS.md`)

```
nouvelle branche → /clear → plan mode → prompt de l'étape → relire le plan
→ exécution → relire le code + tests verts → commit → PR → /code-review → merge
```

- **`/clear` entre deux étapes** : contexte propre, moins d'erreurs, moins de consommation. La continuité passe par `PROGRESS.md`, pas par l'historique du chat.
- **Ne fusionne jamais un code que tu ne sais pas expliquer.** Demande « explique-moi ce choix comme si je devais le défendre à l'oral » : tu devras sûrement le faire devant un jury.
- **Travail en parallèle** : après la phase 0, une personne peut prendre la phase 1 (cœur C++) pendant qu'une autre fait la phase 3 (UI sur données simulées), chacune sur sa branche et son PC. Si tu veux deux sessions sur ton propre PC, utilise `claude --worktree <nom>` (une copie de travail isolée par session).
- **Si le design dérive** : prends une capture, colle-la (`Alt+V`) et cite la section de la SPEC (« ça ne respecte pas §11.2 : texte directement sur le fond »).

---

## Étape 10 — Vérifier le serveur sous Linux (WSL), avant de pousser

Le CI compile et teste le serveur sous Linux (GCC 14 **et** Clang 20, sanitizers ASan+UBSan, `clang-format`/`clang-tidy` 23) en plus de Windows. Pour attraper ces problèmes **avant** la PR plutôt qu'en CI, on rejoue la même vérification localement dans WSL2 (Ubuntu 24.04, la même version que le runner du CI).

### Une fois par PC : installer WSL et les outils

1. **WSL2 + Ubuntu 24.04** (une fois par PC), dans PowerShell **en administrateur** :
   ```powershell
   wsl --install -d Ubuntu-24.04
   ```
   Redémarre si demandé, puis ouvre Ubuntu depuis le menu Démarrer pour créer ton utilisateur Unix (mot de passe différent de ton compte Windows, c'est normal).

2. **Outils Linux** (GCC 14, Clang 20, clang-format/clang-tidy 23, autotools, ninja, cmake, rsync, vcpkg) : depuis PowerShell, à la racine du repo :
   ```powershell
   wsl -d Ubuntu-24.04 -- bash scripts/linux-setup.sh
   ```
   Demande ton mot de passe **sudo Linux** (celui créé à l'étape 1, pas ton mot de passe Windows). Installe tout depuis les mêmes sources que `.github/workflows/ci.yml` (versions identiques), clone `vcpkg` dans `~/vcpkg` à la baseline de `server/vcpkg.json` et le bootstrap. **Relançable sans risque** (idempotent) — à refaire si le CI change de version d'outil ou de baseline vcpkg.

### À chaque fois : vérifier avant de pousser

```powershell
wsl -d Ubuntu-24.04 -- bash scripts/linux-check.sh
```

Ce script ne demande jamais sudo. Il copie l'arbre de travail vers `~/uno-neon-linux` (un dossier natif WSL — compiler directement sur `/mnt/c` est très lent), puis pour **GCC 14** et pour **Clang 20** : configure, build et `ctest` avec le preset `debug-asan`, puis `clang-format` et `clang-tidy` (une seule fois, sur le build Clang), exactement comme le job `server-linux` du CI. Si un outil manque, il s'arrête avec un message qui indique de relancer `linux-setup.sh`.

Le tout premier lancement est long (vcpkg compile `libsodium`, `uwebsockets`, `spdlog`, etc. depuis les sources) ; vcpkg met les paquets compilés en cache dans `~/.cache/vcpkg/archives` sous WSL, donc les lancements suivants sont nettement plus rapides. En mode acceptation manuelle, autorise Claude Code à lancer ce script en arrière-plan si besoin pour ce premier build.

**Définition de « terminé » (CLAUDE.md) :** `linux-check.sh` doit être vert avant tout push qui touche `server/`.

### Hook automatique (`git push`)

`.githooks/pre-push` (versionné) fait respecter cette règle sans y penser : à chaque `git push`, il regarde si les commits poussés modifient `server/` et, si oui, relance `wsl -d Ubuntu-24.04 -- bash scripts/linux-check.sh` ; le push est bloqué si le script échoue. Un push qui ne touche que `docs/`, `client/`, etc. ne déclenche rien.

`scripts/setup-claude.ps1` (étape 8) exécute `git config core.hooksPath .githooks` pour l'activer — à faire une fois par poste, avant ton premier push.

- **WSL absent, ou distribution `Ubuntu-24.04` non installée** : le hook affiche un avertissement et **laisse passer le push quand même** (pas de blocage pour qui n'a pas encore suivi l'étape 10) ; le CI fait office de filet dans ce cas.
- **Contournement d'urgence** (le check est cassé, ou tu dois pousser vite) : `git push --no-verify`. Le CI (`server-linux`) vérifiera quand même à la PR — un push forcé sans vérification locale n'évite jamais la vérification du CI.
