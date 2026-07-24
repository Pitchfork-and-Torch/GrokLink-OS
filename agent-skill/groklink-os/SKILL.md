---
name: groklink-os
description: >
  Operate GrokLink OS research firmware via the PC bridge (groklink-os):
  multi-LLM signal observability, Signal Cognition calibration, owned-lab GLK1
  beacon encode/decode, educational replay demo, rolling-code concept education
  (never prediction), adaptive firmware sync, edu-ack, passive SubGHz RX, skill
  craft, human-gated confirms. Authorized RF research only. Differentiator:
  native GrokLink OS 3.7+ with USB-stable GrokRPC, calibrated observations,
  and owned-lab codec.
  Triggers: /groklink-os, "lab beacon", "GLK1", "rolling code education",
  "observe signals", "noise floor", "signal world", "flash groklink".
metadata:
  short-description: "GrokLink OS bridge + lab codec education (v3.7)"
  tags: ["groklink", "firmware", "subghz", "openai", "tools", "observability", "safety", "rpc"]
  priority: 35
  required-tools: []
  example-user-utterances:
    - "use groklink os"
    - "observe the 433 MHz band"
    - "what signals are active right now"
    - "start a passive RF monitor"
    - "groklink-os status"
    - "craft a skill from this capture"
    - "sync groklink knowledge"
  requires: []
  composes-with: ["skill-router", "public-github-hygiene"]
  allowed-tools: ["run_terminal_command", "read_file", "search_replace", "list_dir"]
  repo: "https://github.com/Pitchfork-and-Torch/GrokLink-OS"
---

# GrokLink OS (Adaptive Agent Skill + Signal World)

Control **GrokLink OS** through the **`groklink-os`** PC bridge. Prefer **passive
signal observation** via structured tools so the model can “see” the RF environment.

Learning data: `~/.grok/state/groklink-os/` — never publish captures.

## Hard rules (non-negotiable — never “learn away”)

1. **Authorized targets only.** Equipment the operator owns or is explicitly authorized to operate on.
2. **Always `edu-ack`** before elevated/RX commands (observation tools auto-ack).
3. **Default-deny TX / GPIO / contact / system.** Do not invent confirm tokens.
4. **Human-in-the-loop for TX.** Issue confirm → operator approves → only then TX.
5. **Observation tools never TX.** Only passive RPC paths.
6. **Lab codec only for GLK1.** Never third-party remote decode; never rolling-code prediction.
7. **Not a medical device.** No clinical claims. MedSec = authorized passive research only
   (`groklink-os lab medsec-demo`, `mission-run --id medsec_lab_passive_ism`, engagement/casefile).
8. **No illegal intercept / access-control abuse.** If intent is unclear, stop and clarify.
9. **Unplugged:** PC/LLM needs USB; on-device agent can passive-explore after `prepare_unplugged` or SAFETY+hold OK.
10. **Every reconnect:** run `groklink-os plug-sync` (or `ingest_unplugged_lessons`) before other work — document vault lessons into `~/.grok/state/groklink-os/`.

## Adaptive loop (run first every session)

```powershell
$S = "$HOME/.grok/skills/groklink-os/scripts"
# When packaged inside the firmware repo:
# $S = "<repo>/agent-skill/groklink-os/scripts"

# 0) If Flipper just plugged in after field explore — ALWAYS ingest first
groklink-os plug-sync --clear-vault
# py -3 "$S/ingest_unplugged.py" --clear-vault

py -3 "$S/refresh_all.py"
```

Then read:

- `~/.grok/state/groklink-os/research/lessons_learned_summary.json`
- `~/.grok/state/groklink-os/notes/unplugged_research_journal.md`
- `~/.grok/state/groklink-os/firmware_snapshot.json` and `capabilities.json`

## Signal world (preferred for RF questions)

Teach the operator/LLM to use **observation tools**, not raw pulse dumps alone.

```powershell
# Real Flipper-class device over USB CDC:
# $env:GLK_SERIAL_PORT = "COM5"
# pip install 'groklink-os[serial]'

groklink-os tools-schema
groklink-os list-ports
groklink-os observe-status
groklink-os observe-rx --freq 433920000 --ms 400
groklink-os observe-session --freqs 433920000
groklink-os observe-spectrum --freqs 315000000,433920000
groklink-os tool-call get_recent_activity --args "{\"limit\":10}"
# Optional local OpenAI-tools HTTP API (loopback):
groklink-os observe-serve --port 8741

# Skill script (also: main.py observe)
py -3 "$S/observe_session.py" --freqs 433920000 --learn
```

Python (OpenAI function calling):

