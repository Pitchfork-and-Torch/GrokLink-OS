# Recover Flipper Zero from experimental OS / bad flash via DFU.
# Device must show as USB "DFU in FS Mode" (VID_0483 PID_DF11).
# Hold BACK+OK ~30s while plugging USB if not already in DFU.
param(
  [string]$DfuPath = "",
  [int]$DebugLevel = 2
)

$ErrorActionPreference = "Stop"

if (-not $DfuPath) {
  $home = $env:USERPROFILE
  $cands = @(
    (Join-Path $home "Desktop\GrokLink-qFlipper-install\GrokLink-v2.1.3.dfu"),
    (Join-Path $home "GrokLink-Firmware\dist\GrokLink-v2.1.3.dfu"),
    (Join-Path $home "Downloads\GrokLink-v2.1.3.dfu")
  )
  foreach ($c in $cands) {
    if (Test-Path $c) { $DfuPath = $c; break }
  }
}
if (-not $DfuPath -or -not (Test-Path $DfuPath)) {
  throw "No recovery DFU found. Pass -DfuPath to a full GrokLink/Momentum/official .dfu"
}

$cli = "C:\Program Files\qFlipper\qFlipper-cli.exe"
if (-not (Test-Path $cli)) { throw "qFlipper-cli not found at $cli" }

$dfuDev = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
  Where-Object { $_.InstanceId -match 'VID_0483&PID_DF11' }
if (-not $dfuDev) {
  Write-Host "No DFU device (0483:DF11). Hold BACK+OK ~30s, plug USB, then re-run." -ForegroundColor Yellow
  Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match 'VID_0483|VID_316D' } |
    Format-Table Status, FriendlyName, InstanceId -Wrap
  exit 2
}

Write-Host "DFU device OK. Flashing $DfuPath ..."
Get-Process qFlipper -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
& $cli -d $DebugLevel firmware $DfuPath
Write-Host "qFlipper-cli exit=$LASTEXITCODE"
