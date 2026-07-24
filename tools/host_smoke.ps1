# Host smoke: build, test, short OS run, optional RPC ping
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

cmake -B build -DGLK_PLATFORM_HOST=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

$env:GLK_SD_ROOT = Join-Path $Root "sd_card\groklink"
$env:GLK_RPC_PORT = "7341"
$env:GLK_RUN_MS = "4000"

$exe = Join-Path $Root "build\Release\groklink_os.exe"
if (-not (Test-Path $exe)) { $exe = Join-Path $Root "build\groklink_os.exe" }
if (-not (Test-Path $exe)) { $exe = Join-Path $Root "build\groklink_os" }

Write-Host "Starting $exe for smoke..."
$p = Start-Process -FilePath $exe -PassThru -NoNewWindow
Start-Sleep -Seconds 1
try {
  py -3 -c "from groklink_os.rpc.client import GrokLinkClient; c=GrokLinkClient(); c.connect(); print(c.ping()); c.edu_ack(); print(c.status()); c.close()"
} catch {
  Write-Host "Bridge ping skipped/failed (install bridge editable): $_"
}
Wait-Process -Id $p.Id -Timeout 15 -ErrorAction SilentlyContinue
Write-Host "Smoke done."
