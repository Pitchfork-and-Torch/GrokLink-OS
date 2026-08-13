# Changelog

GrokLink OS - from-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, ROM mission catalog, on-device GUI, PC bridge, and multi-LLM signal observability (authorized educational use only).

## 3.8.0 - Storage, Skills & Persistent Autonomy (2026-07-29)

### Storage

- Full host-first storage service: layout ensure, atomic write, append, exists/remove, FNV integrity markers
- Modes: absent / ok / degraded / corrupt / full / readonly with `storage_status` RPC
- Logical layout under `/groklink` (config, missions, skills, logs, blacklist, vault, state, healthcare)
- Path escape (`..`) denied; fail-closed when media unusable

### Skills

- Hot-load scan (merge with ROM catalog), refcount retain/release, SD hot-unload
- Optional package signature hook (`manifest.sig` / `GLKSIG1`); soft-allow when `GLK_FEATURE_SIGNED_SKILLS=0`
- Risk class mapped; `skill_scan` / `skill_status` / `skill_unload` RPC + bridge CLI

### Persistent vault

- Durable hash-chained append (`vault/events.jsonl` + `chain.meta`) when SD present
- Survives reboot on host-sim; plug-sync compatible; optional operator seal
- RAM ring retained for no-SD / sealed paths

### Spectrum planner hardening

- **Minimum settle** enforced (`GLK_SPECTRUM_SETTLE_MS`) - fixed v3.7 cap that shrunk settle to 200 ms
- Global duty budget window; circuit breaker on radio faults cannot be bypassed
- SubGHz resource arbiter acquire/release; `spectrum_status` / planner telemetry

### Agent / policy / power / GUI

- Mission resume tokens on storage (`state/mission_<id>.resume`)
- Power-aware autonomy deferral (deep_sleep / low battery)
- Policy gates for vault clear/seal and storage-degraded skill load
- ST7567 **STORAGE** page: SD mode, skills count, vault, USB host vs field line
- SAFETY arm feedback clearer when FIELD active

### Bridge

- CLI: `storage-status`, `skill-status`, `skill-scan`, `skill-unload`, `vault-status`, `vault-flush`, `spectrum-status`, `power-status`
- RPC API generation **6**; bridge package **3.8.0**

### Tests & docs

- Host: `test_storage_v38`, `test_spectrum_duty`; policy storage-op checks
- Docs: `docs/STORAGE.md`, release notes, ROADMAP 3.8 done / 3.9 sketch
- **Not a medical device.** MedSec path remains authorized passive research only.

## 3.7.1 - MedSec instrument pack (2026-07-22)

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
