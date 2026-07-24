# Resource Budget — GrokLink OS 3.0 (STM32WB55)

Budgets are **targets** for a research image (minimal GUI). Host sim ignores
flash/RAM silicon caps but enforces logical pool limits for determinism tests.

## Silicon (Flipper Zero class)

| Resource | Device | Notes |
|----------|--------|-------|
| Flash | 1 MB | BLE stack + options consume a slice |
| SRAM | 256 KB | Shared; app usable ~100–180 KB depending on stack config |
| CPU | 64 MHz M4 | Network co-processor separate |
| Radio | CC1101 (SubGHz) + NFC + IR + BLE | External buses |

## Flash layout (research image, illustrative)

| Region | Size (approx) | Contents |
|--------|---------------|----------|
| Bootloader + secure header | 32–48 KB | DFU, verify hooks |
| Kernel + HAL | 48–80 KB | RTOS + clocks/gpio/dma |
| Drivers | 60–100 KB | subghz, nfc, ir, gpio, ble shim |
| Services (safety, agent, rpc, storage, audit) | 80–140 KB | Core product |
| Shell / minimal UI | 20–40 KB | Optional |
| TinyML (optional) | 20–40 KB | CMSIS-NN / interpreter |
| Free / skills in flash | remainder | Prefer SD for skills |
| **Total app image goal** | **~350–500 KB** | Leaves headroom vs 1 MB |

Compared to v2 overlay **+40–60 KB** on top of full stock firmware, native
images **replace** stock app suite rather than stacking.

## RAM pools (application SRAM)

| Pool | Size | Purpose |
|------|------|---------|
| Kernel TCB / stacks | 24–32 KB | 8–12 tasks typical |
| Safety + blacklist | 4 KB | State + confirm slots |
| Agent mission IR + vars | 16–24 KB | Richer missions |
| Agent general arena | 32–48 KB | vs v2 ~12 KB heap |
| RPC RX/TX buffers | 8–16 KB | Streaming windows |
| Radio DMA / pulse buffers | 8–16 KB | Async capture |
| Storage cache | 4–8 KB | FS |
| Audit staging | 4 KB | Before SD flush |
| TinyML arena (optional) | 16–32 KB | Model activations |
| Heap (monitored) | 16–24 KB | Rare dynamic |
| **Reserve / idle** | rest | Fragmentation margin |

## Logical caps (defaults, `glk_config.h`)

| Symbol | v2 | OS 3.0 default |
|--------|----|----------------|
| Max missions | 16 | 32 |
| Max skills | 24 | 64 |
| Max mission steps | 32 | 64 |
| Confirm slots | 4 | 8 |
| TX max duration | 2000 ms | 2000 ms (unchanged safety) |
| TX cooldown | 5000 ms | 5000 ms |
| RX cooldown | 1500 ms | 500 ms (async worker safer) |
| RX max single job | 5000 ms | 5000 ms (chunked campaigns OK) |
| Audit line | 384 | 512 |
| ML model max | n/a | 40 KB |

## CPU / latency targets

| Path | Target |
|------|--------|
| Policy check | < 50 µs typical |
| Radio job submit → start | < 5 ms |
| RPC ping RTT (USB) | < 20 ms host-bound |
| Mission timer resolution | 1 ms |
| Deep sleep wake → agent | < 50 ms |

## Power targets (order-of-magnitude)

| Mode | Goal |
|------|------|
| Deep sleep | µA-class board-dependent |
| Listen duty 10% SubGHz | multi-hour portable |
| Active multi-radio | minutes; thermal/battery watch |

## What we refuse to budget

- On-device LLM weights.
- Unbounded capture buffers in RAM.
- TX without confirm + blacklist + audit.
- “Just disable safety in release.”
