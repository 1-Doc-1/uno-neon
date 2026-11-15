# Fonctions partagées par setup-online.ps1 et play-online.ps1 (à charger avec « . »).

# Les paquets winget des deux outils (identifiants vérifiés avec `winget show --id ... --exact`).
$script:OnlineTools = @(
    @{ Command = 'caddy'; WinGetId = 'CaddyServer.Caddy'; Role = 'serveur web local (client, /ws, en-têtes de sécurité)' },
    @{ Command = 'cloudflared'; WinGetId = 'Cloudflare.cloudflared'; Role = 'tunnel Cloudflare (adresse publique temporaire)' }
)

# winget modifie le PATH de la machine : un terminal ouvert avant l'installation ne le voit pas encore.
function Update-PathFromRegistry {
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user = [Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = "$machine;$user"
}

function Find-Tool([string]$Command) {
    $found = Get-Command $Command -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $found) {
        Update-PathFromRegistry
        $found = Get-Command $Command -ErrorAction SilentlyContinue | Select-Object -First 1
    }
    if ($found) { return $found.Source }
    return $null
}

function Assert-PortFree([int]$Port, [string]$What) {
    $listener = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($listener) {
        $owner = Get-Process -Id $listener.OwningProcess -ErrorAction SilentlyContinue
        $name = if ($owner) { $owner.ProcessName } else { 'processus inconnu' }
        throw "Le port $Port ($What) est déjà utilisé par $name (PID $($listener.OwningProcess)). Arrête-le puis relance le script."
    }
}

function Invoke-Checked([string]$What, [scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$What a échoué (code $LASTEXITCODE)."
    }
}

# Met MSVC, CMake et Ninja dans le PATH comme la fonction dev64 (VS Build Tools), si ce terminal ne les a pas déjà.
function Enter-BuildEnvironment {
    if (Get-Command cl -ErrorAction SilentlyContinue) { return }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw "Visual Studio Build Tools introuvable : installe-les, ou lance dev64 dans ce terminal." }
    $vsPath = & $vswhere -latest -products * -property installationPath
    Import-Module (Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = 'C:\dev\vcpkg' }
    # Messages de cl en anglais : Ninja suit les dépendances d'en-têtes grâce à /showIncludes
    $env:VSLANG = '1033'
}

# --- Mot de passe du site (ADR 0043) -----------------------------------------------------------------------------
# Seul le HACHAGE bcrypt est gardé, dans un dossier de l'utilisateur, hors du dépôt. Jamais le mot de passe.
$script:PasswordHashFile = Join-Path $env:LOCALAPPDATA 'noc\site-password.hash'

function ConvertTo-PlainText([System.Security.SecureString]$Secret) {
    $pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Secret)
    try { return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer) }
}

# Demande un mot de passe deux fois, sans l'afficher, et rend son hachage bcrypt. Le mot de passe passe par l'entrée
# standard de `caddy hash-password` : il n'apparaît ni sur une ligne de commande, ni dans un fichier.
function Read-NewPasswordHash([string]$Caddy) {
    $first = Read-Host 'Mot de passe du site (8 caractères au moins)' -AsSecureString
    $second = Read-Host 'Mot de passe, encore une fois' -AsSecureString
    $plain = ConvertTo-PlainText $first
    try {
        if ($plain -ne (ConvertTo-PlainText $second)) { throw 'Les deux saisies sont différentes.' }
        if ($plain.Length -lt 8) { throw 'Le mot de passe doit faire 8 caractères au moins.' }
        $hash = ($plain | & $Caddy hash-password 2>$null | Select-Object -First 1)
        if ($LASTEXITCODE -ne 0 -or -not $hash -or -not $hash.StartsWith('$2')) { throw 'Caddy n''a pas produit de hachage bcrypt.' }
        return $hash.Trim()
    }
    finally { $plain = $null }
}

# Le hachage enregistré, ou une erreur claire s'il n'y en a pas encore.
function Get-StoredPasswordHash {
    if (-not (Test-Path $script:PasswordHashFile)) { return $null }
    $hash = (Get-Content -Raw -Path $script:PasswordHashFile).Trim()
    if (-not $hash.StartsWith('$2')) { throw "Le fichier $script:PasswordHashFile ne contient pas un hachage bcrypt : relance ./scripts/set-password.ps1" }
    return $hash
}
