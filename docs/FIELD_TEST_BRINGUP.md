# Field test — GrokLink OS v3.0.0-bringup DFU

**Date:** 2026-07-22  
**Image:** `GrokLink-OS-v3.0.0-bringup.dfu` (772-byte app @ `0x08000000`)  
**Host:** Windows, qFlipper present, bridge tooling available  

## Goal

Connect over USB and exercise GrokRPC after native bring-up flash.

## Procedure

1. Operator flashed bring-up DFU via qFlipper.  
2. Agent enumerated PnP USB / COM ports.  
3. Attempted serial open on any STM32/Flipper ports.  

## Observations

| Check | Result |
|-------|--------|
| Flipper VID `316D` present | **No** |
| STM32 DFU bootloader `0483:DF11` | **No** (not held in DFU) |
| STM32 VCP `0483:5740` live COM | **No** (ghost COM5/COM6 “Unknown”, open fails) |
| Stock Flipper composite USB | **Absent** |
| Serial open COM5/COM6 | `The port does not exist` |
| GrokRPC / CLI | **N/A** — no transport |

### Interpretation

This is **expected** for the bring-up image:

- Stock Flipper USB stack was **replaced**.  
- Bring-up firmware **does not implement USB**, so Windows correctly sees **no** device class for the running app.  
- MCU is almost certainly **running** (GPIO toggle loop) but is **invisible** to PC software until USB CDC exists.  
- Recovery path remains: hold **BACK** + USB → DFU → flash full image (`GrokLink-v2.1.3.dfu` or stock).

## Pass / fail (bring-up only)

| Criterion | Status |
|-----------|--------|
| DFU accepted by qFlipper | Pass |
| Host can open serial / RPC | **Fail (by design of bring-up)** |

## Follow-up: P1 CDC field test (2026-07-22)

| Step | Result |
|------|--------|
| Reboot to DFU via RPC (`tools/flash_cdc_via_dfu.py`) | Pass |
| Flash `GrokLink-OS-v3.0.1-cdc.dfu` 100% | Pass |
| Enumerate `0483:5740` COM7 | Pass |
| `{"cmd":"ping"}` → pong `3.0.1-cdc` | **Pass** |
| `status` / `edu_ack` | **Pass** |

**Native USB path is live.** Lab restore anytime: `.\tools\recover_flipper.ps1`.

## Immediate recommendations

1. **For lab work today:** reflash `GrokLink-v2.1.3.dfu` (full overlay) so `groklink` CLI works.  
2. **For native OS:** implement Phase 1 (`docs/ROADMAP_NATIVE.md`) — USB CDC + banner + ping — rebuild DFU, reflash, re-run `tools/device_probe.ps1`.  

## Restore lab firmware (when ready)

```text
1. Hold BACK, plug USB
2. qFlipper → Install from file → GrokLink-v2.1.3.dfu
3. Copy /ext/groklink SD tree
4. CLI: groklink status
```
