# Signal World Exploration Report

**Product:** GrokLink OS · **Firmware:** v3.4.0 · **API:** 5  
**Scope:** Authorized lab exploration of multi-LLM signal observability (passive only)  
**Date:** 2026-07-22  
**Authors:** Pitchfork-and-Torch  

> **Legal:** Authorized research / owned equipment only. Observation paths never transmit.  
> Phrase: `I_WILL_USE_ONLY_AUTHORIZED_TARGETS`

This report records what a tool-calling agent can “see” when GrokLink OS is treated as a
**passive RF sensory peripheral**, and what the v3.4 stack returns in a real DFU-flashed lab session.

Companion machine-readable summary: [`signal-world-exploration-summary.json`](signal-world-exploration-summary.json).

---

## 1. What was explored

| Layer | Explored | Result |
|-------|----------|--------|
| USB DFU flash | `GrokLink-OS-v3.4.0-radio.dfu` | Full program success (32,996 payload bytes) |
| USB CDC GrokRPC | JSON-line @ 230400 baud | Live after reboot; STM32 CDC `0483:5740` |
| Policy / edu session | `edu_ack` | Session opens; elevated passive RPC allowed |
| Device status | `status` | version `3.4.0`, missions `3`, skills `3`, heap free `4096` |
| ROM catalog | `mission_list` | Three allowlisted passive missions (no SD) |
| Passive SubGHz RX | `subghz_rx` @ 433.92 MHz | Hardware path `sim:false`; edge counts + RSSI |
| Observation packager | host-side `groklink.signal_observation.v1` | Narratives, occupancy, energy scores |
| Multi-LLM tools surface | OpenAI-format tools | **16** tools (observe, monitor, missions, agent, vault) |
| Offline agent | `agent_offline` enable/disable | Accepted; USB still responsive in short ticks |
| Safety guarantees | tool + packager safety fields | `tx: false` on observation path |

**Not explored (by design):** TX, GPIO drive, confirm-token minting for actuators, payload decode of third-party traffic, any non-owned targets.

---

## 2. Multi-LLM tool surface

The bridge exports OpenAI-compatible function tools. Agents should call
`get_observation_schema` once per session, then use passive tools only.

| Tool | Role |
|------|------|
| `get_observation_schema` | Schema + tool map |
| `get_device_status` | Policy/status (+ optional radio probe) |
| `observe_rx` | Single passive RX snapshot → structured observation |
| `observe_spectrum` | Multi-band sequential scan → hottest band |
| `start_monitor` / `stop_monitor` / `get_monitor_chunk` | Host-side rolling awareness |
| `get_recent_activity` | Local rolling history summary |
| `list_missions` / `list_skills` | Catalog inspection |
| `run_passive_mission` / `get_mission_status` | Allowlisted ROM missions (passive) |
| `start_offline_agent` / `stop_offline_agent` / `get_agent_status` | USB-safe autonomous passive loop |
| `get_vault_tail` | RAM vault of agent events |

Schema identity:

```text
schema = groklink.signal_observation.v1
schema_version = 1
```

Reasoning order for models:

1. Read `result.narrative`  
2. Read `activity.occupancy`, `rssi_dbm`, `pulses`, `energy_score`  
3. For multi-band work, use `spectrum.hottest`  
4. Never invent TX; confirm tokens remain human-gated  

---

## 3. Live lab device profile (post-flash)

| Field | Value |
|-------|--------|
| Firmware | `3.4.0` |
| API | `5` |
| Education session | `edu: true` after ack |
| SD card | not required for ROM missions (`sd: false`) |
| Blacklist | `blacklist_ok: true` |
| Missions | `3` |
| Skills | `3` |
| Heap free (reported) | `4096` |
| Radio path | Hardware CC1101 light RX (`sim: false`) |
| ROM mission IDs | `lab_passive_433`, `lab_spectrum_planner`, `lab_passive_watch` |

Transport notes for integrators:

- Prefer USB CDC when a device with VID `0483` / PID `5740` is present.  
- Host-sim remains available on TCP `127.0.0.1:7341` for CI / offline demos.  
- On some Windows hosts, CDC configuration via standard serial libraries fails; a Win32 handle fallback in the bridge keeps RPC usable.

---

## 4. Passive RF findings (433.92 MHz lab band)

All samples are **edge-count / RSSI only** (v3.x light RX). No protocol decode, no payload capture, no identification of transmitters. Pulse density on an open lab band is treated as **environment energy**, not as content.

### 4.1 Packaged observations from live RPC samples

Host packager (`ObservationPackager.package_rx`) turned raw RPC into self-describing observations:

| Sample | Dwell | Pulses | RSSI (dBm) | Occupancy | Energy score | Simulated |
|--------|-------|--------|------------|-----------|--------------|-----------|
| A (post first flash) | 300 ms | 6851 | −114 | **high** | 0.65 | false |
| B (post reflash) | 300 ms | 6778 | −112 | **high** | 0.65 | false |
| Quiet model check | 400 ms | 0 | −120 | **quiet** | 0.00 | false |

Narratives (model-facing one-liners):

```text
Passive RX at 433.920 MHz for 300 ms: pulses=6851, rssi=-114 dBm, occupancy=high, simulated=False.
Passive RX at 433.920 MHz for 300 ms: pulses=6778, rssi=-112 dBm, occupancy=high, simulated=False.
Passive RX at 433.920 MHz for 400 ms: pulses=0, rssi=-120 dBm, occupancy=quiet, simulated=False.
```

**Interpretation for agents**

