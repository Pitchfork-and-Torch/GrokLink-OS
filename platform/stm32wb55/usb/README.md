# USB device (Phase P1)

Integrate **libusb_stm32** (Apache-2.0) for STM32WB55:

- Driver: `usbd_stm32wb55_devfs.c`
- Core: `usbd_core.c`
- App: CDC ACM → COM port on Windows (`0483:5740` typical)

## Critical: 48 MHz USB clock + CRS

WB55 USB FS needs 48 MHz within ±0.25%. `glk_usb_rcc.c` enables **HSI48** and
**CRS** (auto-trim from USB SOF). Without accurate CLK48 the host reports
Device Descriptor Request Failed (`VID_0000`).

Also: skip BCD in `usbd_stm32wb55_devfs.c` connect (pull-up only), USB-first
boot in `main_os_cdc.c` (D+ before SPI/radio), and re-assert PA11/PA12 AF after
board GPIO init.

## Steps

1. Vendor `third_party/libusb_stm32` (or submodule).  
2. Provide `stm32.h` or PLATFORMIO-style `STM32WBxx` + CMSIS include path from
   Flipper `lib/stm32wb_cmsis` / ufbt SDK headers.  
3. Implement `glk_usb_cdc_init/poll/read/write` in `glk_usb_cdc.c`.  
4. Link `apps/device/main_cdc.c` instead of GPIO-only bring-up main.  
5. `.\tools\build_dfu.ps1 -Profile Cdc` → flash → `.\tools\device_probe.ps1`.  

Reference demo: upstream `libusb_stm32/demo/cdc_loop.c` (no HID for first cut).
