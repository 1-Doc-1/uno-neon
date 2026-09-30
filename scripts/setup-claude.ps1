# Installe sur ce PC les plugins Claude Code activés pour le projet, puis vérifie les outils.
# Usage : dans PowerShell, à la racine du repo, après `dev64` (voir docs/SETUP.md) et après avoir lancé `claude` au moins une fois.
#   .\scripts\setup-claude.ps1

$ErrorActionPreference = 'Stop'

# Liste des plugins du projet (si elle change, mettre aussi à jour le tableau de docs/SETUP.md, étape 4).
$marketplace = 'claude-plugins-official'
$projectPlugins = @('clangd-lsp', 'typescript-lsp', 'frontend-design', 'feature-dev', 'code-review')

# Outils attendus dans le PATH (cmake, ninja et cl ne sont présents qu'après `dev64`).
$requiredTools = @('git', 'node', 'npm', 'gh', 'cmake', 'ninja', 'cl', 'clangd',
                   'typescript-language-server', 'playwright-cli', 'claude')

if (-not (Get-Command claude -ErrorAction SilentlyContinue)) {
    throw "Claude Code n'est pas installé. Voir docs/SETUP.md, étape 1."
}

# Active le hook pre-push versionné (.githooks/pre-push) : vérification Linux (WSL) automatique
# avant tout push qui touche server/. Voir docs/SETUP.md, étape 10.
git config core.hooksPath .githooks

# Le marketplace officiel est enregistré au premier lancement interactif ; on s'en assure sans échouer s'il existe déjà.
try { $null = claude plugin marketplace add anthropics/claude-plugins-official 2>&1 } catch { }

$failedPlugins = @()
foreach ($plugin in $projectPlugins) {
    Write-Host "-> Installation de $plugin@$marketplace (portée project)"
    claude plugin install "$plugin@$marketplace" --scope project
    # Les commandes natives ne lèvent pas d'exception PowerShell : on lit leur code de sortie.
    if ($LASTEXITCODE -ne 0) { $failedPlugins += $plugin }
}
if ($failedPlugins) {
    Write-Warning "Échec de l'installation de : $($failedPlugins -join ', '). Relance le script ou installe-les via /plugin."
}

$missingTools = $requiredTools | Where-Object { -not (Get-Command $_ -ErrorAction SilentlyContinue) }
if ($missingTools) {
    Write-Warning "Outils introuvables dans le PATH : $($missingTools -join ', '). Voir docs/SETUP.md, étape 1."
} else {
    Write-Host "Tous les outils sont présents." -ForegroundColor Green
}

if (-not $env:VCPKG_ROOT -or -not (Test-Path (Join-Path $env:VCPKG_ROOT 'vcpkg.exe'))) {
    Write-Warning "VCPKG_ROOT absent ou invalide ($env:VCPKG_ROOT). Voir docs/SETUP.md, étape 1."
}

# VsDevCmd expose l'architecture cible : un compilateur x86 casserait l'édition de liens avec les paquets x64-windows.
if ($env:VSCMD_ARG_TGT_ARCH -and $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    Write-Warning "Compilateur configuré pour $env:VSCMD_ARG_TGT_ARCH au lieu de x64 : utilise 'dev64'."
}

Write-Host "Terminé. Lance 'claude' puis '/plugin' (onglet Installed) et '/mcp' pour vérifier." -ForegroundColor Cyan
