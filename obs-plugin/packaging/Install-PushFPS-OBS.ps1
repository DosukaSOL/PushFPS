<#
  PushFPS for OBS Studio: installer.

  Copies pushfps-obs.dll into OBS's per-machine plugin folder
  (%ProgramData%\obs-studio\plugins\pushfps-obs\bin\64bit), which OBS Studio 28 and newer load
  on start. No administrator rights are needed and the OBS installation itself is not touched.

  Usage (normally just double-click Install-PushFPS-OBS.cmd):
    Install-PushFPS-OBS.ps1             install or update
    Install-PushFPS-OBS.ps1 -Uninstall  remove the plugin

  Works with Windows PowerShell 5.1 and PowerShell 7.
#>
[CmdletBinding()]
param([switch]$Uninstall)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$pluginRoot = Join-Path $env:ProgramData 'obs-studio\plugins\pushfps-obs'
$binDir = Join-Path $pluginRoot 'bin\64bit'
$dataDir = Join-Path $pluginRoot 'data'

function Test-ObsRunning { [bool](Get-Process -Name 'obs64' -ErrorAction SilentlyContinue) }

if (Test-ObsRunning) {
    Write-Host 'OBS Studio is running. Close OBS Studio first, then run this again.' -ForegroundColor Yellow
    exit 2
}

if ($Uninstall) {
    if (Test-Path $pluginRoot) {
        Remove-Item -Recurse -Force $pluginRoot
        Write-Host 'PushFPS for OBS was removed.' -ForegroundColor Green
    } else {
        Write-Host 'PushFPS for OBS is not installed.'
    }
    exit 0
}

$dll = Join-Path $here 'pushfps-obs\bin\64bit\pushfps-obs.dll'
if (-not (Test-Path $dll)) { throw "pushfps-obs.dll not found next to this script ($dll). Unzip the whole download first." }

$obs = $null
foreach ($key in 'HKLM:\SOFTWARE\OBS Studio', 'HKLM:\SOFTWARE\WOW6432Node\OBS Studio') {
    $item = Get-ItemProperty -Path $key -ErrorAction SilentlyContinue
    if ($item -and $item.'(default)') { $obs = $item.'(default)'; break }
}
if (-not $obs -and (Test-Path "$env:ProgramFiles\obs-studio\bin\64bit\obs64.exe")) { $obs = "$env:ProgramFiles\obs-studio" }
if ($obs) { Write-Host "OBS Studio found: $obs" } else {
    Write-Host 'OBS Studio was not found. The plugin is installed anyway and loads once OBS Studio 28 or newer is installed.' -ForegroundColor Yellow
}

New-Item -ItemType Directory -Force $binDir, $dataDir | Out-Null
Copy-Item -Force $dll $binDir
$readme = Join-Path $here 'READ ME FIRST.txt'
if (Test-Path $readme) { Copy-Item -Force $readme (Join-Path $dataDir 'README.txt') }

Write-Host ''
Write-Host 'PushFPS for OBS is installed.' -ForegroundColor Green
Write-Host 'Start OBS Studio, then in a scene: + (Add source) > PushFPS.'
Write-Host 'Tools > Open PushFPS starts the app with OBS game capture support.'
exit 0
