GrokLink OS DFU pack
====================

Latest device image (OsRadio / CC1101 SPI):
  GrokLink-OS-v3.8.0-radio.dfu   <- preferred flash file
  GrokLink-OS-v3.8.0-radio.bin
  GrokLink-OS-v3.8.0-radio.hex
  GrokLink-OS-v3.8.0-radio.json

Flash:
  1. Hold BACK + OK, plug USB (DFU in FS Mode, 0483:DF11)
  2. .\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.8.0-radio.dfu
  3. Or: qFlipper-cli firmware GrokLink-OS-v3.8.0-radio.dfu

Post-flash USB: 0483:5740 product "GrokLink OS" @ 230400 baud.

Recover stock Flipper:
  .\tools\recover_flipper.ps1

Authorized research only. Not a medical device.
