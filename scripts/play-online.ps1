<#
.SYNOPSIS
    Rend le jeu jouable par des amis à distance, par un tunnel Cloudflare, avec une seule commande.

.DESCRIPTION
    1. compile le serveur en Release (sans crochets de test) et le client en production ;
    2. lance Caddy (sert le client, relaie /ws, compresse, en-têtes de sécurité) sur 127.0.0.1 ;
    3. lance un quick tunnel Cloudflare vers Caddy et lit son adresse publique ;
    4. lance le serveur sur 127.0.0.1 seulement, avec cette adresse pour SEULE origine autorisée ;
    5. vérifie la sécurité (écoute locale seulement, en-têtes, origine, pas de /health public) ;
    6. affiche l'adresse en grand et la copie dans le presse-papiers.
    Le PC ne se met pas en veille tant que le script tourne. Ctrl+C arrête tout (tunnel, Caddy, serveur) ; si la
    fenêtre est fermée, Windows arrête aussi les trois programmes (objet Job).
    Prérequis : ./scripts/setup-online.ps1 (le script ouvre lui-même l'environnement MSVC comme dev64).

.EXAMPLE
    ./scripts/play-online.ps1
    ./scripts/play-online.ps1 -SkipBuild
#>
param(
    [switch]$SkipBuild,
    [int]$WebPort = 8080,
    [int]$ServerPort = 9001
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'online-common.ps1')

$root = Split-Path $PSScriptRoot -Parent
$serverDir = Join-Path $root 'server'
$clientDir = Join-Path $root 'client'
$serverExe = Join-Path $serverDir 'build/release/uno_server.exe'
$webRoot = Join-Path $clientDir 'dist/client/browser'
$caddyfile = Join-Path $root 'deploy/Caddyfile.tunnel'
$logDir = Join-Path $env:TEMP 'uno-neon-online'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

// Un objet Job Windows qui tue tous ses processus quand son dernier handle se ferme : même si la fenêtre est fermée
// ou le script tué, le tunnel, Caddy et le serveur ne restent jamais orphelins.
public static class OnlineJob {
    [StructLayout(LayoutKind.Sequential)]
    struct BasicLimits {
        public long PerProcessUserTimeLimit; public long PerJobUserTimeLimit; public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize; public UIntPtr MaximumWorkingSetSize; public uint ActiveProcessLimit;
        public UIntPtr Affinity; public uint PriorityClass; public uint SchedulingClass;
    }
    [StructLayout(LayoutKind.Sequential)]
    struct IoCounters {
        public ulong A; public ulong B; public ulong C; public ulong D; public ulong E; public ulong F;
    }
    [StructLayout(LayoutKind.Sequential)]
    struct ExtendedLimits {
        public BasicLimits Basic; public IoCounters Io; public UIntPtr ProcessMemory; public UIntPtr JobMemory;
        public UIntPtr PeakProcessMemory; public UIntPtr PeakJobMemory;
    }
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] static extern IntPtr CreateJobObject(IntPtr attributes, string name);
    [DllImport("kernel32.dll")] static extern bool SetInformationJobObject(IntPtr job, int infoClass, IntPtr info, uint size);
    [DllImport("kernel32.dll")] static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll")] static extern uint SetThreadExecutionState(uint flags);

    static IntPtr job = IntPtr.Zero;

    public static void Create() {
        job = CreateJobObject(IntPtr.Zero, null);
        var limits = new ExtendedLimits();
        limits.Basic.LimitFlags = 0x2000; // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        int size = Marshal.SizeOf(typeof(ExtendedLimits));
        IntPtr buffer = Marshal.AllocHGlobal(size);
        try {
            Marshal.StructureToPtr(limits, buffer, false);
            if (!SetInformationJobObject(job, 9, buffer, (uint)size)) throw new InvalidOperationException("SetInformationJobObject");
        } finally { Marshal.FreeHGlobal(buffer); }
    }
    public static void Add(IntPtr process) {
        if (!AssignProcessToJobObject(job, process)) throw new InvalidOperationException("AssignProcessToJobObject");
    }
    public static void Close() {
        if (job != IntPtr.Zero) { CloseHandle(job); job = IntPtr.Zero; }
    }
    // ES_CONTINUOUS | ES_SYSTEM_REQUIRED : le PC ne se met pas en veille (l'écran peut s'éteindre).
    public static void KeepAwake() { SetThreadExecutionState(0x80000001); }
    public static void AllowSleep() { SetThreadExecutionState(0x80000000); }
}
'@

