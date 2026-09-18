# GrokLink OS v3.6.5

From-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, on-device GUI, and PC bridge (authorized educational use only).

## Install (DFU)

1. BACK+OK, plug USB (DFU 0483:DF11)
2. Flash GrokLink-OS-v3.6.5-radio.dfu
3. COM @ 230400, JSON GrokRPC

## Fix

Deferred RPC now kicks USB bulk IN (tx_kick) so ping/status replies leave the device.

MIT License.
