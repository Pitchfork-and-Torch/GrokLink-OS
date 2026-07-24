# Limitations Analysis — GrokLink-Firmware v2.1.x → GrokLink OS 3.0

This document inventories every major constraint discovered in the production
overlay ([GrokLink-Firmware](https://github.com/Pitchfork-and-Torch/GrokLink-Firmware)
v2.1.3) and states how GrokLink OS (native) addresses it.

Analysis sources: local tree at `GrokLink-Firmware`, `docs/ARCHITECTURE.md`,
`docs/WHAT_WE_LEARNED.md`, `docs/ASYNC_RADIO_DESIGN.md`, `gl_config.h`, safety
headers, field reports, and lab boot-loop experience.

---

## 1. Architectural class: overlay fragility

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| Depends on Flipper Furi + Momentum | Overlay patches into foreign scheduler, HAL, RPC, storage | Clean-room kernel + HAL; optional Flipper board profile only as a *port* |
| FAP / service registration coupling | `application.fam`, fbt, api_symbols, STARTUP hooks | First-class services registered with GrokLink kernel |
| Dual maintenance | Upstream Flipper/Momentum churn breaks overlay | Own release train; board support packages (BSP) versioned separately |
| Cannot reshape concurrency | Stuck with Furi task model and shared radio workers | Preemptive multi-priority kernel with radio arbitration |

---

## 2. Memory & flash

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| Agent+RPC extras **~40–60 KB .text**, hard ceiling **~80–120 KB** | Overlay budget to leave room for stock firmware | Full image owned by GrokLink; modular link sections; optional skill modules offload to SD |
| Agent heap **< ~12 KB** | Mission buffers tiny; no ML arena | Pooled heaps + per-task budgets; agent arena target **32–48 KB** usable; optional TinyML arena **16–32 KB** |
| Static caps: 16 missions, 24 skills, 32 steps | `gl_config.h` hard limits | Configurable pools; default raised (32 missions, 64 skills, 64 steps) with compile-time budgets |
| No fragmentation control | Ad-hoc allocs inside Furi | Memory pools + monitored heap with watermarks and OOM fail-closed |

**STM32WB55 reality (unchanged silicon):**

| Resource | Typical Flipper Zero (WB55) |
|----------|-----------------------------|
| Flash | 1 MB (shared with wireless stack / options) |
| SRAM | 256 KB total; app-usable portion board-dependent |
| Dual-core | Cortex-M4 app + Cortex-M0+ network (BLE stack) |

OS 3.0 **does not** invent RAM; it **reclaims** budget by removing the stock GUI
app suite when building a research image, and by isolating optional subsystems.

---

## 3. Concurrency & scheduling

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| 1 Hz mission tick | Coarse autonomy | Event-driven agent + high-res kernel timers |
| Blocking SubGHz in agent/RPC context | USB drop / reboot under multi-band load | Dedicated radio worker task; non-blocking job queue |
| No true multi-priority GrokLink control | Competes with Flipper services unpredictably | Explicit priorities: ISR > radio > safety > agent > rpc > shell |
| Shared locks ad-hoc | Radio mutex bolted on late | Kernel mutex/queue + **resource arbiter** for multi-radio |

---

## 4. Radio / driver model

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| Thin facade over Flipper HAL | `gl_hw_*` / workers not fully owned | First-party drivers with async completion callbacks |
| Spectrum multi-band **hard-disabled** on device | Boot-loop mitigation | Controlled multi-band **planner** with settle, duty limit, circuit breaker |
| RX wall-clock caps (≤1–5 s) | Safety + stability | Caps remain (safety) but async buffering allows longer *mission* campaigns via chunked jobs |
| TX often “ack file” not full encode | Honest but incomplete | Pluggable encoder path; still confirm-gated |
| Little multi-radio concurrency | One domain at a time | Arbiter allows e.g. IR + GPIO events while SubGHz RX if RF front-end free |
| BLE status channel planned only | USB exclusive ops pain | Native BLE status/data channel service |

---

## 5. Storage

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| SD-only bulk data via Furi storage | `/ext/groklink` | Own FS adapter: SD + optional internal flash partitions |
| Append JSONL without journal | Crash mid-write risk | Crash-safe log segments + integrity hash chain |
| Private vault designed lightly | Docs only / soft crypto | Vault service with basic encryption hooks (key from operator secret) |
| Blacklist fail paths limited | Reload on init | Continuous integrity check; fail-closed degraded mode |

---

## 6. Agent & skills

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| Thin deterministic state machine | Rules + timers + thresholds only | Rich mission IR: seq, if, loop, timeout, parallel, resource reserve |
| Skills = JSON + optional FAP stub | FAP still Flipper runtime | Native skill packages: manifest + rules + optional bytecode/WASM-lite or native .so/.bin |
| Hot-reload fragile | Index rebuild | Versioned skill registry with refcount + signature verify hook |
| Offline autonomy limited | Schedule windows basic | Mission windows + RTC wake + resume tokens + sync-on-reconnect |

---

## 7. ML / intelligence

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| Explicit **no on-device LLM** | Correct for silicon | Unchanged — no LLM on device |
| ML stub / histogram features only | `gl_features` + weak infer | TinyML path: feature extractors + optional CMSIS-NN / TFLite Micro (≤20–40 KB models) |
| Model cannot act | Good | **Preserved & strengthened**: model outputs are *observations* only; policy engine alone may open hardware |

---

## 8. RPC & PC bridge

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| JSON over CLI/VCP primary | Framing `GROKRPC:…` | Binary framed protobuf sessions + optional JSON debug |
| Limited streaming / back-pressure | Long ops block | Stream channels with credits / windowing |
| Large transfers awkward | Captures via SD mostly | Chunked bulk transfer + resume |
| Bridge solid but overlay-era | Python package mature | **groklink-os** package: async client, CLI, craft, stream |

---

## 9. Power

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| Stock Flipper power policy | Not mission-aware | Mission-aware power manager: deep sleep, radio duty-cycle, RTC wake, battery profiles |

---

## 10. Safety (preserve & strengthen)

v2 already has strong design: default-deny, confirm tokens, blacklists, edu ack,
audit hash chain, risk classes. **None of these are relaxed.**

OS 3.0 adds:

- Policy checks **inside driver call path**, not only service wrappers.
- Cryptographic audit chaining (BLAKE2s/SHA-256 truncated) with export tooling.
- Signed skill packages (optional enforce).
- Explicit degraded modes matrix (no SD, corrupt BL, OOM, radio fault).
- Rate limits and duty cycle enforced in kernel resource layer.

---

## 11. GUI / UX

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| CLI primary; UI FAP optional | Good for lab | Shell + minimal status UI port; full stock Flipper UI not required on research images |

---

## 12. Build / test / release

| Limitation (v2) | Detail | OS 3.0 response |
|-----------------|--------|-----------------|
| Overlay scripts into fbt | Fragile | CMake multi-platform: `HOST` sim + `STM32WB55` |
| Limited unit tests on device C | Bridge well tested | Host unit tests for kernel, safety, agent, mission IR |
| DFU of full Momentum image | Large, mixed | Native DFU/bin artifacts labeled GrokLink OS |

---

## Summary: what “expansion” means

GrokLink OS does **not** claim infinite RAM or unrestricted TX. It claims:

1. **Ownership** of the software stack (no overlay tax).
2. **Better use** of the same silicon via scheduling, pooling, async radio.
3. **Richer autonomy** and optional tiny ML under the same safety soul.
4. **Production structure** (kernel/services/drivers/bridge/tests/docs) that
   can grow to higher-resource MCUs without rewrite.

See [ARCHITECTURE.md](ARCHITECTURE.md) and [RESOURCE_BUDGET.md](RESOURCE_BUDGET.md).