function Start-Child([string]$Name, [string]$Exe, [string[]]$Arguments, [hashtable]$Environment) {
    $stdout = Join-Path $logDir "$Name.out.log"
    $stderr = Join-Path $logDir "$Name.err.log"
    foreach ($key in $Environment.Keys) { Set-Item -Path "Env:$key" -Value $Environment[$key] }
    try {
        $options = @{ FilePath = $Exe; PassThru = $true; WindowStyle = 'Hidden'; RedirectStandardOutput = $stdout; RedirectStandardError = $stderr }
        if ($Arguments.Count -gt 0) { $options.ArgumentList = $Arguments }
        $process = Start-Process @options
    }
    finally {
        foreach ($key in $Environment.Keys) { Remove-Item -Path "Env:$key" -ErrorAction SilentlyContinue }
    }
    [OnlineJob]::Add($process.Handle)
    return $process
}

function Wait-Listening([int]$Port, [System.Diagnostics.Process]$Process, [string]$What) {
    $deadline = (Get-Date).AddSeconds(15)
    while (-not (Get-NetTCPConnection -State Listen -LocalPort $Port -OwningProcess $Process.Id -ErrorAction SilentlyContinue)) {
        if ($Process.HasExited) { throw "$What s'est arrêté au démarrage (code $($Process.ExitCode)). Voir $logDir" }
        if ((Get-Date) -gt $deadline) { throw "$What n'écoute pas sur le port $Port après 15 s. Voir $logDir" }
        Start-Sleep -Milliseconds 200
    }
}

# Les sources de scripts de la CSP : les hachages qu'Angular (autoCsp) a calculés pour les scripts de l'index.html compilé.
function Get-ScriptCsp([string]$IndexHtml) {
    $html = Get-Content -Raw -Path $IndexHtml
    $hashes = [regex]::Matches($html, "'sha256-[A-Za-z0-9+/=]+'") | ForEach-Object { $_.Value } | Select-Object -Unique
    if (-not $hashes) { throw "Aucun hachage de script dans $IndexHtml : la CSP ne peut pas être construite (autoCsp désactivé ?)." }
    return "'strict-dynamic' " + ($hashes -join ' ')
}

# Les contrôles de sécurité, faits sur la vraie installation avant de montrer l'adresse à quiconque.
function Assert-Secure([string]$PublicUrl) {
    foreach ($port in $ServerPort, $WebPort) {
        $listeners = Get-NetTCPConnection -State Listen -LocalPort $port -ErrorAction SilentlyContinue
        $open = @($listeners | Where-Object { $_.LocalAddress -notin '127.0.0.1', '::1' })
        if ($open.Count -gt 0) { throw "Le port $port écoute sur $($open[0].LocalAddress) : il doit rester sur 127.0.0.1." }
    }

    # Comme un navigateur : on demande la page compressée (PowerShell 5.1 ne le fait pas tout seul)
    $page = Invoke-WebRequest -Uri $PublicUrl -UseBasicParsing -Headers @{ 'Accept-Encoding' = 'gzip' }
    $csp = $page.Headers['Content-Security-Policy'] -join ' '
    if ($page.StatusCode -ne 200 -or $csp -notmatch "default-src 'self'" -or $csp -notmatch "frame-ancestors 'none'") {
        throw "La page publique n'a pas la CSP attendue (code $($page.StatusCode), CSP reçue : '$csp')."
    }
    foreach ($header in 'Strict-Transport-Security', 'X-Content-Type-Options', 'Referrer-Policy', 'Permissions-Policy') {
        if (-not $page.Headers[$header]) { throw "En-tête de sécurité absent : $header." }
    }
    if (($page.Headers['Content-Encoding'] -join '') -notin 'zstd', 'gzip', 'br') { Write-Warning 'La page n''est pas compressée.' }

    if ((Get-HttpStatus "$PublicUrl/health") -ne 404) { throw '/health est public : il doit répondre 404 par le tunnel.' }

    $wsUrl = ($PublicUrl -replace '^https', 'wss') + '/ws'
    if (Test-WebSocket $wsUrl 'https://evil.example') { throw "Une origine étrangère est acceptée par le serveur." }
    if (-not (Test-WebSocket $wsUrl $PublicUrl)) { throw "Le serveur refuse l'origine du tunnel : les amis ne pourraient pas jouer." }
}

