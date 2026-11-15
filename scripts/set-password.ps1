<#
.SYNOPSIS
    Choisit (ou retire) le mot de passe qui protège le site pendant une soirée en ligne (ADR 0043).

.DESCRIPTION
    Demande un mot de passe (deux fois, sans l'afficher), en calcule le HACHAGE bcrypt avec `caddy hash-password`
    et l'enregistre dans %LOCALAPPDATA%\noc\site-password.hash, un dossier HORS du dépôt.
    Le mot de passe lui-même n'est écrit nulle part : ni dans un fichier, ni dans un journal, ni sur la ligne de
    commande (il passe par l'entrée standard de Caddy).

    Ensuite, ./scripts/play-online.ps1 -Password exige ce mot de passe de tous les visiteurs (nom d'utilisateur : noc).
    Sans -Password, le site reste ouvert, même si un hachage est enregistré.

    Où mettre le hachage pour un autre usage (Caddy lancé à la main, serveur permanent) : dans la variable
    d'environnement SITE_PASSWORD_HASH du processus Caddy. Jamais le mot de passe en clair, jamais dans le dépôt.

.PARAMETER Coolify
    Affiche, après l'enregistrement, le hachage prêt à coller dans la variable SITE_PASSWORD_HASH de Coolify (déploiement
    permanent, docs/DEPLOIEMENT.md), sous les deux formes sûres : à coller tel quel avec « Literal » coché, ou avec les « $ »
    doublés. Le hachage n'est PAS le mot de passe, mais ne le laisse pas traîner dans un message public ni dans le dépôt.
    Sans cette option, rien n'est affiché.

.PARAMETER Remove
    Supprime le hachage enregistré (le site n'est alors plus protégé, même avec -Password : le script te le redemande).

.EXAMPLE
    ./scripts/set-password.ps1            # choisir ou changer le mot de passe
    ./scripts/set-password.ps1 -Remove    # l'oublier
#>
param(
    [switch]$Remove,
    [switch]$Coolify
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'online-common.ps1')

if ($Remove) {
    if (Test-Path $script:PasswordHashFile) {
        Remove-Item -Force $script:PasswordHashFile
        Write-Host 'Mot de passe oublié. Sans -Password, play-online.ps1 laisse le site ouvert.' -ForegroundColor Green
    }
    else {
        Write-Host 'Aucun mot de passe enregistré.'
    }
    return
}

$caddy = Find-Tool 'caddy'
if (-not $caddy) { throw 'Caddy manque : lance ./scripts/setup-online.ps1 -Install' }

$hash = Read-NewPasswordHash $caddy
New-Item -ItemType Directory -Force -Path (Split-Path $script:PasswordHashFile) | Out-Null
Set-Content -Path $script:PasswordHashFile -Value $hash -Encoding ascii -NoNewline

Write-Host ''
Write-Host 'Mot de passe enregistré (seul son hachage est gardé, hors du dépôt) :' -ForegroundColor Green
Write-Host "  $script:PasswordHashFile"
Write-Host ''
Write-Host 'Pour une soirée protégée :  ./scripts/play-online.ps1 -Password'
Write-Host 'Tes amis saisissent le nom d''utilisateur  noc  et ce mot de passe (le navigateur le demande une fois).'
Write-Host 'Partage le mot de passe par un autre canal que le lien (voir docs/MODE-EMPLOI.md).'

if ($Coolify) {
    Write-Host ''
    Write-Host 'Pour Coolify, variable SITE_PASSWORD_HASH (au choix, voir docs/DEPLOIEMENT.md) :' -ForegroundColor Cyan
    Write-Host '  1) coche « Literal » et colle TEL QUEL :'
    Write-Host "     $hash"
    Write-Host '  2) sans « Literal », avec les « $ » doublés :'
    Write-Host "     $($hash.Replace('$', '$$'))"
}
