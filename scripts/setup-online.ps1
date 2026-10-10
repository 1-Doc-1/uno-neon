<#
.SYNOPSIS
    Vérifie que tout ce qu'il faut pour jouer par un tunnel est installé (une seule fois par PC).

.DESCRIPTION
    Contrôle Caddy et cloudflared (installés par winget), puis ce qui sert à compiler : CMake, Ninja, le compilateur
    MSVC (terminal où `dev64` a été lancé) et Node.js. Avec -Install, installe tout seul Caddy et cloudflared s'ils
    manquent. Rien d'autre n'est modifié.

.EXAMPLE
    ./scripts/setup-online.ps1
    ./scripts/setup-online.ps1 -Install
#>
param(
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'online-common.ps1')

$missing = @()
foreach ($tool in $script:OnlineTools) {
    $path = Find-Tool $tool.Command
    if (-not $path -and $Install) {
        Write-Host "Installation de $($tool.WinGetId)..." -ForegroundColor Cyan
        winget install --id $tool.WinGetId --exact --silent --accept-package-agreements --accept-source-agreements
        $path = Find-Tool $tool.Command
    }
    if ($path) {
        Write-Host ("[ok] {0,-12} {1}" -f $tool.Command, $path) -ForegroundColor Green
    }
    else {
        Write-Host ("[manque] {0,-12} {1}" -f $tool.Command, $tool.Role) -ForegroundColor Red
        $missing += "winget install --id $($tool.WinGetId) --exact"
    }
}

# Pour compiler : dev64 met CMake, Ninja et MSVC dans le PATH
foreach ($name in 'cmake', 'ninja', 'cl', 'node', 'npm') {
    $path = Find-Tool $name
    if ($path) {
        Write-Host ("[ok] {0,-12} {1}" -f $name, $path) -ForegroundColor Green
    }
    else {
        Write-Host ("[manque] {0,-12} lance dev64 dans ce terminal (MSVC, CMake, Ninja) ou installe Node.js" -f $name) -ForegroundColor Red
        $missing += $name
    }
}

if ($missing.Count -gt 0) {
    Write-Host ''
    Write-Host 'Il manque quelque chose. Pour Caddy et cloudflared, soit relance avec -Install, soit tape :'
    $missing | Where-Object { $_ -like 'winget*' } | ForEach-Object { Write-Host "  $_" }
    exit 1
}
Write-Host ''
Write-Host 'Tout est prêt : ./scripts/play-online.ps1' -ForegroundColor Green
