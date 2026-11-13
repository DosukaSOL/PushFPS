<#
  Builds the PushFPS release zip from a finished build (used by .github/workflows/build-windows.yml).

  Zip layout:
    Install-PushFPS.cmd        double-click to install
    Install-PushFPS.ps1        the installer (Veyra runtime package hashes pinned in it)
    READ ME FIRST.txt
    PushFPS-files\             PushFPS.exe, its QML interface, shaders and licence files
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$UpstreamRepo,
    [Parameter(Mandatory)][string]$UpstreamTag,
    [Parameter(Mandatory)][string]$UpstreamSums,
    [string]$OutDir = 'dist'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

# SHA-256 of Veyra's runtime packages, from the release's own SHA256SUMS.txt.
$upstreamVersion = $UpstreamTag.TrimStart('v')
$sums = @{}
foreach ($line in Get-Content $UpstreamSums) {
    if ($line -match '^([0-9a-fA-F]{64})\s+\*?(.+?)\s*$') { $sums[$Matches[2]] = $Matches[1].ToUpperInvariant() }
}
$hash = @{}
foreach ($gpu in 'NVIDIA', 'AMD') {
    $name = "Veyra-$upstreamVersion-$gpu-win64-portable.zip"
    if (-not $sums.ContainsKey($name)) { throw "SHA256SUMS.txt has no entry for $name" }
    $hash[$gpu] = $sums[$name]
}

$name = "PushFPS-$Version-win64"
# The exact source of this build: its release tag, else the commit CI built, else the main branch.
$sourceRef = if ($Version -match '^\d+\.\d+\.\d+$') { "v$Version" } elseif ($env:GITHUB_SHA) { $env:GITHUB_SHA } else { 'main' }
$stage = Join-Path $OutDir $name
$files = Join-Path $stage 'PushFPS-files'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $files | Out-Null

# The program and everything built next to it that the runtime package does not already have newer.
Copy-Item (Join-Path $BuildDir 'PushFPS.exe') $files
Copy-Item (Join-Path $repo 'qml') $files -Recurse
$shaders = Join-Path $BuildDir 'shaders'
if (-not (Test-Path $shaders)) { throw "Compiled shaders not found in $shaders" }
Copy-Item $shaders $files -Recurse
foreach ($doc in 'LICENSE', 'NOTICE.md', 'THIRD_PARTY_NOTICES.md') { Copy-Item (Join-Path $repo $doc) $files }
Copy-Item (Join-Path $repo 'README.md') (Join-Path $files 'PushFPS-README.md')
Copy-Item (Join-Path $repo 'licenses') $files -Recurse

# Installer with the pinned values filled in.
$installer = Get-Content (Join-Path $PSScriptRoot 'Install-PushFPS.ps1') -Raw
$installer = $installer.Replace('@PUSHFPS_VERSION@', $Version)
$installer = $installer.Replace('@UPSTREAM_REPO@', $UpstreamRepo)
$installer = $installer.Replace('@UPSTREAM_TAG@', $UpstreamTag)
$installer = $installer.Replace('@SHA256_NVIDIA@', $hash['NVIDIA'])
$installer = $installer.Replace('@SHA256_AMD@', $hash['AMD'])
if ($installer -match '@[A-Z0-9_]+@') { throw "Unfilled placeholder in installer: $($Matches[0])" }
# UTF-8 with BOM so Windows PowerShell 5.1 reads non-ASCII text correctly.
[IO.File]::WriteAllText((Join-Path $stage 'Install-PushFPS.ps1'), $installer, (New-Object Text.UTF8Encoding $true))
Copy-Item (Join-Path $PSScriptRoot 'Install-PushFPS.cmd') $stage

@"
PushFPS $Version
================

Install
  1. Extract this whole zip to a folder (right-click, Extract All).
  2. Double-click Install-PushFPS.cmd.

  The installer downloads Veyra's official runtime package (about 750 MB, NVIDIA or AMD to match your
  graphics card) from https://github.com/$UpstreamRepo/releases/tag/$UpstreamTag , checks its SHA-256,
  and installs PushFPS to %LOCALAPPDATA%\Programs\PushFPS with Start menu and desktop shortcuts.
  No administrator rights are needed.

  Windows may warn that the file comes from the internet or an unknown publisher (PushFPS is not code
  signed). Choose "More info" and "Run anyway" if you downloaded this zip from
  https://github.com/DosukaSOL/PushFPS/releases .

Uninstall
  Run Install-PushFPS.cmd -Uninstall from a command prompt in this folder,
  or delete %LOCALAPPDATA%\Programs\PushFPS and the PushFPS shortcuts.

Pinned runtime package hashes (SHA-256)
  NVIDIA: $($hash['NVIDIA'])
  AMD:    $($hash['AMD'])

License
  PushFPS is free software under the GNU GPL-3.0 and comes with ABSOLUTELY NO WARRANTY. It is a modified
  version of Veyra by Likely7 (https://github.com/Likely7/Veyra-NRVideo). The license, the modification
  notice and third-party notices are in PushFPS-files (LICENSE, NOTICE.md, THIRD_PARTY_NOTICES.md).
  Source code of this release: https://github.com/DosukaSOL/PushFPS/tree/$sourceRef
"@ | Set-Content -Path (Join-Path $stage 'READ ME FIRST.txt') -Encoding UTF8

$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
$sha = (Get-FileHash $zip -Algorithm SHA256).Hash
"$sha  $name.zip" | Set-Content -Path (Join-Path $OutDir "$name.zip.sha256") -Encoding ASCII
Write-Host "Created $zip"
Write-Host "SHA-256 $sha"
