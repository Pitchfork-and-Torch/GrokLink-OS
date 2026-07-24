# GrokLink OS 3.0 Architecture (“GrokLink Native”)

> **This is a from-scratch research operating system**, not an overlay, fork, or
> patch on official Flipper Zero / Momentum / Unleashed firmware.

Target primary MCU: **STM32WB55** (Cortex-M4 application + Cortex-M0+ network).
Portability: HAL + BSP boundary so higher-resource MCUs can host the same services.

---

## 1. Design principles

1. **Safety by construction** — every actuator path through the policy engine.
2. **Determinism under load** — pools, budgets, bounded queues; fail closed.
3. **Constrained autonomy** — offline missions under policy; brain still PC-side for heavy analysis.
4. **Audit everything** — append-only, hash-chained, exportable.
5. **Modular skills** — hot-load, risk-classed, optionally signed.
6. **Honest silicon** — expand software capability without lying about WB55 limits.

---

## 2. Layer cake

```
+------------------------------------------------------------------+
|  LLM tools + optional localhost observe API (:8741)              |
|  bridge/groklink_os  (RPC, observe packager, craft, store)       |
+-------------------------------+----------------------------------+
                                | USB CDC / TCP host-sim (GrokRPC)
                                v
+------------------------------------------------------------------+
|  Applications                                                    |
|  shell · ST7567 status UI · factory diagnostics                  |
+------------------------------------------------------------------+
|  Services                                                        |
|  safety/policy · agent · skill host · rpc · audit · storage      |
|  power · ml_infer (optional) · radio_arbiter                     |
+------------------------------------------------------------------+
|  Drivers (async)                                                 |
|  subghz · nfc · ir · gpio · ibutton · uart/spi/i2c · ble         |
+------------------------------------------------------------------+
|  HAL (board-portable)                                            |
|  clock · gpio · dma · timer · usb · flash · sdmmc · radio phy    |
+------------------------------------------------------------------+
|  Kernel (GrokLink RTOS)                                          |
|  tasks · mutex · queue · event · timer · mempool · tick · power  |
+------------------------------------------------------------------+
|  Platform BSP                                                    |
|  host sim  |  stm32wb55 board  |  (future MCU)                   |
+------------------------------------------------------------------+
```

---

## 3. Kernel (GrokLink RTOS)

Lightweight preemptive RTOS optimized for dual-core WB55:

| Primitive | Purpose |
|-----------|---------|
| Tasks | Priority 0..31, fixed stacks from pools |
| Mutex | Priority inheritance (bounded) |
| Queue | Fixed-size message queues |
| Event flags | Multi-wait wakeups |
| Software timers | One-shot / periodic |
| Memory pools | Deterministic block alloc |
| Heap | Optional TLSF-style or simple first-fit with budgets |
| Tick | 1 ms default; RTC for deep sleep wake |

### Priority map (default)

| Prio | Role |
|------|------|
| 0–1 | Idle / shell |
| 2–3 | Logging / audit flush |
| 4–6 | Agent mission engine |
| 7–8 | RPC session |
| 9–11 | Storage / FS |
| 12–14 | Driver completion |
| 15–18 | Radio worker / arbiter |
| 19–22 | Safety critical timers |
| 23–31 | Reserved / ISR bottom-halves |

### Dual-core

- **M4**: kernel, drivers, agent, RPC, policy.
- **M0+**: vendor BLE wireless stack (binary) via IPCC mailbox abstraction
  (`glk_ipc`). App never bypasses BLE safety wrappers.

---

## 4. Safety & policy engine

Non-negotiable service; linked into the **driver path**:

```
caller (RPC | agent | shell)
  -> glk_policy_check(request)
       -> edu session? risk class? blacklist? duty? confirm? rate?
       -> decision + audit record
  -> driver op only if Allow
```

Risk classes (compatible with v2 concepts):

`info` · `passive_rx` · `active_tx` · `gpio` · `contact` · `system`

See [SAFETY.md](SAFETY.md).

---

## 5. GrokAgent (native)