# Vrai quand l'adresse sert la vraie page du jeu (et non la page vide de Cloudflare qui précède l'établissement du tunnel).
function Test-PageServed([string]$Url) {
    try { return (Invoke-WebRequest -Uri $Url -UseBasicParsing).Content.Length -gt 0 }
    catch { return $false }
}

# Le code HTTP d'une adresse, même une erreur (404...) ; 0 si personne ne répond.
function Get-HttpStatus([string]$Url) {
    try { return [int](Invoke-WebRequest -Uri $Url -UseBasicParsing).StatusCode }
    catch {
        if ($_.Exception.Response) { return [int]$_.Exception.Response.StatusCode }
        return 0
    }
}

# Vrai si le serveur accepte la poignée de main WebSocket avec cette origine.
function Test-WebSocket([string]$Url, [string]$Origin) {
    $socket = New-Object System.Net.WebSockets.ClientWebSocket
    $socket.Options.SetRequestHeader('Origin', $Origin)
    $cancel = New-Object System.Threading.CancellationTokenSource (10000)
    try {
        $socket.ConnectAsync([Uri]$Url, $cancel.Token).GetAwaiter().GetResult()
        return $true
    }
    catch {
        return $false
    }
    finally {
        $socket.Dispose()
    }
}

function Stop-Child([System.Diagnostics.Process]$Process) {
    if ($Process -and -not $Process.HasExited) {
        # /T : tout l'arbre du processus
        taskkill /PID $Process.Id /T /F 2>&1 | Out-Null
    }
}

# --- Outils -----------------------------------------------------------------------------------------------------
$caddy = Find-Tool 'caddy'
$cloudflared = Find-Tool 'cloudflared'
if (-not $caddy -or -not $cloudflared) { throw 'Caddy ou cloudflared manque : lance ./scripts/setup-online.ps1 -Install' }
Assert-PortFree $WebPort 'Caddy'
Assert-PortFree $ServerPort 'serveur'

# --- Compilation ------------------------------------------------------------------------------------------------
if (-not $SkipBuild -or -not (Test-Path $serverExe) -or -not (Test-Path $webRoot)) {
    Enter-BuildEnvironment
    Push-Location $serverDir
    try {
        Invoke-Checked 'La configuration CMake (release)' { cmake --preset release }
        Invoke-Checked 'La compilation du serveur (release)' { cmake --build --preset release --target uno_server }
    }
    finally { Pop-Location }
    Push-Location $clientDir
    try {
        if (-not (Test-Path 'node_modules')) { Invoke-Checked 'npm ci' { npm ci } }
        Invoke-Checked 'Le build de production du client' { npm run build }
        Invoke-Checked 'Le contrôle du build de production' { node scripts/check-prod-bundle.mjs }
    }
    finally { Pop-Location }
}

# Le binaire de production ne contient aucun crochet de test (SPEC §9.5) : on cherche leur nom dedans
$bytes = [System.IO.File]::ReadAllBytes($serverExe)
if ([System.Text.Encoding]::ASCII.GetString($bytes).Contains('UNO_TEST_')) {
    throw "Le serveur contient des crochets de test (UNO_TEST_) : refus de le publier. Recompile avec le preset release."
}

$scriptCsp = Get-ScriptCsp (Join-Path $webRoot 'index.html')

