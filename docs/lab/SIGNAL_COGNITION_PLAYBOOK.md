# How an agent should use Signal Cognition (v3.5)

**Authorized lab only.** Phrase: `I_WILL_USE_ONLY_AUTHORIZED_TARGETS`  
**Never TX. Never invent confirm tokens. Never claim protocol decode from light RX.**

## Recommended playbook

```text
1. get_observation_schema          # learn v2 fields + event taxonomy
2. get_event_taxonomy              # non-decode event types only
3. get_device_status               # version, edu, heap, sim/hw honesty
4. observe_noise_floor  (owned-lab freq)   # establish host baseline
5. get_calibration_state           # confirm baseline stored
6. observe_rx           (same freq)        # relative calibrated_occupancy
7. observe_spectrum     (optional multi-band)
8. observe_compare      (optional A/B bands)
9. summarize narrative + calibrated_occupancy + snr_est for the operator
10. never TX
```

## Reading observations

| Field | Meaning |
|-------|---------|
| `narrative` | One-line situation for chat models |
| `activity.occupancy` | Absolute heuristic (v1) |
| `activity.calibrated_occupancy` | **quiet / ambient / elevated / busy** vs baseline |
| `calibration.snr_est_db` | rssi − noise_floor when known |
| `stats.pulse_rate_hz` | Edges per second |
| `events[]` | Non-decode taxonomy only (`payload_hex` always null) |
| `safety.tx` | Always `false` on this path |
| `device.simulated` / raw `sim` | Honesty gate for silicon vs host-sim |

## CLI (host)

```powershell
groklink-os edu-ack
groklink-os event-taxonomy
groklink-os observe-noise-floor --freq 433920000 --ms 200
groklink-os calibration-state
groklink-os observe-rx --freq 433920000 --ms 400
groklink-os observe-compare --freq-a 433920000 --freq-b 315000000
```

## DFU / flash notes

1. Enter STM32 DFU (`BACK+OK` ~30s) → `VID_0483:PID_DF11`.
2. Flash `dist/dfu/GrokLink-OS-v3.5.0-radio.dfu` via `tools/flash_os_dfu_only.ps1` (after rebuild).
3. qFlipper “protobuf / exit recovery” errors after flash are **expected** (not Flipper firmware).
4. Prefer short serial sessions; drain between long RX bursts.

## What stayed out of scope

- Protocol / payload decode  
- TX automation  
- Full SD FatFs  
- Removing edu gate or confirm tokens  
