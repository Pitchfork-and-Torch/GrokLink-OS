# Device application images

| Profile | Entry | DFU name | Purpose |
|---------|-------|----------|---------|
| bringup | `platform/stm32wb55/bringup/` | `*-bringup.dfu` | Boot proof, no USB |
| cdc (P1) | `apps/device/main_cdc.c` (planned) | `*-cdc.dfu` | USB serial + RPC ping |
| full (P3+) | `apps/device/main_os.c` (planned) | `*-os.dfu` | Agent + radio + storage |

Build bring-up today:

```powershell
.\tools\build_dfu.ps1
```

See `docs/ROADMAP_NATIVE.md`.