# --- Lancement --------------------------------------------------------------------------------------------------
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
Get-ChildItem $logDir -Filter '*.log' | Remove-Item -Force
[OnlineJob]::Create()
[OnlineJob]::KeepAwake()
$caddyProcess = $null; $tunnelProcess = $null; $serverProcess = $null
try {
    Write-Host 'Démarrage de Caddy...' -ForegroundColor Cyan
    $caddyProcess = Start-Child 'caddy' $caddy @('run', '--config', $caddyfile, '--adapter', 'caddyfile') @{
        UNO_WEB_PORT   = "$WebPort"
        UNO_SERVER_PORT = "$ServerPort"
        UNO_WEB_ROOT   = ($webRoot -replace '\\', '/')
        UNO_CSP_SCRIPT = $scriptCsp
    }
    Wait-Listening $WebPort $caddyProcess 'Caddy'

    Write-Host 'Ouverture du tunnel Cloudflare...' -ForegroundColor Cyan
    $tunnelProcess = Start-Child 'cloudflared' $cloudflared @('tunnel', '--no-autoupdate', '--url', "http://127.0.0.1:$WebPort") @{}
    $publicUrl = $null
    $deadline = (Get-Date).AddSeconds(45)
    while (-not $publicUrl) {
        if ($tunnelProcess.HasExited) { throw "Le tunnel s'est arrêté (code $($tunnelProcess.ExitCode)). Voir $logDir\cloudflared.err.log" }
        if ((Get-Date) -gt $deadline) { throw "Le tunnel n'a pas donné d'adresse après 45 s. Voir $logDir\cloudflared.err.log" }
        Start-Sleep -Milliseconds 500
        $log = Get-Content -Raw -Path (Join-Path $logDir 'cloudflared.err.log') -ErrorAction SilentlyContinue
        if ($log -match 'https://[a-z0-9-]+\.trycloudflare\.com') { $publicUrl = $Matches[0] }
    }

    Write-Host 'Démarrage du serveur...' -ForegroundColor Cyan
    $serverProcess = Start-Child 'server' $serverExe @() @{
        UNO_BIND_ADDRESS    = '127.0.0.1'
        UNO_PORT            = "$ServerPort"
        UNO_ALLOWED_ORIGINS = $publicUrl
        UNO_TRUSTED_PROXY   = 'true'
        UNO_LOG_LEVEL       = 'info'
    }
    Wait-Listening $ServerPort $serverProcess 'Le serveur'

    # Le nom du tunnel met quelques secondes à exister. On le demande à un résolveur public (1.1.1.1) tant qu'il n'existe
    # pas, SANS interroger celui du PC : une réponse « inconnu » y serait gardée en mémoire plusieurs minutes.
    $publicHost = ([Uri]$publicUrl).Host
    $deadline = (Get-Date).AddSeconds(90)
    while (-not (Resolve-DnsName $publicHost -Server 1.1.1.1 -Type A -DnsOnly -ErrorAction SilentlyContinue)) {
        if ((Get-Date) -gt $deadline) { throw "Le nom $publicHost n'existe toujours pas après 90 s (tunnel mal établi ?)." }
        Start-Sleep -Seconds 2
    }
    Clear-DnsClientCache
    $deadline = (Get-Date).AddSeconds(90)
    # Tant que le tunnel n'est pas relié à Caddy, Cloudflare répond lui-même « 200 » avec une page vide : on attend la vraie page
    while (-not (Test-PageServed $publicUrl)) {
        if ((Get-Date) -gt $deadline) { throw "L'adresse $publicUrl ne répond pas après 90 s (tunnel mal établi ?)." }
        Start-Sleep -Seconds 3
    }
    Assert-Secure $publicUrl

    Set-Clipboard -Value $publicUrl
    Write-Host ''
    Write-Host '  ==========================================================' -ForegroundColor Green
    Write-Host '   Le jeu est en ligne. Envoie ce lien à tes amis :' -ForegroundColor Green
    Write-Host ''
    Write-Host "     $publicUrl" -ForegroundColor Yellow
    Write-Host ''
    Write-Host '   (copié dans le presse-papiers)' -ForegroundColor Green
    Write-Host '   Ctrl+C pour tout arrêter. Ne ferme pas cette fenêtre,' -ForegroundColor Green
    Write-Host '   et ne mets pas le PC en veille pendant la partie.' -ForegroundColor Green
    Write-Host '  ==========================================================' -ForegroundColor Green
    Write-Host ''

    while ($true) {
        foreach ($child in @(@('Caddy', $caddyProcess), @('Le tunnel', $tunnelProcess), @('Le serveur', $serverProcess))) {
            if ($child[1].HasExited) { throw "$($child[0]) s'est arrêté (code $($child[1].ExitCode)). Voir $logDir" }
        }
        Start-Sleep -Seconds 2
    }
}
finally {
    Write-Host ''
    Write-Host 'Arrêt du serveur, de Caddy et du tunnel...' -ForegroundColor Cyan
    Stop-Child $serverProcess
    Stop-Child $tunnelProcess
    Stop-Child $caddyProcess
    [OnlineJob]::Close()
    [OnlineJob]::AllowSleep()
    Write-Host 'Tout est arrêté.' -ForegroundColor Green
}
