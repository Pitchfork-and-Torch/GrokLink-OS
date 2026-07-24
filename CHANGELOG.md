# Changelog

GrokLink OS - from-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, ROM mission catalog, on-device GUI, PC bridge, and multi-LLM signal observability (authorized educational use only).

## 3.7.1 — MedSec instrument pack (2026-07-22)

### MedSec / healthcare

- ROM catalog: `medsec_lab_passive_ism`, `fac_rf_snapshot_passive`, `medsec_passive_watch` + skills
- Policy profile **`medsec-strict`**: all TX / GPIO-out / contact / system denied
- SD packs under `sd_card/groklink/healthcare/` + deploy script `scripts/deploy_medsec_packs.ps1`
- RPC `status`: `not_medical_device`, `profile`, `medsec_strict`, disclaimer
- GUI ABOUT/SAFETY: not-medical messaging
- Bridge `groklink-os lab *`: engagement, casefile, anomaly, export, SIEM NDJSON, vault-seal, phi-check, medsec-demo
- Docs: operator runbook, pilot-in-a-box, RoE template, facility RF playbook, risk file, security self-review
- LLM allowlist includes MedSec passive missions only (never TX)

### Notes

- **Not a medical device.** Authorized research / MedSec lab only.
- Independent third-party security review still recommended before hospital pilots.
