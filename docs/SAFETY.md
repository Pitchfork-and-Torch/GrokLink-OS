# Safety Model — GrokLink OS 3.0

## Legal banner

GrokLink OS is for **authorized research, education, and owned equipment only**.
Unauthorized RF/IR/RFID/NFC/access-control interference may be a crime.
**Not a medical device.** Authors accept no liability for misuse.

Education acknowledgement phrase (session open):

```text
I_WILL_USE_ONLY_AUTHORIZED_TARGETS
```

## Default-deny

The following are **denied** unless every gate passes:

| Action class | Risk | Gates |
|--------------|------|-------|
| SubGHz/IR/NFC passive RX | `passive_rx` | edu (session), freq window, rate limit, blacklist |
| SubGHz/IR TX | `active_tx` | edu, confirm token, blacklist, duty cycle, power cap, audit |
| GPIO output | `gpio` | edu, confirm, pin blacklist, audit |
| iButton/contact write | `contact` | edu, confirm, audit |
| System (reboot, unlock, rekey) | `system` | edu, **physical confirm**, audit |

Passive info (`status`, `skill list`) may proceed with lower friction but is still audited when configured.

### Multi-LLM signal observation (3.2+) and Signal Cognition (3.5+)

PC-bridge observation tools (`observe_rx`, `observe_spectrum`, `observe_noise_floor`,
`observe_compare`, monitor sessions, `/v1/tools/*`) are **strictly passive**:

- They may call `edu_ack`, `status`, `subghz_probe`, `subghz_rx`, and `spectrum` only.
- They **must not** call `subghz_tx`, mint confirm tokens for automatic TX, drive GPIO, or reboot.
- They **must not** decode third-party payloads or invent protocol IDs from light RX edges.
- Host observation audit logs go to `~/.grok/state/groklink-os/audit/observe_audit.jsonl`.
- Calibration baselines are **host-local** heuristics, not claims of lab-grade instruments.
- Captures, calibration, and observation JSONL remain on the operator machine by default.

Every packaged observation sets `policy_context.passive_only=true`, `safety.tx=false`,
and (v2) `safety.decode=false`.

### Lab codec education (3.6+) — owned GLK1 only

GrokLink may **encode/decode its own educational lab beacon (GLK1)** for owned-lab demos
and may explain rolling codes at a conceptual level.

| Allowed | Forbidden |
|---------|-----------|
| GLK1 encode/decode | Third-party remote / access-control decode |
| Replay demo of **lab** frames | Rolling-code prediction / seed recovery |
| Edge-timing stats without protocol ID | Brand protocol libraries for cars/garages |
| Human-gated TX plan for lab beacons | Automatic TX or cloning workflows |

See [LAB_CODEC.md](LAB_CODEC.md) and [ROLLING_CODES_EDUCATION.md](ROLLING_CODES_EDUCATION.md).

See [SIGNAL_OBSERVABILITY.md](SIGNAL_OBSERVABILITY.md) and
[design/v3.5-signal-cognition.md](design/v3.5-signal-cognition.md).

## Confirm tokens

- Issued via RPC/CLI with TTL (default 60 s).
- Single-use, action-scoped (and optional freq/pin scope).
- Physical on-device OK required for `system`.
- Agent **cannot** mint tokens for itself without operator policy flag (default off).

## Blacklists

SD paths (v2 compatible):

```text
/groklink/blacklist/freq_mhz.json
/groklink/blacklist/gpio_pins.json
/groklink/blacklist/protocols.json
```

- Editable only offline (mass storage / authorized deploy).
- No RPC to wipe blacklists.
- Corrupt or missing TX-relevant blacklist → **fail closed** for TX.

## Duty cycle & rate limits

Enforced in **policy + radio arbiter** (not only userspace):

- Max TX duration per shot.
- TX cooldown.
- RX spacing (protect USB/radio stability).
- Global circuit breaker after N consecutive radio faults.

## Audit

Append-only segments with hash chaining:

```text
prev_hash | ts | actor | action | risk | decision | reason | detail
```

Export via RPC bulk or SD. Integrity verify tool in bridge: `groklink-os audit verify`.

## Degraded modes

| Condition | Behavior |
|-----------|----------|
| No SD | RPC status + safe passive only; no mission load; no TX |
| Corrupt blacklist | TX denied; RX optional per config |
| OOM | Abort step; release resources; audit `oom`; no actuator |
| Radio fault | Breaker open; sleep PHY; audit |
| Audit full | Stop elevated actions; keep denies |

## ML

Model outputs are **advisory**. Any step that actuates hardware must pass
`glk_policy_check` independently. There is no “auto-TX on anomaly” path.

## Healthcare / MedSec

Passive research only. No clinical claims. Not a medical device — never for
diagnosis, treatment, care monitoring, or patient-connected use.

- v2 packs/runbook: `GrokLink-Firmware` healthcare templates (ethics still bind)
- Worldwide next steps / product path: [MEDSEC_WORLDWIDE_NEXT_STEPS.md](MEDSEC_WORLDWIDE_NEXT_STEPS.md)
- Same ethics, stronger isolation; TX only under written RoE + Faraday + dual control
