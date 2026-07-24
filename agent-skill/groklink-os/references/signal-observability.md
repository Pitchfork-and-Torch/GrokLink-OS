# Signal observability (multi-LLM) — v3.5 Signal Cognition

GrokLink OS 3.5+ exposes the live SubGHz environment as **structured observations**
for OpenAI-style tool calling and any compatible LLM.

Schema: `groklink.signal_observation.v2` (v1-compatible fields retained).

## Tools (prefer these over raw CLI for RF “vision”)

| Tool | Purpose |
|------|---------|
| `get_observation_schema` | Schema + tool map |
| `get_event_taxonomy` | Non-decode event types |
| `get_device_status` | Device/policy + probe |
| `get_calibration_state` | Host noise-floor baselines |
| `observe_noise_floor` | Baseline sample for a band |
| `observe_rx` | One passive RX snapshot |
| `observe_spectrum` | Multi-band sequential scan |
| `observe_compare` | Two-band passive compare |
| `start_monitor` / `get_monitor_chunk` / `stop_monitor` | Continuous passive sampling |
| `get_recent_activity` | Rolling local history |

```powershell
groklink-os tools-schema
groklink-os event-taxonomy
groklink-os observe-noise-floor --freq 433920000 --ms 200
groklink-os tool-call observe_rx --args "{\"freq_hz\":433920000,\"ms\":400}"
groklink-os observe-serve --port 8741
```

## How to present the signal world (v3.5)

1. Call `get_device_status` once; confirm `edu` and radio health.
2. Call `observe_noise_floor` on an **owned-lab** frequency (quiet moment preferred).
3. Call `observe_rx` / `observe_spectrum` / `observe_compare` on authorized bands only.
4. Prefer: `narrative`, `activity.calibrated_occupancy`, `stats.pulse_rate_hz`,
   `calibration.snr_est_db`, `activity.confidence`, `spectrum.hottest`.
5. Fall back to `activity.occupancy` if calibration is empty.
6. Never claim protocol decode from light RX. `payload_hex` is always null.
7. Never TX from observation tools. Confirm tokens are human-only.

## Local data

Observations: `~/.grok/state/groklink-os/observations/recent.jsonl`  
Calibration: `~/.grok/state/groklink-os/calibration.json`  
Audit: `~/.grok/state/groklink-os/audit/observe_audit.jsonl`

Do not publish these files.

Full design: repo `docs/SIGNAL_OBSERVABILITY.md`, `docs/design/v3.5-signal-cognition.md`,
`docs/lab/SIGNAL_COGNITION_PLAYBOOK.md`.
