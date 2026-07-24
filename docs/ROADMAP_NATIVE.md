# Roadmap — GrokLink as a full native OS on Flipper Zero (STM32WB55)

Execution plan from bring-up DFU to a service-capable native OS (agent, safety,
radio, RPC, multi-LLM signal observability) without depending on Furi/Momentum
as a runtime (optional board compatibility only).

---

## Current state (2026-07-22) — v3.7.0

| Layer | Status |
|-------|--------|
| Host RTOS + services + bridge | **Done** |
| DFU packaging (`bin2dfu`, `build_dfu.ps1`) | **Done** |
| Bare-metal boot @ `0x08000000` | **Done** |
| USB CDC JSON RPC (device) | **Stable** (USB-first + bisect-proven) |
| Host DTR vs field SPI arbitration | **Done** (3.7.0) |
| Deferred USB-safe radio jobs | **Done** |
| CC1101 SPI_R light RX + probe | **Done** on OsRadio (VERSION 0x14) |
| On-device ST7567 GUI | **Done** (HOME / RADIO / SAFETY / ABOUT) |
| Policy default-deny + edu_ack | **Done** |
| **Multi-LLM signal observability** | **Done** (bridge observe tools + local HTTP) |
| USB CDC serial transport in bridge | **Done** (`GLK_SERIAL_PORT` / pyserial optional) |
| ROM mission/skill catalog (no SD) | **Done** |
| Passive mission RPC + LLM tools | **Done** |
| USB-safe offline agent + RAM vault | **Done** |
| Lab codec GLK1 + rolling-code education | **Done** (host; no attack tooling) |
| SD FatFs / card hot-load | Roadmap remainder (3.8) |
| BLE M0+ stack | Not started |

Detail plan: [ROADMAP_3.7.md](ROADMAP_3.7.md).

**Lab recovery:** `tools/recover_flipper.ps1` → GrokLink-Firmware v2.1.3 overlay anytime.

---

## Architecture target

```
 LLM tools (OpenAI / Claude / Grok / …)  +  localhost :8741
                    │
         bridge/groklink_os (observe · RPC · craft)
           TCP host-sim :7341  │  USB CDC serial 230400
                    │
         glk_rpc + policy + radio worker + GUI
                    │
         CC1101 SPI_R  ·  ST7567  ·  STM32WB55
```

---

## Phases

### P0 — Boot + DFU — **done**
### P1 — USB CDC console — **done**
### P2 — Kernel + policy + RPC on device — **done** (OsCdc / OsRadio)
### P3 — Radio (CC1101 light RX) — **done** (probe + passive RX; full TX gated)
### P3.5 — On-device GUI — **done**
### P3.6 — Multi-LLM signal observability — **done** (v3.4.0)

| Work | Status |
|------|--------|
| Observation schema v1 | Done |
| OpenAI + Anthropic tool specs | Done |
| Host monitor sessions + SSE | Done |
| Local tools HTTP API | Done |
| USB CDC bridge transport | Done |
| Agent skill observe_session | Done |
| Learning store observation ingest | Done |

### P4 — Storage + skills + agent

| Work | Detail | Status |
|------|--------|--------|
| ROM catalog | Builtin passive missions + skills without SD | **Done** (3.4.0) |
| Mission RPC | arm / status / step / run / disarm | **Done** |
| LLM tools | `run_passive_mission`, `list_missions`, allowlist | **Done** |
| Host SD scan | Load `sd_card/groklink/missions` on host sim | **Done** (boot) |
| SDMMC / SPI SD on device | FatFs or littlefs | Next |
| Hot-load skill packages from card | signed optional | Next |
| Autonomous offline loop without PC | agent task on device + power | Next |

**Exit (partial met):** `mission_list` + passive `mission_run` works on device **without SD**.

### P5 — Productization

| Work | Detail |
|------|--------|
| Power | stop modes, USB wake |
| BLE status | M0+ wireless binary + IPCC |
| Secure boot hooks | image header verify |
| Release polish | signed DFUs, reproducible builds |

---

## Dual-track lab strategy

| Track | Image | Use |
|-------|-------|-----|
| **A — Lab production** | GrokLink-Firmware **v2.1.3** overlay | Recovery / dual workflows |
| **B — Native OS** | `GrokLink-OS-v3.4.0-radio.dfu` | Primary research OS + LLM observe + missions |

```powershell
.\tools\build_dfu.ps1 -Profile OsRadio
.\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.4.0-radio.dfu
# LLM observe + passive mission over CDC:
$env:GLK_SERIAL_PORT = "COMx"
pip install -e "bridge/[serial]"
groklink-os observe-session --freqs 433920000
groklink-os mission-run --id lab_passive_433 --steps 8
```

---

## Engineering standards

1. **Every DFU** ships with docs note + recovery to v2.1.3.  
2. **Host tests** remain green (`pytest` bridge + `ctest` services).  
3. **Device smoke:** `device_probe.ps1` before tagging a CDC+ release.  
4. **Observation tools never TX.**  
5. **Public repos:** single product commit + secret scan (public-github-hygiene).  
