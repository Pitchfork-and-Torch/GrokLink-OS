# Signal World Extension — v3.5 Signal Cognition

**Extends:** [SIGNAL_WORLD_EXPLORATION.md](SIGNAL_WORLD_EXPLORATION.md) (v3.4.0 baseline)  
**Firmware target:** 3.5.0 · **Schema:** `groklink.signal_observation.v2`  
**Date:** 2026-07-22  

## What changed for agents

| Capability | v3.4 | v3.5 |
|------------|------|------|
| Schema | v1 | **v2** (v1-compatible fields + stats/calibration) |
| Tools | 16 | **20** (+ taxonomy, calibration, noise floor, compare) |
| Occupancy | absolute heuristic only | + **calibrated_occupancy** vs noise baseline |
| Events | edge_activity | + quiet_dwell, elevated_vs_noise, band_hotspot, noise_floor_sample |
| Device RX JSON | pulses, rssi | + pulse_rate_hz, rssi_min, rssi_max |
| ROM missions | 3 | **4** (+ `lab_noise_baseline`) |

## Safety (unchanged invariants)

- `policy_context.passive_only = true`
- `safety.tx = false` on every observation
- `safety.decode = false` — no third-party payload decode
- Edu gate required; actuators default-deny + human confirm

## Agent playbook

See [SIGNAL_COGNITION_PLAYBOOK.md](SIGNAL_COGNITION_PLAYBOOK.md).

## Design

Full plan/design: [docs/design/v3.5-signal-cognition.md](../design/v3.5-signal-cognition.md).
