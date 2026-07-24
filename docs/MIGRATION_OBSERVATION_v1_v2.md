# Migration: signal observation v1 → v2

**Non-breaking.** All v1 fields remain present. Schema id changes; tools keep working.

## Identity

| | v1 | v2 |
|--|----|----|
| `schema` | `groklink.signal_observation.v1` | `groklink.signal_observation.v2` |
| `schema_version` | `1` | `2` |
| `schema_compat` | n/a | `["…v1","…v2"]` |

## New optional blocks

- `stats` — `pulse_rate_hz`, `rssi_min_dbm`, `rssi_max_dbm`
- `calibration` — host/device baseline + `snr_est_db`
- `activity.calibrated_occupancy`, `activity.confidence`
- New `kind` values: `noise_floor`, `band_compare`, `calibration_state`
- Event types expanded; `payload_hex` remains **null**

## Device RPC

`subghz_rx` / spectrum bands may include:

```json
"pulse_rate_hz": 1234, "rssi_min": -120, "rssi_max": -110
```

Older hosts ignore unknown keys. Older firmware omits them; host computes `pulse_rate_hz` from pulses/ms.

## Tool surface

v3.4: 16 tools (unchanged).  
v3.5 adds: `get_event_taxonomy`, `get_calibration_state`, `observe_noise_floor`, `observe_compare`.

## Agent code changes

```python
# Before
occ = obs["activity"]["occupancy"]

# After (preferred)
occ = obs["activity"].get("calibrated_occupancy") or obs["activity"]["occupancy"]
```

No change required for callers that only read `narrative` + `occupancy`.
