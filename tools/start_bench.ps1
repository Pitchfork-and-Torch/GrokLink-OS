# Launch Bench Desktop out of the agent job if possible. Returns immediately.
$ErrorActionPreference = "Stop"
$bridge = Join-Path (Split-Path -Parent $PSScriptRoot) "bridge"
Write-Host "Starting GrokLink Bench from $bridge (observe only)"
Start-Process -FilePath "py" -ArgumentList @("-3", "-m", "groklink_os.bench") -WorkingDirectory $bridge
Write-Host "STARTED (if the TUI stays on top, Alt-Tab to GrokLink Bench)"
exit 0
