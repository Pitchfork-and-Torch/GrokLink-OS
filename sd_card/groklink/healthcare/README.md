# Healthcare / MedSec packs (GrokLink OS)

**NOT A MEDICAL DEVICE.** Authorized research, education, and MedSec lab work only.

Copy skills and missions into the live `/ext/groklink/` (or host `sd_card/groklink/`) tree
as described in `docs/HEALTHCARE_OPERATOR_RUNBOOK.md`.

## Contents

| Path | Purpose |
|------|---------|
| blacklist/*.template.json | Optional TX policy starters (review before merge) |
| missions/* | Passive-only mission JSON (also in ROM catalog) |
| skills/* | passive_rx skill packages |

## Enable MedSec-strict profile

1. Copy `config/profile.medsec-strict.json` over `config/profile.json`, **or**
2. Set `"profile": "medsec-strict"` in `config/profile.json`
3. Reload blacklist/profile (reboot or `glk_policy_reload_blacklist`)

Under **medsec-strict**, all TX / GPIO-out / contact / system actuators are denied.

## Skills

| ID | Risk | Purpose |
|----|------|---------|
| medsec_passive_ism_watch | passive_rx | Lab ISM activity watch |
| fac_rf_baseline_watch | passive_rx | Facility engineering RF baseline |
| med_asset_uid_inventory | passive_rx | Owned NFC/UID inventory study |

## Missions (ROM + SD)

| ID | Purpose |
|----|---------|
| medsec_lab_passive_ism | MedSec lab ISM passive sample |
| fac_rf_snapshot_passive | Facility multi-band passive snapshot |
| medsec_passive_watch | Short loop passive watch |

See also: `docs/MEDSEC_WORLDWIDE_NEXT_STEPS.md`, `docs/MEDSEC_PILOT_IN_A_BOX.md`.
