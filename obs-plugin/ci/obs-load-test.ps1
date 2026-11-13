<#
  CI only: installs PushFPS for OBS from a package folder into a real OBS Studio (latest release, portable
  zip run in normal mode), opens a scene collection that holds one PushFPS source in the program scene, and
  checks the OBS log for the lines given in -Expect. The log is copied to -LogDir.
#>
param(
    [Parameter(Mandatory)] [string]$PackageDir,
    [Parameter(Mandatory)] [string[]]$Expect,
    [string]$StopWhen = '',
    [int]$Seconds = 90,
    [string]$LogDir = 'D:\obs-logs'
)
$ErrorActionPreference = 'Stop'

$rel = gh api repos/obsproject/obs-studio/releases/latest | ConvertFrom-Json
$asset = $rel.assets | Where-Object { $_.name -match 'Windows.*\.zip$' -and $_.name -notmatch 'PDB|arm64' } | Select-Object -First 1
if (-not $asset) { throw "no Windows zip in OBS $($rel.tag_name)" }
Write-Host "OBS Studio $($rel.tag_name): $($asset.name)"
if (-not (Test-Path D:\obs)) {
    curl.exe -sSL --fail -o D:\obs.zip $asset.browser_download_url
    if ($LASTEXITCODE -ne 0) { throw 'OBS download failed' }
    Expand-Archive D:\obs.zip D:\obs -Force
}
Remove-Item D:\obs\portable_mode*, D:\obs\obs_portable_mode* -ErrorAction SilentlyContinue
$obs = Get-ChildItem D:\obs -Recurse -Filter obs64.exe | Select-Object -First 1
if (-not $obs) { throw 'obs64.exe not in the zip' }

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PackageDir 'Install-PushFPS-OBS.ps1')
if ($LASTEXITCODE -ne 0) { throw "plugin installer exit $LASTEXITCODE" }
if (-not (Test-Path "$env:ProgramData\obs-studio\plugins\pushfps-obs\bin\64bit\pushfps-obs.dll")) { throw 'installer did not place the dll' }

$cfg = Join-Path $env:APPDATA 'obs-studio'
New-Item -ItemType Directory -Force "$cfg\basic\scenes" | Out-Null
Remove-Item "$cfg\logs" -Recurse -Force -ErrorAction SilentlyContinue
$src = '6c1d6f52-0d55-4a45-9a70-6b2a3d6f0001'
$collection = @{
    name = 'Untitled'; current_scene = 'Scene'; current_program_scene = 'Scene'
    scene_order = @(@{ name = 'Scene' })
    sources = @(
        @{ id = 'pushfps_source'; versioned_id = 'pushfps_source'; name = 'PushFPS'; uuid = $src
           settings = @{}; enabled = $true; flags = 0; mixers = 0; volume = 1.0 },
        @{ id = 'scene'; versioned_id = 'scene'; name = 'Scene'; uuid = '6c1d6f52-0d55-4a45-9a70-6b2a3d6f0002'
           settings = @{ id_counter = 1; items = @(@{ name = 'PushFPS'; source_uuid = $src; visible = $true; id = 1
                                                       pos = @{ x = 0; y = 0 }; scale = @{ x = 1; y = 1 }; rot = 0; align = 5 }) } }
    )
}
$collection | ConvertTo-Json -Depth 10 | Set-Content "$cfg\basic\scenes\Untitled.json" -Encoding utf8

$p = Start-Process $obs.FullName -WorkingDirectory $obs.DirectoryName -PassThru `
    -ArgumentList '--disable-updater', '--disable-shutdown-check', '--disable-missing-files-check', '--multi'
$deadline = (Get-Date).AddSeconds($Seconds); $log = ''; $file = $null
while ((Get-Date) -lt $deadline) {
    Start-Sleep 5
    $file = Get-ChildItem "$cfg\logs" -Filter *.txt -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
    if ($file) { $log = Get-Content $file.FullName -Raw }
    if (($StopWhen -and $log -match $StopWhen) -or $p.HasExited) { break }
}
Start-Sleep 3
if ($file) { $log = Get-Content $file.FullName -Raw }
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
New-Item -ItemType Directory -Force $LogDir | Out-Null
if ($file) { Copy-Item $file.FullName $LogDir }
($log -split "`n") | Where-Object { $_ -match 'pushfps|PushFPS|game-capture|window-capture|D3D11|adapter' } | Select-Object -First 150 | Out-Host
$missing = @($Expect | Where-Object { $log -notmatch $_ })
if ($missing.Count) { $missing | ForEach-Object { Write-Host "::error::OBS log is missing: $_" }; throw 'OBS load test failed' }
Write-Host 'All expected OBS log lines found.'
