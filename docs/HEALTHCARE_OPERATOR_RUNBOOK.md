# Healthcare / MedSec operator runbook (GrokLink OS)

**NOT A MEDICAL DEVICE.** Authorized lab and education only.

## 1. Prerequisites

1. GrokLink OS on device (status shows version 3.7+ preferred).
2. SD layout `/ext/groklink/` present (or host sim with `sd_card/groklink`).
3. Written authorization / RoE for every target (own gear or signed engagement).
4. One serial owner: close qFlipper before the PC bridge.

## 2. Install MedSec packs

**Automated (recommended):**

```powershell
cd $env:USERPROFILE\groklink-os
.\scripts\deploy_medsec_packs.ps1 -SdRoot ".\sd_card\groklink" -MedsecStrict
# or mount device EXT and point -SdRoot at /ext/groklink
```

**Manual:**

1. Copy skill folders under `sd_card/groklink/skills/medsec_*`, `fac_rf_*`, `med_asset_*`
   to device `/ext/groklink/skills/<id>/` (if not using ROM catalog alone).
2. Copy missions under `sd_card/groklink/missions/medsec_*` and `fac_rf_*`
   to `/ext/groklink/missions/` (ROM also embeds the same ids).
3. Optional MedSec-strict profile:
   - Copy `config/profile.medsec-strict.json` → `config/profile.json`
   - **All TX forbidden** under this profile.
4. Optional: review healthcare blacklist templates under
   `sd_card/groklink/healthcare/blacklist/` before merging into active blacklist files.
5. Reload / reboot.

## 3. 15-minute passive demo

```powershell
cd bridge
py -3 -m pip install -e ".[serial]"
# $env:GLK_SERIAL_PORT = "COMx"   # if needed

groklink-os ping
groklink-os edu-ack
groklink-os status
# Expect: not_medical_device true when firmware has MedSec status fields

groklink-os lab medsec-demo
# or:
groklink-os mission-run --id medsec_lab_passive_ism --steps 8
groklink-os vault-tail -n 8
```

## 4. Engagement + casefile (evidence)

```powershell
groklink-os lab engagement-init `
  --operator lab-op1 `
  --engagement ENG-2026-001 `
  --site bench-a `
  --profile medsec-strict `
  --roe-ack

groklink-os lab engagement-show

groklink-os lab casefile `
  --dir cases/ENG-2026-001 `
  --title "MedSec passive ISM baseline" `
  --hypothesis "Lab ISM occupancy is quiet under Faraday" `
  --freqs 433920000,315000000 `
  --narrative "Passive only; no TX; not a medical device."
```

## 5. Anomaly + export (never auto-TX)

```powershell
# Use observation store JSONL if available:
# typically under ~/.grok/state/groklink-os/observations/recent.jsonl

groklink-os lab anomaly --history path\to\history.jsonl --out anomaly.json
groklink-os lab export-csv --history path\to\history.jsonl --out export.csv
groklink-os lab export-json --history path\to\history.jsonl --out export.json
groklink-os lab export-research --history path\to\history.jsonl --out research_bundle.json
```

`export-research` is experimental research-shaped data only — **not** clinical FHIR/EHR.

## 6. Policies

| Rule | Value |
|------|--------|
| Autonomous TX missions | Forbidden |
| medsec-strict TX | Forbidden (switch profile only for Faraday dual-control lab) |
| max_risk_class | passive_rx |
| Patient data / PHI on SD | Forbidden |
| Care decisions from edge metrics | Forbidden |

## 7. Stop conditions

- Unexpected clinical device behavior nearby: stop radio, document, escalate.
- Audit integrity concern: stop engagement, preserve storage image.
- Pressure to TX without RoE: refuse.

## 8. See also

- [MEDSEC_WORLDWIDE_NEXT_STEPS.md](MEDSEC_WORLDWIDE_NEXT_STEPS.md)
- [MEDSEC_PILOT_IN_A_BOX.md](MEDSEC_PILOT_IN_A_BOX.md)
- [MEDSEC_RISK_FILE.md](MEDSEC_RISK_FILE.md)
- [SAFETY.md](SAFETY.md)
