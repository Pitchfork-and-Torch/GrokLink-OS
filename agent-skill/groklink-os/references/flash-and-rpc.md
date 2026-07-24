# GrokLink OS — Flash & RPC (agent reference)

Public repo: https://github.com/Pitchfork-and-Torch/GrokLink-OS

Prefer **live** data from `scripts/sync_firmware_knowledge.py` and
`scripts/probe_capabilities.py` over this static file when they disagree.

## Product boundary

| Product | Nature |
|---------|--------|
| **GrokLink OS 3.x** | From-scratch RTOS (this skill) |
| GrokLink-Firmware 2.x | Prior Furi **overlay** — different bridge |

## DFU entry (lab hardware, Flipper F7 class)

1. Unplug USB.
2. Hold **BACK + OK** ~30 s.
3. Plug USB → **DFU in FS Mode** (`0483:DF11`).

```powershell
.\tools\build_dfu.ps1 -Profile OsRadio
.\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\<artifact>.dfu
.\tools\recover_flipper.ps1
```

Confirm with the operator before flash/recover. After flash, run:

```powershell
py -3 scripts/refresh_all.py --deep-probe
```

## RPC transport

- Live device: USB CDC JSON lines (baud commonly **230400**).
- Host simulator: TCP `GLK_RPC_HOST` / `GLK_RPC_PORT` (default `127.0.0.1:7341`).

Baseline commands (extend via sync/probe):

```json
{"cmd":"ping"}
{"cmd":"edu_ack","phrase":"I_WILL_USE_ONLY_AUTHORIZED_TARGETS"}
{"cmd":"status"}
{"cmd":"subghz_rx","freq_hz":433920000,"ms":400}
```

## Bridge install

```powershell
cd bridge
py -3 -m pip install -e .
groklink-os --help
```

## GUI pages (ST7567)

**Left / Right**: HOME · RADIO · SAFETY · ABOUT.

## Safety

Risk classes: `info` · `passive_rx` · `active_tx` · `gpio` · `contact` · `system`.  
Full model: repo `docs/SAFETY.md` (also excerpted into firmware_snapshot after sync).
