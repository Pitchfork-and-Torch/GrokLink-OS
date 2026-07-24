# Field Report — GrokLink OS v3.7.0

**Date:** 2026-07-22  
**Firmware:** 3.7.0 · **API:** 5  
**Artifact:** `GrokLink-OS-v3.7.0-radio.dfu` (35,252 payload bytes)  
**Scope:** Authorized lab only · passive RF + owned-lab codec education  
**Authors:** Pitchfork-and-Torch  

> Phrase: `I_WILL_USE_ONLY_AUTHORIZED_TARGETS`  
> No third-party payload decode · no rolling-code prediction · TX remains human-gated  

Companion machine data:

- [`field-suite-v3.7.0-raw.json`](field-suite-v3.7.0-raw.json) — automated suite results  
- [`field-suite-v3.7.0-packaged.json`](field-suite-v3.7.0-packaged.json) — host packager + lab-codec checks  
- Architecture poster: [`docs/assets/architecture-infographic.jpg`](../assets/architecture-infographic.jpg)

---

## 1. Flash & identity (new capabilities baseline)

| Check | Result |
|-------|--------|
| DFU program | **100%** · 35,252 payload bytes |
| CDC re-enumeration | USB Serial `0483:5740` · COM7 |
| `ping` | `ok` · **version `3.7.0`** · api 5 · native |
| `edu_ack` | `edu: true` |
| `status` | missions **4** · skills **3** · heap_free **4096** · blacklist_ok · sd false |
| ROM missions | `lab_passive_433`, `lab_spectrum_planner`, `lab_passive_watch`, `lab_noise_baseline` |
| ROM skills | `lab_passive_listen`, `lab_anomaly_watch`, `lab_gpio_event` |

**Verdict:** Silicon identity matches release **v3.7.0**. USB-first / light-RPC product path is on-device.

---

## 2. Device suite (post-flash live)

| Area | Result |
|------|--------|
| Identity / edu / status | **PASS** |
| Mission list + skill list | **PASS** |
| `subghz_probe` (hw) | **PASS** · version 20 · hw true |
| `subghz_rx` @ 433.92 MHz, 150 ms | **PASS** · host round-trip ~121 ms |
| Offline agent on → ping → vault_tail → off | **PASS** (CDC stays responsive) |
| Mission arm / status / disarm (`lab_passive_watch`) | **PASS** |
| Audit tail | **PASS** |
| Second RX @ 315 MHz | **FAIL** · serial RPC timeout (post multi-command stress) |
| Light spectrum | **FAIL** · timeout after prior radio dwell |

**Device score: 15 / 17** (pass rate **88.2%**).

Critical identity, policy, first-band RX, offline agent, and mission control all pass on **3.7.0 silicon**.

---

## 3. New capability focus (v3.7.0)

| Capability | Field evidence |
|------------|----------------|
| USB-first boot + CDC stability | Immediate post-flash `ping` / `status` / `edu` without VID_0000 bind failure |
| Light RPC while services live | Offline agent ticks did not starve subsequent `ping` |
| Host DTR / field SPI split | Host session remained CDC-primary for full 15-pass block |
| ROM passive missions | Arm/status/disarm on `lab_passive_watch` ok |
| Signal Cognition host layer | 27 OpenAI-format observe tools; packager narratives + safety flags |
| GLK1 lab codec | Encode → decode **PASS**; third-party hex **refused**; replay same-bytes **true** |
| Rolling-code education | Host education path present; **no prediction tooling** |

---

## 4. Host Signal Cognition & lab codec

| Test | Result |
|------|--------|
| `tools_openai_format` | **PASS** · 27 tools |
| `lab_beacon` encode (GLK1) | **PASS** |
| `lab_beacon` decode FIELD payload | **PASS** |
| Reject non-GLK1 (`deadbeef…`) | **PASS** · `not_lab_beacon` · third-party decode false |
| Replay demo same counter | **PASS** · `same_bytes: true` |
| Rolling-code education | **PASS** · forbids prediction class tooling |

Packaged observation safety invariants:

- `safety.tx = false`  
- `safety.decode = false`  
- `authorized_use_only = true`  

---

## 5. Known field issue (unchanged class)

Long or **repeated radio dwells** can leave Windows CDC in **WriteFile error 121** / RPC timeout until cable unplug/replug.

Mitigation for agents (v3.7.0 ops):

1. Prefer **one short RX** (≤150–200 ms) per connection for field scripts  
2. Avoid chaining 315 + spectrum immediately after 433 dwell  
3. Host packaging of last good raw RPC remains valid  
4. Reconnect COM after multi-RX stress before more RPC  

This does **not** invalidate the 433 hardware RX path or identity results collected before stress.

---

## 6. Safety audit (field)

| Invariant | Observed |
|-----------|----------|
| Edu gate used | Yes |
| Observation path non-TX | Yes (suite never issued confirm/TX) |
| Third-party decode refused | Yes |
| Rolling-code prediction | Absent |
| Default-deny TX | Not exercised; observe tools cannot TX |
| Blacklist flag | `blacklist_ok: true` on status |

---

## 7. Scorecard

| Metric | Value |
|--------|-------|
| Device automated checks | **15 / 17** |
| Host lab codec / education | **All pass** |
| Observe tools available | **27** |
| Version on silicon | **3.7.0** |
| DFU payload | 35,252 bytes |

---

## 8. Recommendations

1. Keep agent dwells short; one logical RX per connection when scripting.  
2. Prefer `observe_noise_floor` once, then `observe_rx` with calibrated occupancy.  
3. Use GLK1 lab tools only for education; never third-party remote decode.  
4. Follow-up firmware: further USB pump hardening for multi-band `spectrum` / chained RX (roadmap 3.7.x soak).  
5. Operator one-pager: flash → ping → edu-ack → single observe-rx → unplug for field → plug-sync.

---

## 9. Conclusion

**GrokLink OS v3.7.0 is field-operational** on silicon for:

- USB-stable GrokRPC identity and policy  
- Hardware SubGHz probe + short passive RX  
- Offline agent without starving CDC  
- ROM mission arm/disarm  
- Host Signal Cognition (27 tools) + GLK1 lab codec education  

Field score **15/17** on device; remaining fails are **host USB stack stress after multi-RX**, not missing product features. Ready for continued authorized lab use.

---

*Pitchfork-and-Torch · Research firmware · MIT · Authorized targets only*
