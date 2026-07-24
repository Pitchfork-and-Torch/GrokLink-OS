# GrokRPC API reference (JSON debug mode)

Transport (host): TCP `127.0.0.1:7341`, one JSON object per line, one JSON response per line.  
Transport (device): USB CDC 230400, same JSON lines (deferred radio — not in USB IRQ).

Binary framing: magic `GL`, version 3, type, seq, length, payload, CRC32 — see `glk_rpc_frame_encode`.

**LLM / rich observations:** device responses stay compact. The PC bridge packages them into
`groklink.signal_observation.v1` — see [SIGNAL_OBSERVABILITY.md](../SIGNAL_OBSERVABILITY.md).

## Commands

### ping

```json
{"cmd":"ping"}
→ {"ok":true,"cmd":"pong","api":3,"version":"3.3.0"}
```

### edu_ack

```json
{"cmd":"edu_ack","phrase":"I_WILL_USE_ONLY_AUTHORIZED_TARGETS"}
```

### status

```json
{"cmd":"status"}
```

### confirm_issue

```json
{"cmd":"confirm_issue","action":"subghz_tx","ttl_sec":60,"freq_hz":433920000}
```

### subghz_rx

```json
{"cmd":"subghz_rx","freq_hz":433920000,"ms":500}
→ {"ok":true,"err":0,"freq_hz":433920000,"ms":400,"pulses":12,"rssi":-67,"sim":false,"ts_ms":1234,"kind":"rx"}
```

Fields `ts_ms` and `kind` added in 3.2 (backward compatible extras).

### subghz_probe

```json
{"cmd":"subghz_probe"}
→ {"ok":true,"partnum":0,"version":20,"hw":true}
```

### subghz_tx

```json
{"cmd":"subghz_tx","freq_hz":433920000,"path":"/path/file.sub","confirm_id":"C..."}
```

Honest host mode: `tx_mode: ack_file` when path validated. **Not** available via observation tools.

### spectrum

```json
{"cmd":"spectrum","freqs":[300000000,433920000],"ms":400,"settle_ms":2000}
→ {"ok":true,"kind":"spectrum","ms":400,"settle_ms":2000,"ts_ms":...,"bands":[{"freq_hz":...,"pulses":...,"rssi":...}]}
```

### mission_list / mission_arm / mission_disarm / mission_status / mission_step / mission_run

```json
{"cmd":"mission_list"}
{"cmd":"mission_arm","id":"lab_passive_433"}
{"cmd":"mission_status","id":"lab_passive_433"}
{"cmd":"mission_step"}
{"cmd":"mission_run","steps":8}
{"cmd":"mission_disarm","id":"lab_passive_433"}
```

ROM catalog (v3.7.1+, no SD required): lab passives + MedSec  
`lab_passive_433`, `lab_spectrum_planner`, `lab_passive_watch`, `lab_noise_baseline`,  
`medsec_lab_passive_ism`, `fac_rf_snapshot_passive`, `medsec_passive_watch` — **passive only**.  
`status` may include `not_medical_device`, `profile`, `medsec_strict`.

### agent_offline / agent_auto / agent_status (v3.4+)

```json
{"cmd":"agent_auto","id":"lab_passive_watch","on":true}
{"cmd":"agent_offline","on":true}
{"cmd":"agent_status"}
```

Device main loop runs one IR step ~600 ms when USB is idle and offline is enabled.

### vault_tail / vault_clear / catalog_reload

```json
{"cmd":"vault_tail","n":8}
{"cmd":"vault_clear"}
{"cmd":"catalog_reload"}
```

RAM vault holds recent mission RX/infer/done events without SD.

### skill_list / audit_tail / physical_confirm

See `glk_rpc.c` handlers.
