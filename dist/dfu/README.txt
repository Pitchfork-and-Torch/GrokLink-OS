GrokLink OS 3.0.0 bring-up DFU
==============================

EXPERIMENTAL native image for STM32WB55 / Flipper Zero F7-class hardware.

Contents:
  GrokLink-OS-v3.0.0-bringup.dfu   <- qFlipper Install from file
  GrokLink-OS-v3.0.0-bringup.bin
  GrokLink-OS-v3.0.0-bringup.hex
  GrokLink-OS-v3.0.0-bringup.json

This image does NOT include:
  - Flipper GUI / dolphin OS
  - GrokAgent / GrokRPC services
  - SubGHz stack (CC1101 HAL is separate source; not linked here yet)

After flash you will lose stock Flipper UI until you reinstall a full DFU
(e.g. GrokLink-Firmware v2.1.3 or official/Momentum).

Recovery:
  1. Hold BACK, plug USB
  2. qFlipper -> Install from file -> known-good full .dfu

Flash:
  1. Close serial tools
  2. qFlipper -> Install from file -> this .dfu
  3. Do not unplug mid-flash
