# DFU build and flash — GrokLink OS

## Profiles (`tools/build_dfu.ps1`)

| Profile | Artifact | Notes |
|---------|----------|--------|
| **OsRadio** (recommended lab) | `GrokLink-OS-v3.4.0-radio.dfu` | USB CDC + RPC + CC1101 + GUI |
| OsCdc | `GrokLink-OS-v3.4.0-rpc.dfu` | USB CDC + RPC + GUI, sim radio |
| Cdc | `GrokLink-OS-v3.0.1-cdc.dfu` | Early CDC bring-up |
| Bringup | `GrokLink-OS-v3.0.0-bringup.dfu` | GPIO-only, no USB |

## Build

Requires `arm-none-eabi-gcc` (ufbt toolchain is used automatically if present):

```powershell
.\tools\build_dfu.ps1 -Profile OsRadio
```

Inspect DFU:

```powershell
py -3 tools\bin2dfu.py --inspect dist\dfu\GrokLink-OS-v3.4.0-radio.dfu
```

## Flash (qFlipper)

1. Have recovery DFU ready (e.g. GrokLink-Firmware v2.1.3 or stock/Momentum full image).
2. Close PC bridge / serial tools.
3. Enter DFU: unplug → hold **BACK + OK ~30 s** → plug USB → **DFU in FS Mode** (`0483:DF11`).
4. Flash:

```powershell
.\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\GrokLink-OS-v3.4.0-radio.dfu
```

Or: qFlipper → **Install from file**.

5. After reboot, open serial **230400** and send JSON (see README).

## Recovery

If the unit seems dead:

1. Hold **BACK + OK ~30 s**, connect USB → DFU.
2. Install a **full** known-good DFU (not a minimal bring-up image):

```powershell
.\tools\recover_flipper.ps1 -DfuPath path\to\recovery.dfu
```

## Tools

| Tool | Role |
|------|------|
| `tools/bin2dfu.py` | `.bin` → DfuSe `.dfu` |
| `tools/build_dfu.ps1` | Compile + pack profiles |
| `tools/flash_os_dfu_only.ps1` | Wait for DFU, flash OS image |
| `tools/recover_flipper.ps1` | Flash recovery/full firmware |
| `platform/stm32wb55/` | BSP, USB, SPI radio, display |
