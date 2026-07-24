# Flash GrokLink OS DFU ONLY when device is in STM32 DFU mode (0483:DF11).
# Do NOT use plain "qFlipper-cli firmware" while GrokLink CDC (5740) is running —
# qFlipper expects Flipper protobuf RPC and will hang/timeout.
param(
  [string]$DfuPath = "dist\dfu\GrokLink-OS-v3.7.0-radio.dfu",
  [int]$WaitSeconds = 120
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root
if (-not [IO.Path]::IsPathRooted($DfuPath)) { $DfuPath = Join-Path $Root $DfuPath }
if (-not (Test-Path $DfuPath)) { throw "Missing $DfuPath" }

$cli = "C:\Program Files\qFlipper\qFlipper-cli.exe"
if (-not (Test-Path $cli)) { throw "qFlipper-cli not found" }

function Test-Dfu {
  $d = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match 'VID_0483&PID_DF11' }
  return $null -ne $d
}

function Test-Cdc {
  $d = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match 'VID_0483&PID_5740' }
  return $null -ne $d
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " GrokLink OS DFU flash (bootloader mode only)"
Write-Host " File: $DfuPath"
Write-Host "============================================================"
Write-Host ""
Write-Host "1) Unplug the USB cable from the Flipper."
Write-Host "2) Hold BACK + OK together for ~30 seconds (bootloader entry)."
Write-Host "3) While still holding, plug USB back in."
Write-Host "4) Release after Windows shows DFU (or after ~2 s on cable)."
Write-Host "   Windows should show:  DFU in FS Mode  (VID_0483 PID_DF11)"
Write-Host "   NOT 'USB Serial Device' / COM7."
Write-Host ""
Write-Host "Waiting up to $WaitSeconds s for DFU..."

Get-Process qFlipper -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

$ok = $false
for ($i = 0; $i -lt $WaitSeconds; $i++) {
  if (Test-Dfu) {
    Write-Host "DFU detected at t=${i}s" -ForegroundColor Green
    $ok = $true
    break
  }
  if (($i % 5) -eq 0) {
    $mode = if (Test-Cdc) { "CDC app mode (wrong for this step)" } else { "no STM32 USB" }
    Write-Host "  ... $i s  ($mode)"
  }
  Start-Sleep -Seconds 1
}

if (-not $ok) {
  Write-Host ""
  Write-Host "DFU never appeared. Common issues:" -ForegroundColor Yellow
  Write-Host "  - Released BACK before USB was plugged"
  Write-Host "  - qFlipper GUI still open (close it)"
  Write-Host "  - Wrong USB cable (charge-only)"
  Write-Host "  - Device still showing COM7 = still in app mode"
  Write-Host ""
  Write-Host "Retry this script after entering DFU."
  exit 2
}

Write-Host "Flashing..." -ForegroundColor Cyan
& $cli -d 2 firmware $DfuPath
$code = $LASTEXITCODE
Write-Host "qFlipper-cli exit=$code"

Start-Sleep -Seconds 3
Write-Host ""
Write-Host "After flash, open a serial terminal on the new COM port @ 230400 and send:"
Write-Host '  {"cmd":"ping"}'
Write-Host '  {"cmd":"edu_ack","phrase":"I_WILL_USE_ONLY_AUTHORIZED_TARGETS"}'
Write-Host '  {"cmd":"status"}'
Write-Host '  {"cmd":"subghz_rx","freq_hz":433920000,"ms":400}'
Write-Host ""
Write-Host "Restore lab firmware anytime:  .\tools\recover_flipper.ps1"

# Best-effort probe
Start-Sleep -Seconds 2
py -3 -c @"
from serial.tools import list_ports
import serial, time
for p in list_ports.comports():
    if p.vid==0x0483 and p.pid==0x5740:
        try:
            s=serial.Serial(p.device,230400,timeout=1.2); time.sleep(0.5)
            s.write(b'{\"cmd\":\"ping\"}\n'); time.sleep(0.6)
            print(p.device, s.read(300)); s.close()
        except Exception as e:
            print(p.device, e)
"@

exit $code
