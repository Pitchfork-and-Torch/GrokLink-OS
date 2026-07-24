# Build & Flash — GrokLink OS 3.0

## Host simulation (primary development)

### Prerequisites

- CMake ≥ 3.16
- C11 compiler (MSVC, Clang, or GCC)
- Python 3.10+ for the bridge

### Configure & build (Windows)

```powershell
cd path\to\GrokLink-OS
cmake -B build -DGLK_PLATFORM_HOST=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Run

```powershell
$env:GLK_SD_ROOT = "$PWD\sd_card\groklink"
$env:GLK_RPC_PORT = "7341"
.\build\Release\groklink_os.exe
```

Environment:

| Variable | Meaning |
|----------|---------|
| `GLK_SD_ROOT` | Path to groklink SD tree |
| `GLK_RPC_PORT` | TCP port (default 7341) |
| `GLK_RUN_MS` | Auto-stop after N ms (tests) |

## STM32WB55 target

### Toolchain

- `arm-none-eabi-gcc` 12+ or 13+
- OpenOCD / STM32CubeProgrammer / DFU tools
- Optional: ninja

### CMake sketch

```bash
cmake -B build-wb55 \
  -DGLK_PLATFORM_HOST=OFF \
  -DCMAKE_TOOLCHAIN_FILE=platform/stm32wb55/cmake/arm-none-eabi.cmake
cmake --build build-wb55
```

Board support lives under `platform/stm32wb55/`:

- `board/` — clocks, pins, CC1101 SPI, USB, SDMMC
- `cmsis/` — device headers (vendor pack)
- `linker/` — flash/RAM scatter

### Flash

1. Enter DFU (bootloader buttons per board).
2. Flash `groklink_os.dfu` / `.bin` via CubeProgrammer or dfu-util.
3. Copy `sd_card/groklink/` to SD card root as `/groklink/` (or board mount path).

### Secure boot hooks

Image header slots are defined for hash + signature. Enable
`GLK_FEATURE_SECURE_BOOT_HOOKS` and wire keys for release builds. Development
images may run unsigned.

## Reproducible release

```text
VERSION file → CMake project version
CHANGELOG.md entry
ctest green on host
bridge package version aligned
SD seed packaged as release asset
```

## Troubleshooting

| Symptom | Action |
|---------|--------|
| RPC bind fail | Change `GLK_RPC_PORT`; ensure nothing else on 7341 |
| Degraded / no missions | Check `GLK_SD_ROOT` points at `sd_card/groklink` |
| TX always denied | `edu-ack` + `confirm-issue` + valid file path |
| Radio breaker open | Power-cycle session / clear faults after settle |
