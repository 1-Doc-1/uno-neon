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
