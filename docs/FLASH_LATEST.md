# Flash GrokLink OS v3.7.0 (Flipper F7 class)

## Download

From the [latest GitHub Release](https://github.com/Pitchfork-and-Torch/GrokLink-OS/releases/latest):

- **GrokLink-OS-v3.7.0-radio.dfu** (preferred)
- `.bin` / `.hex` also attached

## Enter DFU

1. Unplug the device  
2. Hold **BACK + OK**  
3. Plug USB while holding — look for **DFU in FS Mode** (`0483:DF11`)

## Flash

```powershell
.\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.7.0-radio.dfu
```

Or:

```text
qFlipper-cli firmware GrokLink-OS-v3.7.0-radio.dfu
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
groklink-os observe-rx --freq 433920000 --ms 200
```

## Recover stock Flipper

```powershell
.\tools\recover_flipper.ps1
```

Authorized research only.
