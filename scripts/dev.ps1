<#
.SYNOPSIS
    Lance le serveur C++ et le client Angular ensemble, pour jouer ou développer.

.DESCRIPTION
    Démarre build/dev/uno_server.exe (port 9001) puis `npm start` (http://localhost:4200, proxy /ws vers le serveur).
    Ctrl+C arrête le client, et le serveur est arrêté avec lui.
    Si le serveur n'est pas compilé, ou avec -Build, il est d'abord compilé : lancer alors le script depuis un terminal
    où `dev64` a été exécuté (MSVC, CMake et Ninja dans le PATH).

.EXAMPLE
    ./scripts/dev.ps1
    ./scripts/dev.ps1 -Build
#>
param(
    [switch]$Build
)

$ErrorActionPreference = 'Stop'

$root = Split-Path $PSScriptRoot -Parent
$serverDir = Join-Path $root 'server'
$clientDir = Join-Path $root 'client'
$serverExe = Join-Path $serverDir 'build/dev/uno_server.exe'

function Invoke-Checked([string]$What, [scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$What a échoué (code $LASTEXITCODE)."
    }
}

if ($Build -or -not (Test-Path $serverExe)) {
    Push-Location $serverDir
    try {
        Invoke-Checked 'La configuration CMake' { cmake --preset dev }
        Invoke-Checked 'La compilation du serveur' { cmake --build --preset dev }
    }
    finally {
        Pop-Location
    }
}

if (-not (Test-Path (Join-Path $clientDir 'node_modules'))) {
    Push-Location $clientDir
    try {
        Invoke-Checked 'npm ci' { npm ci }
    }
    finally {
        Pop-Location
    }
}

$server = Start-Process -FilePath $serverExe -WorkingDirectory $serverDir -NoNewWindow -PassThru
Write-Host "Serveur lancé (PID $($server.Id)) sur le port 9001. Client : http://localhost:4200"

Push-Location $clientDir
try {
    npm start
}
finally {
    Pop-Location
    if (-not $server.HasExited) {
        Stop-Process -Id $server.Id -Force
    }
}
