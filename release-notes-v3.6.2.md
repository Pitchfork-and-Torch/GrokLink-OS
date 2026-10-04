# GrokLink OS v3.6.2

From-scratch research RTOS for multi-radio portable hardware with gated agent autonomy, modular skills, on-device GUI, and PC bridge (authorized educational use only).

## Install (DFU)

1. Enter DFU: hold BACK + OK, plug USB (Windows shows DFU in FS Mode, 0483:DF11).
2. Flash:

```text
qFlipper-cli firmware GrokLink-OS-v3.6.2-radio.dfu
```

Or from a clone:

```text
.\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.6.2-radio.dfu
```

3. After reboot, open the new COM port at 230400 and send JSON GrokRPC (not Flipper protobuf). Post-flash qFlipper RPC errors are expected.

## USB CDC recovery (this release)

Hardened USB enumeration after DFU so hosts no longer stick on Device Descriptor Request Failed (VID_0000): skip broken BCD path, HSI48 CRS trim, USB-first boot before SPI/radio, soft reconnect, deferred field explore, bus-powered descriptor, short USB strings.

## Recover stock Flipper

```text
.\tools\recover_flipper.ps1
```

MIT License.
