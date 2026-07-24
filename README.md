# GrokLink OS

<p align="center">
  <img src="docs/assets/logo.jpg" alt="GrokLink OS logo" width="160"/>
</p>

<p align="center"><strong>From-scratch research RTOS for portable multi-radio hardware.</strong></p>

[![License: MIT](https://img.shields.io/badge/License-MIT-cyan.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-3.7.1-blue.svg)](VERSION)
[![Release](https://img.shields.io/github/v/release/Pitchfork-and-Torch/GrokLink-OS)](https://github.com/Pitchfork-and-Torch/GrokLink-OS/releases/latest)

> **Not an overlay, not a fork, and not a patch** on official Flipper Zero / Momentum / Unleashed firmware.
> GrokLink OS owns its kernel, HAL, drivers, services, GUI, and safety path end-to-end.
> Prior product ([GrokLink-Firmware](https://github.com/Pitchfork-and-Torch/GrokLink-Firmware) v2.x) was a Furi overlay.

---

## LEGAL / ETHICAL WARNING

For **authorized research, education, and equipment you own** (or are explicitly authorized to operate on).

Unauthorized access to RF / IR / RFID / NFC / access-control / vehicle systems may be a crime.
Authors accept **no liability** for misuse.

- **TX**, **GPIO drive**, **contact**, and **system** actions are **default-deny**.
- Confirm tokens, blacklists, duty limits, and append-only audit logs are mandatory.
- **Not a medical device** — never for diagnosis, treatment, care monitoring, or patient-connected use.
- **MedSec** = authorized research / facility RF instrument under written RoE (see [docs/HEALTHCARE_OPERATOR_RUNBOOK.md](docs/HEALTHCARE_OPERATOR_RUNBOOK.md)).
- Profile **`medsec-strict`** forbids all TX (passive only).
- **No third-party remote decode** or rolling-code prediction tooling.

Education phrase:

```text
I_WILL_USE_ONLY_AUTHORIZED_TARGETS
```

---

## Latest: v3.7.0 — USB-stable field unit

| Area | Status |
|------|--------|
| USB CDC JSON GrokRPC | **Stable** — USB-first boot, light RPC in RX callback |
| Windows identity | `VID_0483` / `PID_5740`, product **GrokLink OS** |
| Host vs field SPI | Host DTR keeps CDC primary; field SPI when unplugged / GUI arm |
| CC1101 SPI (bit-bang SPI_R) | Live — probe VERSION `0x14`; passive RX `sim:false` |
| On-device GUI | ST7567: HOME / RADIO / SAFETY / ABOUT |
| Policy default-deny TX | Live |
| Multi-LLM signal observability | Live host bridge (observe tools, schema v2) |
| Signal Cognition | Noise-floor calibration, pulse-rate, taxonomy |
| Lab Codec (GLK1) | Owned-lab beacon encode/decode education only |
| ROM passive missions | 7 missions without SD (incl. MedSec) |
| Offline agent + RAM vault | Passive RX only; plug-sync on reconnect |
| MedSec packs + lab evidence CLI | Passive skills/missions; `groklink-os lab *` |

Plan for follow-on work: [docs/ROADMAP_3.7.md](docs/ROADMAP_3.7.md) · MedSec: [docs/MEDSEC_WORLDWIDE_NEXT_STEPS.md](docs/MEDSEC_WORLDWIDE_NEXT_STEPS.md).

### MedSec quick path

```powershell
groklink-os edu-ack
groklink-os lab medsec-demo
groklink-os lab engagement-init --operator lab-op1 --engagement ENG-001 --site bench --roe-ack
groklink-os lab casefile --dir cases/ENG-001 --title "passive baseline" --hypothesis "quiet ISM"
```

---

## Architecture

<p align="center">
  <img src="docs/assets/architecture-infographic.jpg" alt="GrokLink OS 3.7 architecture — USB-stable stack, default-deny safety, multi-LLM observe layer" width="100%"/>
</p>

```
PC bridge (groklink-os)
        | USB CDC 230400
        v
+-----------------------------------------+
| GUI (ST7567) · Apps                     |
| Services: policy · agent · rpc · skill  |
|           audit · storage · power · ml  |
| Drivers: CC1101 SubGHz · gpio · ir ...  |
| GrokLink RTOS kernel + HAL              |
| BSP: host sim | stm32wb55 (F7 class)    |
+-----------------------------------------+
```

Field report (v3.7.0 silicon): [docs/lab/FIELD_REPORT_v3.7.0.md](docs/lab/FIELD_REPORT_v3.7.0.md)

---

## Flash (device)

1. Enter DFU: unplug, hold **BACK + OK**, plug USB → **DFU in FS Mode** (`0483:DF11`).
2. Flash latest DFU from [Releases](https://github.com/Pitchfork-and-Torch/GrokLink-OS/releases/latest):

```powershell
.\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.7.0-radio.dfu
# or: qFlipper-cli firmware path\to\GrokLink-OS-v3.7.0-radio.dfu
```

3. After reboot: USB Serial (`0483:5740`) @ **230400**. qFlipper protobuf errors after flash are expected.

```powershell
groklink-os ping
groklink-os edu-ack
groklink-os status
```

Recover stock Flipper / lab overlay: `.\tools\recover_flipper.ps1`

Detail: [docs/FLASH_LATEST.md](docs/FLASH_LATEST.md) · [docs/QFLIPPER_AND_WINDOWS.md](docs/QFLIPPER_AND_WINDOWS.md)

---

## Quick start (host bridge)

```powershell
git clone https://github.com/Pitchfork-and-Torch/GrokLink-OS.git
cd GrokLink-OS/bridge
py -3 -m pip install -e ".[serial]"
# $env:GLK_SERIAL_PORT = "COMx"
groklink-os ping
groklink-os edu-ack
groklink-os observe-rx --freq 433920000 --ms 400
```

Unplugged field (passive only):

```powershell
groklink-os prepare-unplugged --id lab_passive_watch
# or on device: SAFETY page, hold OK ~2s
# on reconnect:
groklink-os plug-sync --clear-vault
```

---

## Docs

| Doc | Topic |
|-----|--------|
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Stack overview |
| [SAFETY.md](docs/SAFETY.md) | Policy model |
| [SIGNAL_OBSERVABILITY.md](docs/SIGNAL_OBSERVABILITY.md) | Multi-LLM observe schema |
| [UNPLUGGED_AUTONOMY.md](docs/UNPLUGGED_AUTONOMY.md) | Offline agent |
| [PLUG_SYNC_RESEARCH.md](docs/PLUG_SYNC_RESEARCH.md) | Reconnect ingest |
| [LAB_CODEC.md](docs/LAB_CODEC.md) | GLK1 education |
| [ROADMAP_3.7.md](docs/ROADMAP_3.7.md) | 3.7.x / 3.8 plan |
| [BUILD.md](docs/BUILD.md) | Toolchain / DFU build |

Agent skill: `agent-skill/groklink-os/`

---

## Support the work

GrokLink OS is **free and open source**. Bug reports and feature requests are welcome via [GitHub Issues](https://github.com/Pitchfork-and-Torch/GrokLink-OS/issues).

## License

MIT — see [LICENSE](LICENSE).