- `occupancy=high` means **dense edge activity in the dwell window**, not a decoded frame.  
- RSSI around −112…−114 dBm with thousands of edges is consistent with a noisy or busy ISM environment (or on-board noise floor / spur contribution).  
- Agents should **not** claim “detected protocol X” from light RX alone.  
- `simulated=false` is the honesty gate: silicon path was live, not host-sim.

### 4.2 Spectrum-style multi-band model

A three-band vector was packaged to exercise `package_spectrum` (315 / 433.92 / 868.35 MHz class):

```text
Spectrum scan of 3 band(s), dwell=250 ms, settle=2000 ms.
Hottest: 433.92 MHz (pulses=6778, occupancy=high).
```

| Band (MHz) | Pulses (model) | RSSI | Occupancy label |
|------------|----------------|------|-----------------|
| 315.0 | 120 | −120 | high* |
| 433.92 | 6778 | −112 | high |
| 868.35 | 40 | −122 | medium |

\*Occupancy labels are host heuristics on pulse density; treat them as **relative ranking aids**, not calibrated spectrum analysis.

**Hottest band selection** correctly preferred 433.92 MHz by pulse count — the same ranking a tool-calling model would surface via `spectrum.hottest`.

---

## 5. Safety model observed in the tool path

Every packaged observation carried:

| Guarantee | Observed |
|-----------|----------|
| `policy_context.passive_only` | `true` |
| `safety.tx` | `false` |
| Edu required for RX | enforced by device policy / tools auto-ack path |
| Default-deny actuators | No observation tool exposes TX |

Agent-facing rule that held under exploration: **LLM tools never open a TX path**.

---

## 6. Offline agent + vault

Short-session checks:

| Step | Result |
|------|--------|
| `agent_offline` enable | `offline: true` |
| `ping` while offline | Still answered (USB pump / deferred radio path) |
| `vault_tail` | Empty when no agent events yet |
| `agent_offline` disable | Returns to online/manual control |

Implication: constrained autonomy is **passive-only** and can coexist with CDC if the USB pump dwell path is active. Long concurrent RX bursts can still stress host serial stacks; integrators should open short sessions, drain buffers, and avoid overlapping exclusive COM handles.

---

## 7. Agent skill loop (portable)

The portable skill under `agent-skill/groklink-os/` implements the adaptive loop:

1. `refresh_all.py` — sync VERSION / RPC / docs into local learning store  
2. `probe_capabilities.py` — passive capability map  
3. `session_check.py` — ping · edu · status · optional RX  
4. `observe_session.py` — multi-freq observe + optional learn  

Learning store remains **local** (`~/.grok/state/groklink-os/`) and is **not** published. Captures and raw dumps stay off the public tree.

---

## 8. Findings summary

### Strengths

1. **End-to-end ownership** — native RTOS path from DFU → CDC → policy → radio → host observation packager.  
2. **Honest silicon** — `sim:false` on live CC1101 light RX after probe path.  
3. **LLM-ready packaging** — narratives + occupancy + schema identity reduce tool-call friction.  
4. **ROM missions without SD** — three passive missions available immediately after flash.  
5. **Safety by construction** — observation tools stay passive; TX remains human-gated.  
6. **Portable skill** — same workflows for humans and agents via `agent-skill/groklink-os/`.

### Limitations / lab notes

1. **Light RX only** — edges + RSSI; no demod/decode in this firmware class.  
2. **Occupancy is a host heuristic** — useful for ranking, not a substitute for calibrated instruments.  
3. **Windows CDC quirks** — exclusive COM handles and serial configure failures require careful transport code (bridge includes a Win32 fallback).  
4. **Long multi-command serial bursts** can desync line buffering; prefer one logical operation per short session when debugging.  
5. **qFlipper post-flash “protobuf / exit recovery” errors are expected** when running GrokLink (not official Flipper firmware).

### Recommended agent playbook

```text
1. get_observation_schema
2. get_device_status
3. observe_rx  (owned-lab frequency only)
4. observe_spectrum  (optional multi-band)
5. summarize narrative + hottest band for the operator
6. never TX; never invent confirm tokens
```

---

## 9. Reproducibility (public)

```powershell
git clone https://github.com/Pitchfork-and-Torch/GrokLink-OS.git
cd GrokLink-OS/bridge
py -3 -m pip install -e ".[serial]"

# Device in DFU: flash dist/dfu/GrokLink-OS-v3.4.0-radio.dfu via tools/flash_os_dfu_only.ps1
# Then set serial port env for your host OS and:

groklink-os edu-ack
groklink-os status
groklink-os observe-rx --freq 433920000 --ms 400
groklink-os tools-schema
```

Skill install:

```powershell
Copy-Item -Recurse agent-skill\groklink-os $HOME\.grok\skills\groklink-os
```

---

## 10. Conclusion

On v3.4.0 hardware, a tool-calling agent can already:

- Confirm device identity and policy state  
- Open an education session  
- Take passive RX snapshots and receive **self-describing observations**  
- Rank multi-band activity via packaged spectrum results  
- List and reason about ROM passive missions  
- Start/stop a USB-safe offline passive agent and inspect a RAM vault  

The “signal world” is therefore real enough for **lab situational awareness**: occupancy narratives, energy scores, and honest `sim` flags — without crossing into decode, TX, or unauthorized targets.

Further work belongs in calibrated spectrum tooling, richer event taxonomy, and host monitor UX — not in weakening the default-deny boundary.

---

*Pitchfork-and-Torch · Research firmware · MIT · Authorized targets only*
