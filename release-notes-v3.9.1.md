# GrokLink OS v3.9.1 host/CI honesty

Public cut is **host/CI honesty**. Device DFU is **not recut**. No OTA.

Last packaged OsRadio in this tree: `dist/dfu/GrokLink-OS-v3.8.0-radio.dfu`.
v3.9.0 named a `v3.9.0-radio.dfu` path but did not attach an asset. Flashed silicon stays whatever the operator last burned until a human-gated build+flash.

## What shipped (host/CI)

| Track | Status |
|-------|--------|
| GLKFS file CRC on read | Fail-closed `CORRUPT`. Does not reformat |
| GLKFS table CRC fail | Stays corrupt. Empty host images may format. Physical cards never auto-format |
| Streaming append | No 2048-byte RAM cap. Rewrite reuses an extent when it fits |
| Honest SD probe | `posix` / `host_image` / `host_sim` / `no_card` / `no_spi` / `present` |
| Host `init_device` | `NOSUPPORT` + `host_sim`. Never a fake RAM disk |
| BLE | `tx` / `advertising` / `ipcc` false. `glk_ble_tick` is a no-op |
| PC bridge | Skip USB banners. First complete JSON object only |
| Bench | Fail-closed TCP unless a COM port is explicit. Does not hang a TUI |
| Lab codec tests | Truncated / CRC mismatch / invalid hex / oversize. GLK1 only |

## Honesty

- This tag is host tests, bridge, docs, and landing. Not a new device image.
- BLE is still an M0+ IPCC skeleton. Not GATT. Not a phone app.
- Observation tools never TX.
- Authorized research only.

## Operator

```powershell
# Host
cmake -S . -B build-host ; cmake --build build-host --config Release
ctest --test-dir build-host -C Release --output-on-failure
py -3 -m pytest bridge/tests -q

# Device (human-gated; not part of this cut)
powershell -File tools\build_dfu.ps1 -Profile OsRadio
powershell -File tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.8.0-radio.dfu
powershell -File tools\usb_soak.ps1
groklink-os edu-ack
groklink-os storage-status
groklink-os ble-status
```

Education phrase: `I_WILL_USE_ONLY_AUTHORIZED_TARGETS`
