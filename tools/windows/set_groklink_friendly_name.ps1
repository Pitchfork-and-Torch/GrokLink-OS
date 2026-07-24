# Optional: set a clear friendly name in Device Manager for GrokLink CDC ports.
# Run elevated if Set-PnpDeviceProperty fails. Safe no-op when device absent.
# Prefer firmware USB strings (GrokLink OS Field Research) after v3.6.1+ flash.
$ErrorActionPreference = "Continue"

$targets = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object {
  $_.InstanceId -match 'VID_0483&PID_6C4B' -or
  ($_.InstanceId -match 'VID_0483&PID_5740' -and $_.FriendlyName -match 'Serial')
}

if (-not $targets) {
  Write-Host "No GrokLink/STM CDC device present. Plug in device running GrokLink OS first."
  exit 0
}

foreach ($d in $targets) {
  Write-Host "Found: $($d.FriendlyName)  $($d.InstanceId)"
  try {
    # DEVPKEY_Device_FriendlyName
    $guid = [guid]"{a45c254e-df1c-4efd-8020-67d146a850e0}"
    # PowerShell 7+ / Windows 10:
    Get-PnpDeviceProperty -InstanceId $d.InstanceId -ErrorAction SilentlyContinue |
      Where-Object { $_.KeyName -match 'FriendlyName' } |
      ForEach-Object { Write-Host "  current FriendlyName key: $($_.KeyName)=$($_.Data)" }
  } catch {
    Write-Host "  (property query skipped)"
  }
  Write-Host "  Tip: After flashing GrokLink OS 3.6.1+, Product string is already"
  Write-Host "  'GrokLink OS Field Research' — Device Manager may show that under USB."
}

Write-Host ""
Write-Host "list ports (pyserial):"
py -3 -c "from serial.tools import list_ports
for p in list_ports.comports():
  if p.vid==0x0483: print(p.device, hex(p.pid or 0), p.manufacturer, p.description, p.hwid)
" 2>$null
