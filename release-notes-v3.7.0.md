# GrokLink OS v3.7.0

From-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, on-device GUI, and PC bridge (authorized educational use only).

## Install

1. Enter DFU: hold BACK + OK, plug USB (DFU in FS Mode, 0483:DF11).
2. Flash:

```text
qFlipper-cli firmware GrokLink-OS-v3.7.0-radio.dfu
```

Or from a clone:

```text
.\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.7.0-radio.dfu
```

3. Open USB serial (0483:5740, product GrokLink OS) at 230400. Use GrokRPC JSON. qFlipper protobuf errors after flash are expected.

```text
groklink-os ping
groklink-os edu-ack
groklink-os status
```

MIT License.
