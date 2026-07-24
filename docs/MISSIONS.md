# Missions — GrokLink OS 3.3

Missions are offline or online programs executed by **GrokAgent**.

## ROM catalog (no SD required, v3.3+)

Loaded at boot via `glk_catalog_load_defaults()` on device and host:

| Id | Purpose |
|----|---------|
| `lab_passive_433` | One passive 433.92 RX + infer |
| `lab_spectrum_planner` | 315 + 433 sequential passive RX |
| `lab_passive_watch` | Loop×2 RX with pulse threshold log |
| `lab_noise_baseline` | Short dwells for host calibration |
| `medsec_lab_passive_ism` | MedSec lab ISM passive (433 + 315); **not a medical device** |
| `fac_rf_snapshot_passive` | Facility multi-band passive snapshot |
| `medsec_passive_watch` | Short MedSec passive watch loop |

```powershell
groklink-os mission-run --id lab_passive_433 --steps 8
# RPC:
# {"cmd":"mission_arm","id":"lab_passive_433"}
# {"cmd":"mission_run","steps":8}
```

LLM tool: `run_passive_mission` (allowlisted ids only — never TX).  
Allowlist includes lab + MedSec passive ids above.

## Offline autonomous loop (v3.4+)

```powershell
groklink-os agent-offline --id lab_passive_watch
# device ticks passively while USB idle; pull results:
groklink-os vault-tail -n 12
groklink-os agent-status
groklink-os agent-offline --stop
```

RPC: `agent_auto` + `agent_offline` + `vault_tail`. Events live in a **RAM vault** (no SD).

## JSON format (v2-compatible + extensions)

```json
{
  "id": "lab_passive_433",
  "autonomous": false,
  "steps": [
    {"op": "log", "msg": "start"},
    {"op": "subghz_rx", "freq_hz": 433920000, "ms": 400},
    {"op": "infer"},
    {"op": "if_pulses_gt", "threshold": 20},
    {"op": "log", "msg": "busy"},
    {"op": "loop", "count": 3},
    {"op": "subghz_rx", "freq_hz": 433920000, "ms": 300},
    {"op": "end_loop"}
  ]
}
```

## Opcodes

| op | Meaning | Risk |
|----|---------|------|
| `sleep` / `sleep_ms` | Delay | info |
| `subghz_rx` | Passive RX job | passive_rx |
| `subghz_tx` | TX (needs confirm; agent usually denied) | active_tx |
| `log` | Audit/info log | info |
| `infer` | TinyML / heuristic | info (no actuate) |
| `if_pulses_gt` | Conditional skip | info |
| `loop` / `end_loop` | Counted loop | info |
| `abort` | Fail mission | info |
| `parallel_begin/end` | Reserved | — |
| `reserve` / `release` | Resource arbiter (reserved) | — |

## Safety

Every hardware step calls `glk_policy_check`. Autonomous missions still cannot
bypass blacklists, duty limits, or confirm rules.

## Resume

Future: `state/mission_<id>.resume` on SD for crash recovery. Hooks in storage
service layout.
