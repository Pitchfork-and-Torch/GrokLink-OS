# Watch for GrokLink CDC (VID 0483 PID 5740) and run plug-sync on each reconnect.
# Run in background while you unplug/plug the Flipper for field research.
# Local learning store only — never auto-TX.
param(
  [int]$PollSeconds = 3,
  [switch]$ClearVaultAfterIngest
)

$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root
$env:GROKLINK_OS_ROOT = $Root

function Test-GrokLinkCdc {
  $d = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
    Where-Object { $_.InstanceId -match 'VID_0483&PID_5740' }
  return $null -ne $d
}

Write-Host "GrokLink plug-sync watcher — Ctrl+C to stop"
Write-Host "On each USB reconnect: groklink-os plug-sync"
Write-Host "Learn dir: $env:USERPROFILE\.grok\state\groklink-os\"
Write-Host ""

$wasPresent = Test-GrokLinkCdc
if ($wasPresent) {
  Write-Host "[$(Get-Date -Format o)] Device already present — running initial plug-sync..."
  $args = @("-3", "-m", "groklink_os.cli", "plug-sync")
  if ($ClearVaultAfterIngest) { $args += "--clear-vault" }
  # Prefer installed entrypoint
  try {
    if ($ClearVaultAfterIngest) {
      groklink-os plug-sync --clear-vault
    } else {
      groklink-os plug-sync
    }
  } catch {
    py @args
  }
}

while ($true) {
  Start-Sleep -Seconds $PollSeconds
  $now = Test-GrokLinkCdc
  if ($now -and -not $wasPresent) {
    Write-Host ""
    Write-Host "[$(Get-Date -Format o)] Reconnect detected — ingesting unplugged lessons..."
    Start-Sleep -Seconds 1
    try {
      if ($ClearVaultAfterIngest) {
        groklink-os plug-sync --clear-vault
      } else {
        groklink-os plug-sync
      }
    } catch {
      $extra = @()
      if ($ClearVaultAfterIngest) { $extra = @("--clear-vault") }
      py -3 -c "from groklink_os.research.plug_sync import ingest_unplugged_lessons; import json; print(json.dumps(ingest_unplugged_lessons(clear_vault=$([string]$ClearVaultAfterIngest.IsPresent).ToLower()).to_dict(), indent=2))"
    }
    Write-Host "[$(Get-Date -Format o)] Ingest pass done."
  }
  if (-not $now -and $wasPresent) {
    Write-Host "[$(Get-Date -Format o)] Device unplugged — field agent may continue offline."
  }
  $wasPresent = $now
}
