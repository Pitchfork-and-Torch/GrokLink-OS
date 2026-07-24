# GrokLink OS — v3.7.0 shipped and v3.7.x / v3.8 plan

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

## Near-term polish (3.7.x)

1. **USB soak tests** — multi-hour CDC open/close, plug-sync loops, qFlipper DFU cycle script.
2. **`reboot_dfu` reliability** — document button DFU as primary; soft reboot as best-effort.
3. **Bridge defaults** — auto-detect product string / COM after flash; friendlier Windows errors.
4. **GUI field UX** — clearer SAFETY hold feedback; show “USB host active / field deferred”.
5. **Docs** — operator one-pager: flash, ping, unplugged arm, plug-sync only.

## Next major: v3.8 (storage + skills on device)

| Work | Notes |
|------|--------|
| SD FatFs or littlefs | Hot-load missions/skills from card |
| Signed skill packages | Optional verify hook already in design |
| Persistent vault | Survive reboot (today: RAM vault) |
| Spectrum planner duty limits | Circuit breaker already partial |

## Later

| Work | Notes |
|------|--------|
| BLE status channel | M0+ stack / dual-core |
| Minimal “GrokLink Desktop” | DFU + serial + observe (not Flipper protobuf) |
| CI host tests only | Device DFU remains human-gated |

## Safety non-negotiables (all versions)

- Authorized targets only  
- Default-deny TX / GPIO / contact / system  
- No third-party remote decode or rolling-code prediction  
- Education phrase required before elevated ops  
- Not a medical device; MedSec path is research/facility instrument only — see [MEDSEC_WORLDWIDE_NEXT_STEPS.md](MEDSEC_WORLDWIDE_NEXT_STEPS.md)  
