# Signal Observability — Multi-LLM RF Sensory Interface

**GrokLink OS 3.2+** lets OpenAI models and other tool-calling LLMs treat a
Flipper-class device as a **passive RF sensory peripheral**.

Authorized research / owned equipment only. **Observation paths never transmit.**

---

## 1. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  LLM (OpenAI / Claude / Grok / Cursor / Codex / …)          │
│  function-calling / tool use                                │
└───────────────────────────┬─────────────────────────────────┘
                            │ tools JSON  or  HTTP /v1/tools/*
                            v
┌─────────────────────────────────────────────────────────────┐
│  PC bridge  groklink-os                                     │
│  observe/ : schema · packager · monitor · store · tools     │
│  optional HTTP API :127.0.0.1:8741                          │
│  learning store: ~/.grok/state/groklink-os/                 │
└───────────────────────────┬─────────────────────────────────┘
                            │ JSON-line GrokRPC (TCP host-sim
                            │ or USB CDC on device)
                            v
┌─────────────────────────────────────────────────────────────┐
│  Device services: policy → radio worker → CC1101 light RX   │
│  Default-deny TX · edu_ack · deferred USB-safe radio jobs   │
└─────────────────────────────────────────────────────────────┘
```

Heavy formatting, occupancy labels, rolling summaries, and LLM packaging run
**on the host**. The device keeps compact JSON (`pulses`, `rssi`, `ts_ms`).

---

## 2. Observation schema (`groklink.signal_observation.v2`)

v3.5 **Signal Cognition** ships schema **v2** (additive; v1 fields preserved; `schema_compat` lists v1).

Every observation is self-describing JSON:

| Field | Purpose |
|-------|---------|
| `schema` / `schema_version` | Identity for LLM parsers (`v2` / `2`) |
| `schema_compat` | Includes `groklink.signal_observation.v1` |
| `observation_id` | Unique id |
| `kind` | `rx_snapshot` \| `spectrum_scan` \| `monitor_chunk` \| `device_status` \| `activity_summary` \| `noise_floor` \| `band_compare` \| … |
| `timestamps.utc` | Host UTC |
| `timestamps.device_mono_ms` | Device tick when available |
| `device` | Firmware version, edu, sim/hw flags |
| `policy_context` | Passive-only guarantees |
| `rf` | Frequency, dwell, band label |
| `stats` | pulse_rate_hz, rssi_min/max (light RX) |
| `calibration` | noise_floor_dbm, baseline_pulse_rate_hz, snr_est_db |
| `activity` | pulses, rssi_dbm, energy_score, occupancy, **calibrated_occupancy**, confidence |
| `spectrum.bands` | Multi-band vectors (+ calibrated fields) |
| `window` | Monitor / summary / compare context |
| `events` | Non-decode taxonomy only (`payload_hex` always null) |
| `raw_device` | Original RPC payload |
| `narrative` | One-line natural language for chat models |
| `safety` | `tx: false`, `decode: false`, authorized-use marker |

**Occupancy** (absolute host heuristic): `quiet` | `low` | `medium` | `high` | `unknown`.  
**Calibrated occupancy** (vs baseline): `quiet` | `ambient` | `elevated` | `busy` | `unknown`.

JSON Schema: `bridge/schemas/signal_observation.v2.json`.

---

## 3. OpenAI / multi-LLM tools

| Tool | Role |
|------|------|
| `get_observation_schema` | Schema + tool map |
| `get_device_status` | Status + optional `subghz_probe` |
| `observe_rx` | Single passive RX observation |
| `observe_spectrum` | Sequential multi-band scan |
| `start_monitor` | Host loop of passive samples |
| `stop_monitor` | Stop loop |
| `get_monitor_chunk` | Chunked session results |
| `get_recent_activity` | Local rolling history + summary |

Export schemas:

```powershell
groklink-os tools-schema
# or
groklink-os tools-schema --out tools.json
```

Dispatch one tool:

```powershell
groklink-os tool-call observe_rx --args "{\"freq_hz\":433920000,\"ms\":400}"
```

Python:

```python
from groklink_os.observe.tools import ToolDispatcher, tools_openai_format

tools = tools_openai_format()  # pass to OpenAI chat.completions
d = ToolDispatcher()
print(d.dispatch("observe_rx", {"freq_hz": 433920000, "ms": 400}))
d.close()
```

---

## 4. Transports (host sim + real device)

| Mode | Env / usage |
|------|-------------|
| TCP host-sim | `GLK_RPC_HOST=127.0.0.1` `GLK_RPC_PORT=7341` (default) |
| USB CDC device | `GLK_SERIAL_PORT=COM5` (Windows) or `/dev/ttyACM0` · baud `230400` |
| Optional dep | `pip install 'groklink-os[serial]'` (pyserial) |

```powershell
$env:GLK_SERIAL_PORT = "COM5"
groklink-os list-ports
groklink-os ping
groklink-os observe-rx --freq 433920000 --ms 200
```

## 5. Local OpenAI-compatible endpoint

```powershell
# Host OS sim or CDC device must be reachable
groklink-os observe-serve --port 8741
```

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/health` | Liveness |
| GET | `/v1/tools` | Tool definitions |
| GET | `/v1/ports` | USB serial port list |
| GET | `/v1/system_prompt` | LLM system rules |
| POST | `/v1/tools/call` | `{"name","arguments"}` |
| POST | `/v1/tools/call_batch` | OpenAI `tool_calls` array |
| POST | `/v1/session/run` | Scripted multi-step passive session |
| GET | `/v1/observations/recent?limit=20` | Recent store |
| GET | `/v1/observe/stream` | SSE monitor chunks |
| POST | `/v1/chat/completions` | Tool-exec shim + tools catalog |

Binds **127.0.0.1** by default. No cloud upload.

Example:

```powershell
curl http://127.0.0.1:8741/v1/tools
curl -X POST http://127.0.0.1:8741/v1/tools/call `
  -H "Content-Type: application/json" `
  -d "{\"name\":\"observe_rx\",\"arguments\":{\"freq_hz\":433920000,\"ms\":200}}"
```

---

## 6. Safety invariants

1. Observation tools call only **passive** RPC (`edu_ack`, `status`, `subghz_probe`, `subghz_rx`, `spectrum`).
2. **No** observation tool issues `subghz_tx`, GPIO, or system actions.
3. `edu_ack` is auto-applied by the tool dispatcher before RX.
4. Device policy still enforces edu, frequency window, RX cooldown, blacklist.
5. Captures and observation JSONL stay under `~/.grok/state/groklink-os/` (local).
6. Every tool call is audited to `audit/observe_audit.jsonl`.

---

## 7. Device RPC (compact)

Backward compatible with v3.1.5 clients. Enrichments:

```json
{"cmd":"subghz_rx","freq_hz":433920000,"ms":400}
→ {"ok":true,"freq_hz":...,"ms":...,"pulses":N,"rssi":...,"sim":false,"ts_ms":...,"kind":"rx"}

{"cmd":"spectrum","freqs":[315000000,433920000],"ms":400}
→ {"ok":true,"kind":"spectrum","ms":400,"settle_ms":2000,"ts_ms":...,"bands":[{"freq_hz":...,"pulses":...,"rssi":...}]}
```

---

## 8. Agent skill

See `agent-skill/groklink-os/SKILL.md` and `references/signal-observability.md`.

Session start:

```powershell
py -3 scripts/refresh_all.py
py -3 scripts/observe_session.py --freqs 433920000 --learn
# or
py -3 scripts/main.py observe --freqs 433920000 --mock
groklink-os observe-session --freqs 433920000
```

Ingest observation JSONL into band notes:

```powershell
py -3 scripts/learn_from_data.py --observations $HOME/.grok/state/groklink-os/observations/recent.jsonl
```

---

## 9. Tests & examples

```powershell
cd bridge
py -3 -m pip install -e ".[dev]"
py -3 -m pytest tests -q
py -3 ../examples/llm_observe_lab.py --mock
py -3 ../examples/openai_tools_example.py --mock --session
```

OpenAI / multi-LLM helper modules:

- `groklink_os.observe.agent_loop` — system prompt, tool specs, scripted session, tool_call executor
- `examples/openai_tools_example.py` — print tools + simulate assistant tool_calls

---

## 10. Design rationale (gap → solution)

| Gap in 3.1.5 | Solution in 3.2 |
|--------------|-----------------|
| Raw RPC only (pulses/rssi) | Host `ObservationPackager` + schema v1 |
| No timestamps / narrative | Host UTC + device `ts_ms` + `narrative` |
| No continuous observe | Host `MonitorSession` + chunks / SSE |
| No OpenAI tools | `TOOL_DEFINITIONS` + `ToolDispatcher` |
| No multi-LLM HTTP | stdlib `observe-serve` on :8741 |
| Skill CLI-only loop | Skill docs + tool-call workflow |
| Learning store unstructured | Observations JSONL + activity summary |
| Spectrum missing RSSI | Device bands include `rssi` |
| Bridge version drift | Bridge **3.4.0** aligned with OS |
| TCP-only host path | USB CDC serial transport + `list-ports` |
| No multi-turn scripted session | `observe-session` / `POST /v1/session/run` |
| Observations not in learn loop | `learn_from_data.py --observations` |
