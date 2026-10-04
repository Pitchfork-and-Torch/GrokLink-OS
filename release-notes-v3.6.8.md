# GrokLink OS v3.6.8

From-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, on-device GUI, and PC bridge (authorized educational use only).

## Install (DFU)

1. BACK+OK, plug USB (DFU 0483:DF11)
2. Flash GrokLink-OS-v3.6.8-radio.dfu
3. COM @ 230400, JSON GrokRPC

## Fix

USB-first boot (identical order to proven 3.0.1-cdc), pure poll settle, then services with poll between steps; light RPC in bulk RX callback.

MIT License.
