# Flash GrokLink OS (human-gated)

v3.9.1 is a **host/CI honesty** cut. It does **not** recut DFU. No OTA.

Last packaged OsRadio in this tree:

- **GrokLink-OS-v3.8.0-radio.dfu** (preferred existing pack)
- `.bin` / `.hex` also in `dist/dfu/`

v3.9.0 named `GrokLink-OS-v3.9.0-radio.dfu` but did not attach that file. Do not flash a path that is not on disk. Field Card on silicon needs a human `tools/build_dfu.ps1 -Profile OsRadio` then flash.

Also listed on the [latest GitHub Release](https://github.com/Pitchfork-and-Torch/GrokLink-OS/releases/latest) when assets are attached.

## Enter DFU

1. Unplug the device
2. Hold **BACK + OK**
3. Plug USB while holding - look for **DFU in FS Mode** (`0483:DF11`)

## Flash

```powershell
.\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.8.0-radio.dfu
```

Or:

```text
qFlipper-cli firmware GrokLink-OS-v3.8.0-radio.dfu
```

Post-flash qFlipper protobuf / exit-recovery errors are **expected** (not Flipper OS).

## After flash

Expect **USB Serial** `0483:5740`, product **GrokLink OS**, **230400** baud.

```powershell
$env:GLK_SERIAL_PORT = "COMx"
cd bridge
pip install -e ".[serial]"
groklink-os ping
groklink-os edu-ack
groklink-os status
groklink-os storage-status
groklink-os ble-status
```

Host-sim (no device) reports bridge **3.9.1**. Flashed silicon reports whatever image was last burned.

## Recover stock Flipper

```powershell
.\tools\recover_flipper.ps1
```

Authorized research only.
