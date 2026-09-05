# GrokLink OS v3.6.6

From-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, on-device GUI, and PC bridge (authorized educational use only).

## Install (DFU)

1. BACK+OK, plug USB (DFU 0483:DF11)
2. Flash GrokLink-OS-v3.6.6-radio.dfu
3. COM @ 230400, JSON GrokRPC

## Fix

Light RPC (ping/status/edu_ack) answered in USB RX callback like CDC-only so bulk IN flushes on the same eprx. No nested usbd_poll in TX path.

MIT License.
