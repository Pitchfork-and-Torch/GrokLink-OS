# USB CDC soak: open/close ping. Returns in under 8s. No never-exit.
# Exit 0: soak ok OR no device (SKIP). Exit 1: device present but soak failed.
param(
    [string]$Port = "",
    [int]$Cycles = 3,
    [int]$Baud = 230400
)

$ErrorActionPreference = "Continue"
$deadline = (Get-Date).AddSeconds(7)
Write-Host "GrokLink USB soak v3.9 (fail-closed, no hang)"

function Get-GlkPort {
    if ($Port) { return $Port }
    $pnp = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
        Where-Object { $_.InstanceId -match 'VID_0483' -and ($_.FriendlyName -match 'GrokLink|STM|Serial' -or $_.InstanceId -match 'PID_6C4B|PID_5740') }
    foreach ($d in $pnp) {
        if ($d.FriendlyName -match '(COM\d+)') { return $Matches[1] }
    }
    return ""
}

$p = Get-GlkPort
if (-not $p) {
    Write-Host "SKIP no serial device"
    exit 0
}

$ok = 0
for ($i = 1; $i -le $Cycles; $i++) {
    if ((Get-Date) -gt $deadline) {
        Write-Host "FAIL timeout"
        exit 1
    }
    $sp = $null
    try {
        $sp = New-Object System.IO.Ports.SerialPort $p, $Baud, None, 8, One
        $sp.ReadTimeout = 800
        $sp.WriteTimeout = 800
        $sp.NewLine = "`n"
        $sp.DtrEnable = $true
        $sp.Open()
        Start-Sleep -Milliseconds 200
        $null = $sp.ReadExisting()
        $sp.WriteLine('{"cmd":"ping"}')
        Start-Sleep -Milliseconds 250
        $r = $sp.ReadExisting()
        $sp.Close()
        if ($r -match '"ok"\s*:\s*true') { $ok++ ; Write-Host "cycle $i ok" }
        else { Write-Host "cycle $i no-pong: $r" }
    } catch {
        Write-Host "cycle $i err $_"
        if ($sp) { try { $sp.Close() } catch {} }
    }
}

if ($ok -ge 1) {
    Write-Host "SOAK_OK cycles=$ok/$Cycles"
    exit 0
}
Write-Host "FAIL no successful ping"
exit 1