Event-driven multi-task agent:

- **Mission IR** — compiled from JSON (v2 compatible) to compact bytecode.
- **Opcodes** — `NOP, SLEEP, RX, TX, GPIO_*, IR_*, NFC_*, LOG, IF, LOOP, PARALLEL_BEGIN/END, RESERVE, RELEASE, INFER, ABORT`.
- **Watchdog** — mission wall-clock + step timeout.
- **Offline** — RTC schedule, resume token on SD, sync on RPC reconnect.
- **ML** — optional `INFER` produces features/labels into mission vars; **never** opens TX/GPIO without separate policy-approved steps.

---

## 6. Skill system

```
/sd/groklink/skills/<id>/
  manifest.json     # id, version, risk_class, entry, signature?
  rules.json        # decision rules / mission snippets
  protocol.json     # PC decoder hints
  module.bin        # optional native/bytecode (signed)
  README.md
```

Runtime:

1. Scan / verify / register.
2. Map risk class → policy ceiling.
3. Load rules into agent; optional sandbox for bytecode.
4. Hot-unload when refcount 0.

---

## 7. Drivers & radio

Async job model:

```
submit(job) -> queue -> radio_worker
  acquire(resource)
  program PHY
  DMA/IRQ capture
  complete(cb) / stream chunks
  release + duty accounting
```

**Arbiter** serializes exclusive resources (SubGHz frontend) and allows
compatible concurrent domains (e.g. GPIO IRQ + IR RX).

Multi-band spectrum: **planner** issues sequential jobs with settle gaps and
global circuit breaker — never a tight multi-band loop on the RPC thread.

---

## 8. Storage

- SD card primary (FatFs or host FS shim).
- Optional internal flash: factory config, secure boot keys, small vault.
- Journaled log segments: `logs/audit-NNNN.seg` + hash chain root in `logs/audit.chain`.
- Crash-safe mission state: `state/mission_<id>.resume`.

---

## 9. GrokRPC v3

- Framing: magic `GL` (0x47 0x4C), version, flags, length, payload, CRC32.
- Payload: protobuf (`schemas/groklink_os.proto`) or JSON debug mode.
- Sessions: edu ack, capabilities, stream subscribe, bulk transfer.
- Transports: USB CDC, BLE GATT (status + data), host TCP sim.

---

## 10. Power

Policies: `run`, `listen_duty`, `mission_idle`, `deep_sleep`.

Wake sources: RTC alarm, USB plug, GPIO IRQ (policy-limited), BLE wake.

Battery profiling hooks feed agent scheduler (defer non-critical missions).

---

## 11. Secure boot & signing (hooks)

- Image header with hash + signature slot.
- Skill package Ed25519/secp256r1 signature field (optional enforce via Kconfig).
- Debug unlock requires physical confirm + audit.

---

## 12. Host simulation

`platform/host` builds the same services on Windows/Linux for:

- Unit/integration tests without hardware.
- RPC bridge development (`tcp://127.0.0.1:7341` or serial sim).
- Mission IR and safety regression.

---

## 13. Migration from v2 overlay

Concepts preserved: missions JSON, skill manifests, risk classes, confirm tokens,
edu phrase, SD layout `/groklink/...`, skill craft loop.

See [MIGRATION.md](MIGRATION.md).

---

## 14. SubGHz / CC1101 (F7 board)

Native stack (not Furi):

```
glk_radio worker
  -> glk_hal_subghz (path SW0 + RX window)
      -> glk_cc1101 (TI register map)
          -> SPI_R board glue (PA5/PB4/PB5, CS PD0, GDO0 PA1)
```

See [BOARD_F7_CC1101.md](BOARD_F7_CC1101.md).

## 15. Why not keep the overlay?

Field evidence (boot loops under multi-band RX, 1 Hz agent, Furi coupling,
flash budget fights) shows the **overlay tax** is the bottleneck—not only
silicon. Native ownership of scheduling, radio workers, and memory is the
only path to expand capability while keeping fail-closed safety.
