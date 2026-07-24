# Flashing a Flipper Zero — GrokLink OS vs GrokLink Firmware

## Hard truth (read this first)

| Project | What it is | Flashable to Flipper? |
|---------|------------|------------------------|
| **GrokLink-OS 3.0 bring-up DFU** | Minimal native bare-metal image @ `0x08000000` | **Yes (experimental)** — see [DFU.md](DFU.md) |
| **GrokLink-OS 3.0 host** | Full services simulator (`groklink_os.exe`) | N/A (PC only) |
| **GrokLink-Firmware 2.1.x** | Full Momentum overlay with agent/RPC | **Yes (lab recommended)** |

### Bring-up DFU vs full lab image

- **`GrokLink-OS-v3.0.0-bringup.dfu`** — boots native code on STM32WB55; **no** Flipper UI, **no** GrokAgent/RPC yet. Use only if you accept reinstalling a full DFU afterward.  
- **`GrokLink-v2.1.3.dfu`** — complete lab firmware with `groklink` CLI and PC bridge.  

Build the OS DFU anytime with `.\tools\build_dfu.ps1` (see [DFU.md](DFU.md)).

---

## Lab use: force-update with GrokLink Firmware v2.1.3

For day-to-day research (agent, RPC, skills), use the **overlay** DFU:

### Files on this machine

```text
Desktop\GrokLink-qFlipper-install\GrokLink-v2.1.3.dfu
GrokLink-Firmware\dist\GrokLink-v2.1.3.dfu
```

Also on GitHub Releases for GrokLink-Firmware when published.

### Force install via qFlipper (recommended)

1. **Close** any PC bridge / serial tool holding the COM port.  
2. Open **qFlipper**.  
3. Connect Flipper USB.  
4. If the device is wedged / boot-looping:  
   - Hold **BACK** + plug USB (or use qFlipper “Repair” / DFU mode per current qFlipper UI).  
   - Wait until qFlipper sees the device in **DFU / update** state.  
5. **Install from file** → select `GrokLink-v2.1.3.dfu`.  
6. Wait for full flash + reboot (do not unplug).  
7. After boot, copy SD assets (see below).

This is a **full firmware replace** (Momentum base + GrokLink overlay services), not a partial FAP install.

### Force install if qFlipper fails (CLI recovery)

If you still have a working Momentum/Flipper tree with GrokLink overlay applied:

```powershell
cd path\to\momentum-or-flipper-tree
.\fbt.cmd flash_usb_full
```

Or use **STM32CubeProgrammer** / **dfu-util** only with a **known-good** full Flipper `.dfu` (stock, Momentum, or GrokLink-v2.1.3).  
Never flash the host `groklink_os.exe` or a partial object file.

### SD card (required for missions/skills)

Copy GrokLink-Firmware `sd_card/groklink/` → Flipper:

```text
/ext/groklink/config/
/ext/groklink/blacklist/
/ext/groklink/missions/
/ext/groklink/skills/
/ext/groklink/logs/
```

PowerShell helper (if present):

```powershell
Desktop\GrokLink-qFlipper-install\COPY-TO-FLIPPER-SD.ps1
```

### Verify on device

qFlipper → CLI tab:

```text
groklink status
```

PC:

```powershell
cd path\to\GrokLink-Firmware\bridge
py -3 -m pip install -e .
$env:GROKLINK_TRANSPORT = "cli"
$env:GROKLINK_PORT = "auto"
py -3 -m groklink.cli connect
py -3 -m groklink.cli status
```

Expect something like `groklink-2.1.3` / API 2 — **not** GrokLink OS `3.0.0`.

---

## Experimental: flash GrokLink OS **bring-up** DFU

```text
GrokLink-OS\dist\dfu\GrokLink-OS-v3.0.0-bringup.dfu
Desktop\GrokLink-qFlipper-install\GrokLink-OS-v3.0.0-bringup.dfu
```

1. Have **GrokLink-v2.1.3.dfu** ready for recovery.  
2. qFlipper → Install from file → bring-up DFU (DFU mode: hold BACK + USB).  
3. Device will not show stock UI.  
4. Restore lab stack by flashing **v2.1.3** again.  

Details: [DFU.md](DFU.md).

## Using GrokLink OS 3.0 **host** without flashing Flipper

Run the native stack on PC (develop missions/RPC/skills safely):

```powershell
cd path\to\GrokLink-OS
$env:GLK_SD_ROOT = "$PWD\sd_card\groklink"
$env:GLK_RPC_PORT = "7341"
.\build\Release\groklink_os.exe

# other terminal
cd bridge
py -3 -m pip install -e .
py -3 -m groklink_os.cli edu-ack
py -3 -m groklink_os.cli status
```

This is the supported “force update your workflow” path for OS 3.0 today.

---

## Roadmap: real Flipper flash of GrokLink OS

To produce a force-flashable native image, all of the following must land:

| Step | Status | Notes |
|------|--------|--------|
| 1. Host RTOS + services + RPC | Done | CI / bridge |
| 2. CC1101 driver + F7 pin map | Partial | Register path done; SPI/EXTI board glue stubs |
| 3. CMSIS startup + clocks + linker | Not done | STM32CubeWB pack |
| 4. USB CDC (or keep serial via existing bootloader path) | Not done | Need device stack |
| 5. SD card + storage | Partial | Host FS only |
| 6. Minimal UI or headless + LED | Not done | Optional for lab headless |
| 7. Preserve / coexist with option bytes & wireless stack | Critical | Wrong layout → DFU brick until recovery |
| 8. Package `.bin` + convert to Flipper-compatible `.dfu` | Not done | Match F7 full-image layout |
| 9. Lab flash on sacrificial unit | Not done | Then document force-update |

Until step 8 exists in Releases, **qFlipper “Install from file” for GrokLink OS is impossible**.

### Flipper DFU recovery (if something goes wrong)

1. Hold **BACK**, connect USB → DFU.  
2. qFlipper → install a known-good full firmware (official, Momentum, or GrokLink-v2.1.3).  
3. Do not leave the device mid-flash.

---

## Decision guide

| Goal | Action |
|------|--------|
| Newest **on-device** GrokLink on Flipper | Force-flash **GrokLink-v2.1.3.dfu** + SD seed |
| Exercise **OS 3.0** APIs now | Run **host** `groklink_os` + `groklink-os` CLI |
| True native OS on Flipper | Finish silicon bring-up (roadmap above); no shortcut DFU |

---

## Related docs

- [BUILD.md](BUILD.md) — host + future ARM cmake  
- [BOARD_F7_CC1101.md](BOARD_F7_CC1101.md) — pinout for native SubGHz  
- GrokLink-Firmware [BUILD_FLASH.md](https://github.com/Pitchfork-and-Torch/GrokLink-Firmware/blob/main/docs/BUILD_FLASH.md) — overlay build/flash  
