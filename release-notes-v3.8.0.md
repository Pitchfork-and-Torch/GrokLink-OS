# GrokLink OS v3.8.0 - Storage, Skills & Persistent Autonomy

**Date:** 2026-07-29  
**Codename:** Field Research (storage-backed)

## Summary

v3.8.0 closes the roadmap gap after the USB-stable field unit (3.7.x): durable storage, hot-load skills/missions, persistent vault, hardened spectrum duty planner, and clearer operator status - without weakening default-deny safety or MedSec passive posture.

## What changed

| Area | Delivered |
|------|-----------|
| Storage | Crash-safe host FS API, layout, degraded modes, integrity markers |
| Skills | SD scan/merge, refcount, unload, optional `GLKSIG1` verify hook |
| Vault | Hash-chained durable events + optional seal; reboot-survivable on sim |
| Spectrum | Min settle enforced, duty budget, arbiter, breaker fail-closed |
| Agent | Resume tokens, power-aware deferral |
| GUI | STORAGE page + SAFETY arm feedback |
| Bridge/RPC | API 6; status + skill/storage/vault/planner/power commands |
| Tests | `storage_v38`, `spectrum_duty`; all host ctest + bridge pytest green |

## Build / test / flash

### Host simulation

```powershell
cd GrokLink-OS
cmake -B build-host -DGLK_PLATFORM_HOST=ON -DGLK_BUILD_TESTS=ON
cmake --build build-host --config Release
cd build-host; ctest -C Release --output-on-failure

# Run host OS (RPC :7341)
$env:GLK_SD_ROOT = "$PWD\..\sd_card\groklink"
$env:GLK_RUN_MS = "3000"
.\Release\groklink_os.exe
```

### Bridge

```powershell
cd bridge
py -3 -m pip install -e .
py -3 -m pytest tests -q
groklink-os storage-status
groklink-os skill-scan
groklink-os vault-status
groklink-os spectrum-status
```

### Device DFU (unchanged packaging path)

```powershell
# OsRadio / CDC profiles as before
powershell -ExecutionPolicy Bypass -File tools\build_dfu.ps1
# Flash via qFlipper / dfu-util per docs/DFU.md and docs/FLASH_LATEST.md
```

## Safety (unchanged non-negotiables)

- Education phrase: `I_WILL_USE_ONLY_AUTHORIZED_TARGETS`
- Default-deny TX / GPIO / contact / system
- No third-party remote decode or rolling-code prediction
- Observation tools remain passive-only
- **Not a medical device** - MedSec = authorized research / facility instrument only; `medsec-strict` forbids TX

## Known limitations

- Device littlefs/SDMMC mount is API-ready; host sim is the full fidelity path today. Bare-metal without FS stays ROM-passive.
- Skill signatures are lightweight `GLKSIG1` + FNV integrity (hook for Ed25519 later); not a secure-boot substitute.
- Vault seal is operator-token protection, not full-disk encryption; PHI tooling stays host-side.
- Spectrum duty budgets are software-enforced in the radio worker (must not be bypassed by new RPC).

## Suggested next (3.9)

- BLE status channel (M0+ IPCC skeleton)
- Real littlefs on SDMMC for field durability
- Ed25519 skill/package verify when crypto budget allows
- Minimal GrokLink Desktop (DFU + serial + observe)
- CI host-test only (device DFU remains human-gated)
- Soak scripts for USB open/close + plug-sync

## Operator one-pager

1. Flash latest DFU or run host sim with `sd_card/groklink`.
2. `groklink-os edu-ack` then `status` - expect `version: 3.8.0`, `api: 6`.
3. `storage-status` / `skill-scan` / `vault-status` for new surfaces.
4. Unplugged: SAFETY hold OK or `prepare-unplugged` - passive only; plug-sync on reconnect.
5. Never clinical use. Authorized targets only.
