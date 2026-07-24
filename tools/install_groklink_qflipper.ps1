# Install GrokLink OS via qFlipper-cli DFU path (same engine as "Install from file").
# qFlipper GUI will NOT fully manage GrokLink (no Flipper protobuf) — DFU flash only.
# Device must be in STM32 DFU: BACK+OK ~30s → "DFU in FS Mode" (0483:DF11).
param(
  [string]$DfuPath = "dist\dfu\GrokLink-OS-v3.6.1-radio.dfu",
  [int]$WaitSeconds = 120
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root
if (-not [IO.Path]::IsPathRooted($DfuPath)) { $DfuPath = Join-Path $Root $DfuPath }
if (-not (Test-Path $DfuPath)) { throw "Missing DFU: $DfuPath" }

$cli = "C:\Program Files\qFlipper\qFlipper-cli.exe"
if (-not (Test-Path $cli)) {
  $cli = "C:\Program Files (x86)\qFlipper\qFlipper-cli.exe"
}
if (-not (Test-Path $cli)) {
  throw "qFlipper-cli not found. Install qFlipper from https://flipperzero.one/update then re-run."
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " GrokLink OS installer (via qFlipper DFU backend)"
Write-Host " File: $DfuPath"
Write-Host "============================================================"
Write-Host ""
Write-Host "What qFlipper CAN do here: flash a .dfu while device is in DFU mode."
Write-Host "What qFlipper CANNOT do: Flipper apps, file manager, or protobuf RPC"
Write-Host "  for GrokLink OS (different operating system)."
Write-Host ""
Write-Host "1) Unplug USB"
Write-Host "2) Hold BACK + OK ~30s"
Write-Host "3) Plug USB while holding; release after DFU appears"
Write-Host "   Windows: DFU in FS Mode  VID_0483 PID_DF11"
Write-Host ""

function Test-Dfu {
  $null -ne (Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match 'VID_0483&PID_DF11' })
}

Get-Process qFlipper -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

$ok = $false
for ($i = 0; $i -lt $WaitSeconds; $i++) {
  if (Test-Dfu) {
    Write-Host "DFU detected at t=${i}s" -ForegroundColor Green
    $ok = $true
    break
  }
  if (($i % 5) -eq 0) { Write-Host "  ... waiting for DFU ($i s)" }
  Start-Sleep -Seconds 1
}
if (-not $ok) { throw "DFU not found. Enter BACK+OK bootloader and retry." }

Write-Host "Flashing with qFlipper-cli..." -ForegroundColor Cyan
& $cli -d 2 firmware $DfuPath
$code = $LASTEXITCODE
Write-Host "qFlipper-cli exit=$code (protobuf timeout after flash is NORMAL for GrokLink)"

Write-Host ""
Write-Host "After reboot, Windows should show USB serial for GrokLink OS:"
Write-Host "  Manufacturer: Pitchfork-and-Torch"
Write-Host "  Product:      GrokLink OS Field Research"
Write-Host "  USB ID:       VID_0483 PID_6C4B  (not Flipper 5740)"
Write-Host ""
Write-Host "Then:"
Write-Host "  groklink-os plug-sync --clear-vault"
Write-Host "  groklink-os ping"
Write-Host ""
Write-Host "Docs: docs/QFLIPPER_AND_WINDOWS.md"
exit 0
