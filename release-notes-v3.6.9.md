# GrokLink OS v3.6.9

From-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, on-device GUI, and PC bridge (authorized educational use only).

## Install (DFU)

1. BACK+OK, plug USB (DFU 0483:DF11)
2. Flash GrokLink-OS-v3.6.9-radio.dfu
3. COM @ 230400, JSON GrokRPC

## Fix (from bisect)

bisect-link and bisect-init both pass ping. Root cause: early boot field SPI while host binds CDC. v3.6.9: USB-first + services like bisect-init; never auto-arm field after host DTR; SPI agent only when unplugged.

MIT License.
