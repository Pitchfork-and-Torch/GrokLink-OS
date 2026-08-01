# Storage - GrokLink OS v3.8

Crash-safe, host-first storage for skills, missions, vault, logs, and resume state.

## Layout (`GLK_SD_ROOT` / `/groklink`)

| Path | Purpose |
|------|---------|
| `config/` | agent.json, profile.json (incl. medsec-strict) |
| `missions/` | Hot-load mission JSON |
| `skills/<id>/` | manifest.json, rules.json, optional protocol.json, module.bin, manifest.sig |
| `logs/` | audit.jsonl (+ future segments) |
| `blacklist/` | freq / gpio / protocols |
| `vault/` | events.jsonl, chain.meta, .integrity |
| `state/` | mission_*.resume tokens |
| `healthcare/` | MedSec pack templates (research only) |

Host sim default: `sd_card/groklink` or `GLK_SD_ROOT`.

## Modes

| Mode | Meaning |
|------|---------|
| `absent` | No card / path - **ROM passive only** |
| `ok` | Layout healthy, writable |
| `degraded` | Partial layout or soft write errors |
| `corrupt` | Integrity marker failed - elevated actions fail closed |
| `full` | Write rejected |
| `readonly` | Present but not writable |

## Crash-safe helpers

- **Atomic write:** `path.tmp` -> rename over target
- **Append:** vault / audit lines (best-effort; chain meta rewritten atomically)
- **Integrity:** FNV-1a 32 markers (`GLK1 <hex>`) - integrity helper, **not** a crypto claim
- **Path escape:** `..` denied

## Device note

STM32 image without FS reports `absent` / degraded and keeps **ROM catalog** passive missions. The same C API is used on host; littlefs/SDMMC can bind behind `glk_storage_*` later without changing services.

## Skill packages

```
skills/<id>/
  manifest.json    # id, version, risk_class
  rules.json       # optional
  manifest.sig     # optional: GLKSIG1 <fnv1a32-hex of manifest bytes>
```

`GLK_FEATURE_SIGNED_SKILLS=0` (default): unsigned loads; audit soft note.  
`=1`: reject missing/mismatched signatures.

## Vault

- RAM ring (`GLK_VAULT_CAP`) + durable append when storage usable
- Hash chain root in `vault/chain.meta`
- Optional operator seal (`vault_seal` / `vault_unseal`) - light token, not full disk crypto
- Host PHI seal remains in bridge `lab vault-seal`

## RPC

- `storage_status`, `skill_scan`, `skill_status`, `skill_unload`
- `vault_status`, `vault_flush`, `vault_seal`, `vault_unseal`
- `resume_save` / `resume_load`
- `spectrum_status` / `planner_status`, `power_status`

## Degraded matrix

| Condition | Behavior |
|-----------|----------|
| No SD | ROM passive missions/skills; no durable vault; TX fail-closed if blacklist missing |
| Corrupt integrity | `corrupt` mode; skill elevated load denied when enforce on |
| Full volume | Writes return `GLK_ERR_FULL`; agent keeps RAM vault only |
| Power loss mid-write | Temp rename leaves prior file intact |
