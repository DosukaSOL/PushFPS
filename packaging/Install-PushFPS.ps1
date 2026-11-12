<#
  PushFPS installer.

  PushFPS runs on Veyra's official runtime package (Qt, FFmpeg and the NVIDIA / AMD / Intel components that
  Veyra's author distributes). This script downloads that package from Veyra's own GitHub release, checks its
  SHA-256 against the value pinned when this PushFPS release was built, unpacks it, and adds PushFPS on top.
  No administrator rights are needed: everything goes into your user folder.

  Usage (normally just double-click Install-PushFPS.cmd):
    Install-PushFPS.ps1 [-Gpu Auto|NVIDIA|AMD] [-InstallDir <folder>] [-NoShortcuts] [-NoLaunch]
    Install-PushFPS.ps1 -Uninstall [-InstallDir <folder>]

  Works with Windows PowerShell 5.1 and PowerShell 7.
#>
[CmdletBinding()]
param(
    [ValidateSet('Auto', 'NVIDIA', 'AMD')]
    [string]$Gpu = 'Auto',
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA 'Programs\PushFPS'),
    [switch]$NoShortcuts,
    [switch]$NoLaunch,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# Filled in by packaging/make-release.ps1 when the release is built.
$PushFpsVersion = '@PUSHFPS_VERSION@'
$UpstreamRepo = '@UPSTREAM_REPO@'
$UpstreamTag = '@UPSTREAM_TAG@'
$UpstreamSha256 = @{
    'NVIDIA' = '@SHA256_NVIDIA@'
    'AMD'    = '@SHA256_AMD@'
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$filesDir = Join-Path $here 'PushFPS-files'
$startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) 'PushFPS.lnk'
$desktop = Join-Path ([Environment]::GetFolderPath('Desktop')) 'PushFPS.lnk'

function Step([string]$text) { Write-Host ''; Write-Host "  $text" -ForegroundColor Green }
function Info([string]$text) { Write-Host "    $text" }
function Fail([string]$text) { Write-Host ''; Write-Host "  $text" -ForegroundColor Red; exit 1 }

Write-Host ''
Write-Host "  PushFPS $PushFpsVersion" -ForegroundColor Green
Write-Host '  Based on Veyra by Likely7 (GPL-3.0)'

if ($Uninstall) {
    Step 'Removing PushFPS'
    foreach ($link in @($startMenu, $desktop)) { if (Test-Path $link) { Remove-Item $link -Force } }
    if (Test-Path (Join-Path $InstallDir 'PushFPS.exe')) {
        Remove-Item $InstallDir -Recurse -Force
        Info "Removed $InstallDir"
    } else {
        Info "Nothing installed in $InstallDir"
    }
    exit 0
}

if (-not (Test-Path (Join-Path $filesDir 'PushFPS.exe'))) {
    Fail 'PushFPS-files\PushFPS.exe is missing. Extract the whole zip first, then run the installer from that folder.'
}

if ([Environment]::OSVersion.Version.Major -lt 10 -or -not [Environment]::Is64BitOperatingSystem) {
    Fail 'PushFPS needs 64-bit Windows 10 or 11 (Windows 11 recommended).'
}

# Pick the runtime package that matches the graphics card.
if ($Gpu -eq 'Auto') {
    $names = @(Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | ForEach-Object { $_.Name })
    if ($names -match 'NVIDIA') { $Gpu = 'NVIDIA' } else { $Gpu = 'AMD' }
    Step "Graphics card: $(($names | Where-Object { $_ }) -join ', ')"
    if ($Gpu -eq 'AMD') { Info 'No NVIDIA card found: using the AMD / Intel package (FSR and XeSS frame generation).' }
}

$version = $UpstreamTag.TrimStart('v')
$asset = "Veyra-$version-$Gpu-win64-portable.zip"
$expected = $UpstreamSha256[$Gpu]
$url = "https://github.com/$UpstreamRepo/releases/download/$UpstreamTag/$asset"
$cacheDir = Join-Path $env:TEMP 'PushFPS-download'
$zip = Join-Path $cacheDir $asset
New-Item -ItemType Directory -Force $cacheDir | Out-Null

