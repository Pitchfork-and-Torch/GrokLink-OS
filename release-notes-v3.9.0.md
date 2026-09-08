# GrokLink OS v3.9.0 Field Card

Host-proven card filesystem, honest device SD probe, BLE skeleton, Bench Desktop, USB soak, host CI.

## What shipped

| Track | Status |
|-------|--------|
| GLKFS remount on a file-backed card image | Host/CI green |
| Device SD probe | Compiled. Absent without a card. ROM-passive still works |
| Ed25519 skill line on host | `GLKSIG1-ED25519`. Device keeps FNV GLKSIG1 |
| BLE | `ble_status` skeleton. Not a phone app |
| Bench Desktop | Observe-only tk window. Do not launch from a Grok TUI job |
| USB soak | `tools/usb_soak.ps1` |
| CI | GitHub Actions host-tests |

## Honesty

- Device littlefs-class FS is **GLKFS**. Host remount is the proof. Hardware DFU + real SD ACK is operator-gated.
- BLE is an M0+ IPCC **skeleton**, not GATT, not Grok Bot Watch.
- Ed25519 verify is host/CI. Not a secure-boot substitute. No private keys in the tree.
- Observation tools never TX.

## Operator

```powershell
# Host
cmake -S . -B build-host ; cmake --build build-host --config Release
ctest --test-dir build-host -C Release --output-on-failure
py -3 -m pytest bridge/tests -q

# Device (human-gated)
powershell -File tools\build_dfu.ps1 -Profile OsRadio
powershell -File tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.9.0-radio.dfu
powershell -File tools\usb_soak.ps1
groklink-os edu-ack
groklink-os storage-status
groklink-os ble-status
```

Education phrase: `I_WILL_USE_ONLY_AUTHORIZED_TARGETS`
