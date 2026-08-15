param(
    [Parameter(Mandatory = $true)]
    [string] $DevFlash,

    [ValidateSet('gl', 'vulkan')]
    [string] $Renderer = 'gl',

    [int] $Seconds = 60,

    [switch] $StaticCpu,
    [switch] $TraceEvents,
    [switch] $TraceFs,
    [switch] $TraceAv,
    [switch] $TraceThreads,
    [switch] $TraceStack,
    [string] $LogPath,
    [switch] $Interactive
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$probe = Join-Path $repoRoot 'build-pc-rpcs3\vshift_rpcs3_boot_probe.exe'

if (-not (Test-Path -LiteralPath $probe -PathType Leaf)) {
    throw "Probe не найден: $probe. Сначала соберите target vshift_rpcs3_boot_probe."
}
if (-not (Test-Path -LiteralPath $DevFlash -PathType Container)) {
    throw "Каталог dev_flash не найден: $DevFlash"
}

# RPCS3/FFmpeg runtime DLLs are supplied by the MSYS2 UCRT64 toolchain.
$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:Path"
$env:VSHIFT_FULLSCREEN = '1'
$env:VSHIFT_HEADLESS_GL = '1'
$env:VSHIFT_OFFICIAL_PROFILE = '1'
$env:VSHIFT_SKIP_UI_OVERLAY = '1'
$env:VSHIFT_TRACE_BOOT = '1'
$env:VSHIFT_TRACE_PRX = '1'
$env:VSHIFT_TRACE_VIDEO = '1'
$env:VSHIFT_TRACE_FLIP = '1'
$env:VSHIFT_GL_WAIT_SECONDS = [string]([Math]::Max(1, $Seconds))

if ($StaticCpu) {
    $env:VSHIFT_PPU_STATIC = '1'
    $env:VSHIFT_SPU_STATIC = '1'
} else {
    Remove-Item Env:VSHIFT_PPU_STATIC -ErrorAction SilentlyContinue
    Remove-Item Env:VSHIFT_SPU_STATIC -ErrorAction SilentlyContinue
}
if ($TraceEvents) {
    $env:VSHIFT_TRACE_EVENTS = '1'
} else {
    Remove-Item Env:VSHIFT_TRACE_EVENTS -ErrorAction SilentlyContinue
}
if ($TraceFs) {
    $env:VSHIFT_TRACE_FS = '1'
} else {
    Remove-Item Env:VSHIFT_TRACE_FS -ErrorAction SilentlyContinue
}
if ($TraceAv) {
    $env:VSHIFT_TRACE_AV = '1'
} else {
    Remove-Item Env:VSHIFT_TRACE_AV -ErrorAction SilentlyContinue
}
if ($TraceThreads) {
    $env:VSHIFT_TRACE_THREADS = '1'
} else {
    Remove-Item Env:VSHIFT_TRACE_THREADS -ErrorAction SilentlyContinue
}
if ($TraceStack) {
    $env:VSHIFT_TRACE_STACK = '1'
} else {
    Remove-Item Env:VSHIFT_TRACE_STACK -ErrorAction SilentlyContinue
}
if ($Interactive) {
    $env:VSHIFT_INTERACTIVE = '1'
} else {
    Remove-Item Env:VSHIFT_INTERACTIVE -ErrorAction SilentlyContinue
}

$rendererArg = if ($Renderer -eq 'vulkan') { '--vulkan' } else { '--gl' }
Write-Host "VSHift live probe: $DevFlash ($Renderer, ${Seconds}s)"
if ($LogPath) {
    $logDirectory = Split-Path -Parent $LogPath
    if ($logDirectory) {
        New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
    }
    Write-Host "Логи выводятся в реальном времени и сохраняются: $LogPath"
    & $probe $DevFlash $rendererArg 2>&1 | Tee-Object -FilePath $LogPath
} else {
    Write-Host 'Логи ниже выводятся напрямую; окно закрывается по таймеру или WM_CLOSE.'
    & $probe $DevFlash $rendererArg
}
exit $LASTEXITCODE