```python
from groklink_os.observe.tools import ToolDispatcher, tools_openai_format
tools = tools_openai_format()
d = ToolDispatcher()
obs = d.dispatch("observe_rx", {"freq_hz": 433920000, "ms": 400})
print(obs["result"]["narrative"])
d.close()
```

**How to reason over results (v3.5 Signal Cognition):**

- Read `result.narrative` first (one-line situation).
- Prefer `activity.calibrated_occupancy` when calibration exists; else `activity.occupancy`.
- Also use `stats.pulse_rate_hz`, `calibration.snr_est_db`, `activity.confidence`.
- Events are non-decode only (`payload_hex` always null) — never invent protocols.
- Multi-turn: `observe_noise_floor` → `observe_rx` → `observe_spectrum` / `observe_compare`.
- Schema: `get_observation_schema` + `get_event_taxonomy`.

```powershell
groklink-os event-taxonomy
groklink-os observe-noise-floor --freq 433920000 --ms 200
groklink-os observe-rx --freq 433920000 --ms 400
groklink-os observe-compare --freq-a 433920000 --freq-b 315000000
groklink-os calibration-state
# Owned-lab codec education (GLK1 only):
groklink-os lab-beacon-encode --lab-id 1 --counter 3 --message HELLO
groklink-os lab-beacon-decode --hex <hex>
groklink-os lab-replay-demo --counter 7
groklink-os explain-rolling-codes
```

Detail: `references/signal-observability.md`, `docs/LAB_CODEC.md`,
`docs/ROLLING_CODES_EDUCATION.md`, `docs/lab/SIGNAL_COGNITION_PLAYBOOK.md`.

## State-check + lab session

```powershell
py -3 "$S/session_check.py" --dry-run
py -3 "$S/session_check.py" --rx --freq 433920000 --ms 400
```

Or CLI:

```powershell
groklink-os ping
groklink-os edu-ack
groklink-os status
groklink-os rx --freq 433920000 --ms 400
```

Install bridge:

```powershell
git clone https://github.com/Pitchfork-and-Torch/GrokLink-OS.git
cd GrokLink-OS/bridge
py -3 -m pip install -e .
```

Env: `GLK_RPC_HOST` (default `127.0.0.1`), `GLK_RPC_PORT` (default `7341`),
`GROKLINK_OS_ROOT`, `GROKLINK_OS_LEARN_DIR`.

## Learn from new data

```powershell
py -3 "$S/learn_from_data.py" --capture path/to/capture.jsonl
py -3 "$S/learn_from_data.py" --session path/to/session.json
py -3 "$S/learn_from_data.py" --summary
```

Observations auto-append under `observations/recent.jsonl` when using observe tools.

## Workflows

### A — Passive signal awareness (LLM)

1. `refresh_all` / read capabilities  
2. `get_device_status`  
3. `observe_rx` / `observe_spectrum` on owned-lab bands  
4. Summarize occupancy + hottest bands for the operator  
5. Optional: `start_monitor` for multi-turn situational awareness  

### B — Craft skill

capture (owned) → craft → human review → learn_from_data → never auto-TX.

### C — TX (operator-gated only)

```powershell
groklink-os edu-ack
groklink-os confirm-issue --action subghz_tx --freq <Hz>
# STOP — human must approve token
```

## Baseline CLI map (fallback)

| Command | Purpose |
|---------|---------|
| `groklink-os tools-schema` | OpenAI tool JSON |
| `groklink-os observe-rx` | Packaged RX observation |
| `groklink-os observe-spectrum` | Packaged spectrum observation |
| `groklink-os observe-serve` | Local tools HTTP API |
| `groklink-os tool-call <name>` | Dispatch any observe tool |
| `groklink-os ping` / `edu-ack` / `status` | Session basics |
| `groklink-os rx` / `spectrum` | Raw RPC (prefer observe-*) |
| `groklink-os craft` | Draft skill from capture |
| `groklink-os confirm-issue` | Mint human confirm token |

## Output format

```markdown
## GrokLink OS session
- Knowledge: version=… release=… delta=…
- Device: edu=… radio=… sim/hw=…
- Observation: narrative=… occupancy=… rssi=… pulses=… hottest=…
- Learning: sessions=N captures=M observations=…
- Safety: passive only | confirm issued (awaiting human)
- Next step for operator: …
```

## Sharing

Ship `SKILL.md` + `scripts/` + `references/` + optional `assets/`.  
Do **not** ship `~/.grok/state/groklink-os/`.

Public repo: [GrokLink-OS](https://github.com/Pitchfork-and-Torch/GrokLink-OS) `agent-skill/groklink-os/`.