function Test-Download { (Test-Path $zip) -and ((Get-FileHash $zip -Algorithm SHA256).Hash -ieq $expected) }

if (Test-Download) {
    Step "Using the runtime package downloaded earlier"
} else {
    Step "Downloading the runtime package ($Gpu, about 750 MB) from Veyra's GitHub release"
    Info $url
    $curl = Join-Path $env:SystemRoot 'System32\curl.exe'
    if (Test-Path $curl) {
        & $curl -L --fail --retry 3 --retry-delay 5 -C - -o $zip $url
        if ($LASTEXITCODE -ne 0) { Fail "Download failed (curl exit $LASTEXITCODE). Check your internet connection and run the installer again; it continues where it stopped." }
    } else {
        Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
    }
    if (-not (Test-Download)) {
        Remove-Item $zip -Force -ErrorAction SilentlyContinue
        Fail 'The downloaded file does not match the expected SHA-256, so it was deleted. Run the installer again.'
    }
}
Info "SHA-256 verified: $expected"

Step 'Unpacking'
$stage = Join-Path $env:TEMP ("PushFPS-stage-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $stage | Out-Null
try {
    $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
    if (Test-Path $tar) {
        & $tar -xf $zip -C $stage
        if ($LASTEXITCODE -ne 0) { Fail "Unpacking failed (tar exit $LASTEXITCODE)." }
    } else {
        Expand-Archive -Path $zip -DestinationPath $stage -Force
    }
    $exe = Get-ChildItem $stage -Recurse -Filter 'veyra_qml_ui.exe' | Select-Object -First 1
    if (-not $exe) { Fail 'The runtime package has an unexpected layout (veyra_qml_ui.exe not found).' }
    $runtimeRoot = $exe.DirectoryName

    Step "Installing to $InstallDir"
    $running = Get-Process -Name 'PushFPS' -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($InstallDir, [StringComparison]::OrdinalIgnoreCase) }
    if ($running) { Fail 'PushFPS is running. Close it, then run the installer again.' }
    New-Item -ItemType Directory -Force $InstallDir | Out-Null
    # /E copies every folder; files already there are replaced, your settings folder
    # (runtime_local\user-data-2.0.0) and other extra files stay.
    foreach ($source in @($runtimeRoot, $filesDir)) {
        & robocopy $source $InstallDir /E /R:2 /W:2 /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { Fail "Copying files failed (robocopy exit $LASTEXITCODE)." }
    }
    # One program, one name: PushFPS.exe replaces Veyra's launcher in this folder.
    Remove-Item (Join-Path $InstallDir 'veyra_qml_ui.exe') -Force -ErrorAction SilentlyContinue
    Set-Content -Path (Join-Path $InstallDir 'PushFPS-VERSION.txt') -Value @(
        "PushFPS $PushFpsVersion",
        "Runtime: $asset from https://github.com/$UpstreamRepo/releases/tag/$UpstreamTag",
        "Runtime SHA-256: $expected"
    ) -Encoding UTF8
} finally {
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
}

$target = Join-Path $InstallDir 'PushFPS.exe'
if (-not $NoShortcuts) {
    Step 'Adding Start menu and desktop shortcuts'
    $shell = New-Object -ComObject WScript.Shell
    foreach ($link in @($startMenu, $desktop)) {
        $s = $shell.CreateShortcut($link)
        $s.TargetPath = $target
        $s.WorkingDirectory = $InstallDir
        $s.IconLocation = "$target,0"
        $s.Description = 'PushFPS: console frame generation'
        $s.Save()
    }
}

Step 'PushFPS is installed.'
Info "Program: $target"
Info "Remove it any time with: Install-PushFPS.cmd -Uninstall"
Info "The downloaded package stays in $cacheDir for quick reinstalls; you can delete it."

if (-not $NoLaunch) { Start-Process -FilePath $target -WorkingDirectory $InstallDir }
exit 0
