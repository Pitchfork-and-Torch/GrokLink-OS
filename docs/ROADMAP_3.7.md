# GrokLink OS - v3.7.x shipped  |  v3.8.0 shipped  |  v3.9 sketch

## Shipped: v3.7.0 (USB-stable field unit)

Milestone after isolation bisect (CDC-only / bisect-link / bisect-init):

| Item | Status |
|------|--------|
| USB-first boot (CDC-parity order) | Done |
| Light RPC in bulk RX callback | Done |
| Service init with continuous `usbd_poll` | Done |
| Host DTR gates field SPI (no bind kill) | Done |
| Unplugged auto field + GUI SAFETY arm | Done |
| Live verify: ping / status / edu_ack | Done on hardware |
| PID `0483:5740` + product **GrokLink OS** | Done (max Windows usbser compatibility) |

## Shipped: v3.7.1 (MedSec instrument pack)

| Item | Status |
|------|--------|
| ROM MedSec / facility passive catalog | Done |
| `medsec-strict` profile | Done |
| Bridge lab evidence CLI | Done |
| Healthcare SD packs + docs | Done |

## Near-term polish (3.7.x residual)

1. **USB soak tests** - multi-hour CDC open/close, plug-sync loops, qFlipper DFU cycle script.
2. **`reboot_dfu` reliability** - document button DFU as primary; soft reboot as best-effort.
3. **Bridge defaults** - auto-detect product string / COM after flash; friendlier Windows errors.

## Shipped: v3.8.0 (Storage, Skills & Persistent Autonomy)

| Work | Status |
|------|--------|
| SD / host storage stack (crash-safe helpers, layout, degraded modes) | **Done** |
| Hot-load skills/missions + optional signed packages | **Done** |
| Persistent vault (hash chain + seal) | **Done** |
| Spectrum planner duty limits + breaker + arbiter | **Done** |
| Agent resume + power-aware deferral | **Done** |
| GUI STORAGE page + RPC/bridge surface (API 6) | **Done** |
| Host tests (`storage_v38`, `spectrum_duty`) | **Done** |

Detail: [STORAGE.md](STORAGE.md)  |  [release-notes-v3.8.0.md](../release-notes-v3.8.0.md)

## Next major: v3.9 (connectivity & field durability)

| Work | Notes |
|------|--------|
| littlefs on SDMMC (device) | Bind behind existing `glk_storage_*` |
| BLE status channel | M0+ stack / IPCC skeleton |
| Ed25519 skill verify | When flash/crypto budget allows |
| Minimal GrokLink Desktop | DFU + serial + observe (not Flipper protobuf) |
| CI host tests only | Device DFU remains human-gated |
| USB soak automation | Extend 3.7.x residual scripts |

## Safety non-negotiables (all versions)

- Authorized targets only  
- Default-deny TX / GPIO / contact / system  
- No third-party remote decode or rolling-code prediction  
- Education phrase required before elevated ops  
- Not a medical device; MedSec path is research/facility instrument only - see [MEDSEC_WORLDWIDE_NEXT_STEPS.md](MEDSEC_WORLDWIDE_NEXT_STEPS.md)  
