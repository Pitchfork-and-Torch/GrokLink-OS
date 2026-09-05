# GrokLink OS v3.6.7

From-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, on-device GUI, and PC bridge (authorized educational use only).

## Install (DFU)

1. BACK+OK, plug USB (DFU 0483:DF11)
2. Flash GrokLink-OS-v3.6.7-radio.dfu
3. COM @ 230400, JSON GrokRPC

## Fix

Full OS USB path aligned with proven 3.0.1-cdc: connect then pure usbd_poll; light RPC (ping/status/edu_ack) answered in bulk RX callback; SPI/radio deferred; no soft-reconnect thrash.

MIT License.
