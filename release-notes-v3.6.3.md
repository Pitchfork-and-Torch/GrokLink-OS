# GrokLink OS v3.6.3

From-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, on-device GUI, and PC bridge (authorized educational use only).

## Install (DFU)

1. Enter DFU: hold BACK + OK, plug USB (DFU in FS Mode, 0483:DF11).
2. Flash:

```
qFlipper-cli firmware GrokLink-OS-v3.6.3-radio.dfu
```

Or: `.\tools\flash_os_dfu_only.ps1`

3. After reboot: COM port 230400, JSON GrokRPC. qFlipper protobuf errors after flash are expected.

## USB

v3.6.2 fixed device descriptor enum (0483:5740). v3.6.3 delays field SPI and display bring-up so Windows can finish CDC bind and SetCommState/GrokRPC.

MIT License.
