# On-device GUI (v3.1)

GrokLink OS drives the Flipper F7 **ST7567 128×64** panel over SPI_D pins
(bit-bang) and samples the five-way pad.

## Pages (Left / Right)

| Page | Content |
|------|---------|
| HOME | Version, edu state, USB ready |
| RADIO | Probe / RX activity line |
| SAFETY | Default-deny, confirm, audit, no auto-TX |
| ABOUT | Native RTOS branding |

## Pins (hardware facts only)

| Function | Pin |
|----------|-----|
| Display CS | PC11 |
| Display RST | PB0 |
| Display A0/DI | PB1 |
| SPI_D SCK | PD1 |
| SPI_D MOSI | PB15 |
| Up / Down / Left / Right / OK / Back | PB10 / PC6 / PB11 / PB12 / PH3 / PC13 |

## Build

Included in `OsCdc` and `OsRadio` profiles via `tools/build_dfu.ps1`.

## Notes

- GUI poll runs in the main loop next to USB (never inside endpoint callbacks).
- TX remains policy-gated; the GUI never issues RF TX.
- Panel orientation: ST7567 `SEG normal (0xA0)` + `COM reverse (0xC8)` for right-side-up on F7 (v3.1.5+ / 3.2).
