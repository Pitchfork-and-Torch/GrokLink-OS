# Board profile: Flipper Zero F7 (STM32WB55 + CC1101)

GrokLink OS targets electrical compatibility with Flipper Zero **F7** hardware
so a native image can drive the same SubGHz front-end without Furi.

## Pin map (SubGHz)

| Function | Port/Pin | Notes |
|----------|----------|--------|
| CC1101 CS | **PD0** | Active low |
| CC1101 GDO0 | **PA1** | Async serial / edge capture |
| RF path SW0 | **PC4** | Antenna matching switch |
| SPI_R SCK | **PA5** | SPI1 |
| SPI_R MISO | **PB4** | SPI1 |
| SPI_R MOSI | **PB5** | SPI1 |

Source of pin numbers: open F7 `furi_hal_resources` definitions (hardware facts only).

## RF path selection

| Band | Path enum | SW0 |
|------|-----------|-----|
| Isolate | `GLK_RF_PATH_ISOLATE` | 0 |
| ~315 MHz | `GLK_RF_PATH_315` | 1 |
| ~433 MHz | `GLK_RF_PATH_433` | 0 |
| ~868/915 MHz | `GLK_RF_PATH_868` | 1 |

`glk_hal_subghz_set_frequency_and_path()` picks the path from frequency, then
programs the CC1101 PLL (`f * 2^16 / 26 MHz`).

## Software stack

```
glk_radio (async worker + policy)
  -> glk_hal_subghz (path + RX window)
      -> glk_cc1101 (registers / strobes)
          -> glk_board_spi_r_* (SPI1 + GPIO)
```

## Host vs target

| Build | Behavior |
|-------|----------|
| `GLK_PLATFORM_HOST` | Deterministic pulse sim (CI / bridge dev) |
| `GLK_PLATFORM_STM32` | Real SPI + GDO0 edges when board glue is linked |

## Safety

Policy still wraps every RX/TX. This HAL never bypasses confirms or blacklists.
TX carrier helper is for gated, authorized lab use only.

## Bring-up checklist

1. Board SPI glue in `platform/stm32wb55/hal/glk_board_spi_r.c` (bit-bang SPI_R) ✅  
2. Build: `.\tools\build_dfu.ps1 -Profile OsRadio` → `GrokLink-OS-v3.4.0-radio.dfu`  
3. DFU entry (lab): unplug → hold **BACK+OK ~30 s** → plug USB  
4. Probe: `{"cmd":"subghz_probe"}` → VERSION≈**20** (`0x14`) on real CC1101 ✅  
5. Passive RX → `"sim":false`  
6. GUI pages via Left/Right (see [GUI.md](GUI.md))  

SPI notes: bounded CHIP_RDYn wait; RPC deferred out of USB endpoint callbacks.

## Build flags

| Profile | Define | Radio path |
|---------|--------|------------|
| OsCdc | `GLK_RADIO_SIM=1` | Software pulse sim + GUI |
| OsRadio | `GLK_BUILD_RADIO=1` | Real CC1101 + GUI |

