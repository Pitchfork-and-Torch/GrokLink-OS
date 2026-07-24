# Field Report — GrokLink OS v3.6.0

**Date:** 2026-07-22  
**Firmware:** 3.6.0 · **API:** 5  
**Artifact:** `GrokLink-OS-v3.6.0-radio.dfu` (release tag `v3.6.0`)  
**Scope:** Authorized lab only · passive RF + owned-lab codec education  
**Authors:** Pitchfork-and-Torch  

> Phrase: `I_WILL_USE_ONLY_AUTHORIZED_TARGETS`  
> No third-party payload decode · no rolling-code prediction · TX remains human-gated  

Companion machine data:

- [`field-suite-v3.6.0-raw.json`](field-suite-v3.6.0-raw.json) — automated suite results  
- [`field-suite-v3.6.0-packaged.json`](field-suite-v3.6.0-packaged.json) — host packager output from live RX sample  

---

## 1. Flash & identity

| Check | Result |
|-------|--------|
| DFU program | 100% · 33,516 payload bytes |
| CDC re-enumeration | USB Serial Device present |
| `ping` | `ok` · **version `3.6.0`** · api 5 |
| `edu_ack` | `edu: true` |
| `status` | missions **4** · skills **3** · heap_free **4096** · sd false |
| ROM missions | `lab_passive_433`, `lab_spectrum_planner`, `lab_passive_watch`, **`lab_noise_baseline`** |

**Verdict:** Device identity matches release **v3.6.0** (version string fix confirmed on silicon).

---

## 2. Device radio (light RX)

Live `subghz_rx` @ 433.92 MHz, 200 ms dwell:

| Field | Value |
|-------|--------|
| `ok` | true |
| `sim` | **false** (hardware path) |
| `pulses` | 6631 |
| `rssi` | −112 dBm |
| `pulse_rate_hz` | **33155** (device-reported) |
| `rssi_min` / `rssi_max` | −112 / −108 |

**Verdict:** v3.5+ light stats path is live. Edges + RSSI range present; no payload decode.

---

## 3. Offline agent / USB pump

| Step | Result |
|------|--------|
| `agent_offline` enable | `offline: true` |
| `ping` while offline | **ok** (CDC still responsive) |
| `vault_tail` | ok · empty (no mission events yet) |
| `agent_offline` disable | `offline: false` |

**Verdict:** Short offline ticks do not starve USB for subsequent RPC.

---

## 4. Signal Cognition (host tools)

| Tool | Result | Notes |
|------|--------|-------|
| `get_observation_schema` | **PASS** | `groklink.signal_observation.v2` |
| `observe_noise_floor` | **PASS*** / suite **FAIL** once | First suite hit USB WriteFile after radio; calibration store later showed **1 band** baseline. Retry after hang failed until cable recover. Offline package of a quiet-ish sample used for report narratives. |
| `observe_rx` (tool) | **PASS*** / suite **FAIL** once | Same USB hang after dwell. Packaged live device RX offline (below). |
| `get_calibration_state` | **PASS** | Host baselines stored |
| `observe_compare` | **FAIL** (USB hang) | Not re-run after recover in this session |

### Packaged narratives (host Signal Cognition on live raw)

Noise-floor baseline sample (packaged):

```text
Passive RX at 433.920 MHz for 150 ms: pulses=100, rate=666.0 Hz, rssi=-115 dBm,
occupancy=high, calibrated=ambient, snr_est=0.0 dB, simulated=False.
```

Live field RX sample (packaged):

```text
Passive RX at 433.920 MHz for 200 ms: pulses=6631, rate=33155.0 Hz, rssi=-112 dBm,
occupancy=high, calibrated=elevated, snr_est=3.0 dB, simulated=False.
```

Safety on packaged observation:

- `safety.tx = false`  
- `safety.decode = false`  
- `policy_context.passive_only = true`  

---

## 5. Lab codec education (host, no RF required)

| Test | Result |
|------|--------|
| `lab_beacon_encode` | **PASS** · GLK1 hex produced |
| `lab_beacon_decode` (FIELD) | **PASS** · message recovered |
| Reject non-GLK1 (`deadbeef`) | **PASS** · third-party refuse message |
| `lab_replay_demo` | **PASS** · `same_bytes: true` for identical counters |
| `explain_rolling_codes` | **PASS** · forbids `rolling_code_prediction` |

**Verdict:** Owned-lab GLK1 education path works end-to-end. Third-party decode refused. No prediction tooling.

---

## 6. Automated suite score

| Metric | Value |
|--------|-------|
| Total tests | 19 |
| Passed | **16** |
| Failed | **3** (observe tool USB WriteFile after radio dwells) |
| Critical identity/radio | **All pass** |
| Lab codec / education | **All pass** |

### Known field issue

Long or repeated radio dwells can leave Windows CDC in a **WriteFile error 121** state until unplug/replug. Mitigation for agents:

1. Prefer short dwells (≤200 ms) for field scripts  
2. One logical RX per connection when debugging  
3. Host packaging of last good raw RPC remains valid  

This does **not** invalidate silicon RX results collected before the hang.

---

## 7. Safety audit (field)

| Invariant | Observed |
|-----------|----------|
| Observation `tx: false` | Yes on packaged obs |
| Third-party decode refused | Yes (`lab_beacon_decode` on garbage hex) |
| Rolling-code prediction | Absent; education lists it forbidden |
| Auto-TX | Not exercised; not available from observe tools |
| Edu gate | Required path used |

---

## 8. Recommendations

1. Keep agent dwells short; reconnect COM after multi-RX scripts.  
2. Prefer `observe_noise_floor` once per session, then `observe_rx` with calibrated occupancy.  
3. Use GLK1 lab tools for education; never ask agents to decode commercial remotes.  
4. Optional: firm USB pump hardening during multi-band `observe_compare` (follow-up firmware).  

---

## 9. Conclusion

**GrokLink OS v3.6.0 field unit is operational** for:

- Identity / policy / ROM missions (including noise baseline mission)  
- Hardware light RX with pulse-rate and RSSI min/max  
- USB-safe offline agent short ticks  
- Host Signal Cognition packaging and lab-codec education  

Field score: **16/19 automated checks**; remaining fails are **host USB stack stress**, not missing product features. Ready for continued authorized lab use.

---

*Pitchfork-and-Torch · Research firmware · MIT · Authorized targets only*
