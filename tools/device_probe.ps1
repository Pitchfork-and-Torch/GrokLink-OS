# Probe Flipper / STM32 USB serial for GrokLink OS CDC or v2 CLI
param(
  [string]$Port = "",
  [int]$Baud = 230400,
  [int]$TimeoutMs = 1500
)

$ErrorActionPreference = "Continue"
Write-Host "=== GrokLink device probe ==="

# Present COM ports
$ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
Write-Host "System COM ports: $($ports -join ', ')"

Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
  Where-Object { $_.InstanceId -match 'VID_0483|VID_316D|Serial' -or $_.FriendlyName -match 'Flipper|STM|Serial|COM' } |
  Select-Object Status, FriendlyName, InstanceId |
  Format-Table -Wrap

function Test-Port([string]$p) {
  Write-Host "`n--- Testing $p @ $Baud ---"
  try {
    $sp = New-Object System.IO.Ports.SerialPort $p, $Baud, None, 8, One
    $sp.ReadTimeout = $TimeoutMs
    $sp.WriteTimeout = $TimeoutMs
    $sp.NewLine = "`n"
    $sp.DtrEnable = $true
    $sp.RtsEnable = $true
    $sp.Open()
    Start-Sleep -Milliseconds 300
    # Drain
    $null = $sp.ReadExisting()
    # OS 3 JSON ping
    $sp.WriteLine('{"cmd":"ping"}')
    Start-Sleep -Milliseconds 400
    $r = $sp.ReadExisting()
    if (-not $r) {
      # v2 style
      $sp.WriteLine("groklink status")
      Start-Sleep -Milliseconds 500
      $r = $sp.ReadExisting()
    }
    if (-not $r) {
      $sp.WriteLine("")
      Start-Sleep -Milliseconds 200
      $r = $sp.ReadExisting()
    }
    Write-Host "RX ($($r.Length) bytes):"
    Write-Host $r
    $sp.Close()
    if ($r -match 'pong|groklink|GrokLink|version|3\.0') {
      Write-Host "RESULT: $p looks like GrokLink/OS" -ForegroundColor Green
      return $true
    }
    if ($r.Length -gt 0) {
      Write-Host "RESULT: $p answered but not recognized as GrokLink" -ForegroundColor Yellow
      return $true
    }
    Write-Host "RESULT: $p open OK but silent" -ForegroundColor Yellow
    return $false
  } catch {
    Write-Host "RESULT: $p FAIL $_" -ForegroundColor Red
    return $false
  }
}

$ok = $false
if ($Port) {
  $ok = Test-Port $Port
} else {
  foreach ($p in $ports) {
    if ($p -match 'COM\d+') {
      if (Test-Port $p) { $ok = $true; break }
    }
  }
}

if (-not $ok) {
  Write-Host "`nNo responsive GrokLink serial device." -ForegroundColor Red
  Write-Host "If you flashed OS bring-up DFU: no USB stack yet (see docs/FIELD_TEST_BRINGUP.md)."
  Write-Host "Restore lab: DFU mode (BACK+USB) -> GrokLink-v2.1.3.dfu"
  Write-Host "Native path: docs/ROADMAP_NATIVE.md Phase P1 USB CDC"
  exit 1
}
exit 0
