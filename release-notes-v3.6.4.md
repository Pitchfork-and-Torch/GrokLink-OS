# GrokLink OS v3.6.4

From-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, on-device GUI, and PC bridge (authorized educational use only).

## Install (DFU)

1. Enter DFU: hold BACK + OK, plug USB.
2. Flash GrokLink-OS-v3.6.4-radio.dfu via qFlipper-cli or flash_os_dfu_only.ps1
3. COM port 230400, JSON GrokRPC.

## USB

Continuous usbd_poll during CDC bind and service init (fixes WriteFile 0-byte / dead COM after enum). Early ping/status while services boot.

MIT License.
