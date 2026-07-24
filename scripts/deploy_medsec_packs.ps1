#Requires -Version 5.1
<#
.SYNOPSIS
  Deploy MedSec packs + optional medsec-strict profile into a GrokLink SD root.

.PARAMETER SdRoot
  Destination sd_card/groklink or mounted device /ext/groklink path.

.PARAMETER MedsecStrict
  Write profile.json as medsec-strict (all TX forbidden).

.EXAMPLE
  .\scripts\deploy_medsec_packs.ps1 -SdRoot "E:\FLIPPER\groklink" -MedsecStrict
#>
param(
  [Parameter(Mandatory = $true)]
  [string]$SdRoot,
  [switch]$MedsecStrict
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$RepoSd = Join-Path $RepoRoot "sd_card\groklink"
if (-not (Test-Path -LiteralPath $RepoSd)) {
  throw "Cannot find repo sd_card/groklink at $RepoSd"
}

if (-not (Test-Path -LiteralPath $SdRoot)) {
  New-Item -ItemType Directory -Force -Path $SdRoot | Out-Null
}
$SdRoot = (Resolve-Path -LiteralPath $SdRoot).Path
$RepoSd = (Resolve-Path -LiteralPath $RepoSd).Path
$sameTree = ($SdRoot -eq $RepoSd)

Write-Host "Deploy MedSec packs" -ForegroundColor Cyan
Write-Host "  from: $RepoSd"
Write-Host "  to:   $SdRoot"
if ($sameTree) {
  Write-Host "  (same tree: packs already present; applying profile only if requested)" -ForegroundColor Yellow
}

function Copy-Rel([string]$rel) {
  $src = Join-Path $RepoSd $rel
  $dst = Join-Path $SdRoot $rel
  if (-not (Test-Path -LiteralPath $src)) {
    Write-Host "  skip missing $rel" -ForegroundColor Yellow
    return
  }
  if ($sameTree) {
    Write-Host "  ok $rel (in-tree)" -ForegroundColor Green
    return
  }
  $parent = Split-Path $dst -Parent
  New-Item -ItemType Directory -Force -Path $parent | Out-Null
  if (Test-Path -LiteralPath $src -PathType Container) {
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dst -Recurse -Force
  } else {
    Copy-Item -Force -LiteralPath $src -Destination $dst
  }
  Write-Host "  ok $rel" -ForegroundColor Green
}

if (-not $sameTree) {
  Copy-Rel "healthcare"
  foreach ($name in @(
      "medsec_lab_passive_ism.mission.json",
      "fac_rf_snapshot_passive.mission.json",
      "medsec_passive_watch.mission.json"
    )) {
    Copy-Rel ("missions\" + $name)
  }
  foreach ($sk in @("medsec_passive_ism_watch", "fac_rf_baseline_watch", "med_asset_uid_inventory")) {
    Copy-Rel ("skills\" + $sk)
  }
}

$cfg = Join-Path $SdRoot "config"
New-Item -ItemType Directory -Force -Path $cfg | Out-Null
if ($MedsecStrict) {
  $srcProf = Join-Path $RepoSd "config\profile.medsec-strict.json"
  Copy-Item -Force -LiteralPath $srcProf -Destination (Join-Path $cfg "profile.json")
  Write-Host "  profile -> medsec-strict (TX forbidden)" -ForegroundColor Green
} else {
  $dstProf = Join-Path $cfg "profile.json"
  if (-not (Test-Path -LiteralPath $dstProf)) {
    Copy-Item -Force -LiteralPath (Join-Path $RepoSd "config\profile.json") -Destination $dstProf
    Write-Host "  profile -> default" -ForegroundColor Green
  } else {
    Write-Host "  profile unchanged ($dstProf)" -ForegroundColor Green
  }
}

Write-Host ""
Write-Host "NOT A MEDICAL DEVICE. Reboot device after SD copy so policy reloads profile." -ForegroundColor Yellow
Write-Host "Next: groklink-os edu-ack; groklink-os lab medsec-demo" -ForegroundColor Cyan
