# Build GrokLink OS STM32WB55 DFU images
# Profiles:
#   Bringup — GPIO-only (no USB)
#   Cdc     — USB CDC + JSON ping/status (Phase P1)
#   OsCdc   — full RPC + policy + sim radio (Phase P2)
#   OsRadio — full RPC + real CC1101 SPI (Phase P3, no GLK_RADIO_SIM)
param(
  [ValidateSet("Bringup", "Cdc", "OsCdc", "OsRadio", "BisectLink", "BisectInit")]
  [string]$Profile = "OsCdc",
  [string]$GccPrefix = "",
  [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

if (-not $GccPrefix) {
  $cand = Join-Path $env:USERPROFILE ".ufbt\toolchain\x86_64-windows\bin"
  if (Test-Path (Join-Path $cand "arm-none-eabi-gcc.exe")) { $GccPrefix = $cand }
}
if (-not $GccPrefix) {
  $cmd = Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue
  if ($cmd) { $GccPrefix = Split-Path $cmd.Source -Parent }
}
if (-not $GccPrefix) { throw "arm-none-eabi-gcc not found" }

$env:PATH = "$GccPrefix;$env:PATH"
$gcc = Join-Path $GccPrefix "arm-none-eabi-gcc.exe"
$objcopy = Join-Path $GccPrefix "arm-none-eabi-objcopy.exe"
$size = Join-Path $GccPrefix "arm-none-eabi-size.exe"
Write-Host "Using GCC: $gcc"
Write-Host "Profile: $Profile"

if (-not $OutDir) { $OutDir = Join-Path $Root "dist\dfu" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$build = Join-Path $Root "build-wb55-$($Profile.ToLower())"
New-Item -ItemType Directory -Force -Path $build | Out-Null

$ld = Join-Path $Root "platform\stm32wb55\bringup\stm32wb55_bringup.ld"
$cmsis = Join-Path $env:USERPROFILE ".ufbt\current\sdk_headers\f7_sdk\lib"
if (-not (Test-Path (Join-Path $cmsis "cmsis_core\core_cm4.h"))) {
  $cmsisCandidates = @(
    (Join-Path $env:USERPROFILE "Flipper69\firmware\momentum\lib"),
    (Join-Path $env:USERPROFILE "flipper\firmware\momentum\lib")
  )
  $cmsis = $cmsisCandidates | Where-Object { Test-Path (Join-Path $_ "cmsis_core\core_cm4.h") } | Select-Object -First 1
  if (-not $cmsis) {
    throw "CMSIS headers not found under ufbt or Flipper69 (install ufbt SDK headers or Momentum lib)"
  }
}
$cmsisCore = Join-Path $cmsis "cmsis_core"
$cmsisDev = Join-Path $cmsis "stm32wb_cmsis\Include"

$usbInc = Join-Path $Root "third_party\libusb_stm32\inc"
$incRoot = @(
  "-I$usbInc", "-I$cmsisCore", "-I$cmsisDev",
  "-I$(Join-Path $Root 'platform\stm32wb55\bringup')",
  "-I$(Join-Path $Root 'platform\stm32wb55\usb')",
  "-I$(Join-Path $Root 'platform\stm32wb55')",
  "-I$(Join-Path $Root 'platform\stm32wb55\hal')",
  "-I$(Join-Path $Root 'kernel\include')",
  "-I$(Join-Path $Root 'hal\include')",
  "-I$(Join-Path $Root 'drivers\include')",
  "-I$(Join-Path $Root 'services\include')"
)
$commonFlags = @(
  "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard",
  "-ffunction-sections", "-fdata-sections", "-fno-common", "-Os", "-Wall",
  "-DSTM32WB55xx", "-DSTM32WBxx", "-DPLATFORMIO",
  "-DGLK_PLATFORM_STM32=1"
) + $incRoot
# Sim radio by default for non-OsRadio; OsRadio drops this for real CC1101.
if ($Profile -ne "OsRadio") {
  $commonFlags += "-DGLK_RADIO_SIM=1"
}
$ldflags = @(
  "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard",
  "-T$ld", "-Wl,--gc-sections", "-Wl,-Map=$build\map.map",
  "-nostartfiles", "-specs=nano.specs", "-specs=nosys.specs",
  "-lc", "-lm", "-lnosys"
)

$sources = @()
$sources += @{ Path = (Join-Path $Root "platform\stm32wb55\bringup\startup_stm32wb55.S"); Lang = "S" }

function Add-OsCoreSources {
  param([bool]$WithRealRadio)
  $script:sources += @{ Path = (Join-Path $Root "platform\stm32wb55\usb\glk_usb_rcc.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "platform\stm32wb55\port\glk_newlib_stubs.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "third_party\libusb_stm32\src\usbd_core.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "third_party\libusb_stm32\src\usbd_stm32wb55_devfs.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "kernel\src\glk_kernel.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\safety\glk_policy.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\audit\glk_audit.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\rpc\glk_rpc.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\agent\glk_agent.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\agent\glk_catalog.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\agent\glk_vault.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\skill\glk_skill.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\ml\glk_ml.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\power\glk_power.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "hal\src\glk_hal_subghz.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "drivers\src\glk_radio.c"); Lang = "c" }
  if ($WithRealRadio) {
    $script:sources += @{ Path = (Join-Path $Root "drivers\src\glk_cc1101.c"); Lang = "c" }
    $script:sources += @{ Path = (Join-Path $Root "platform\stm32wb55\hal\glk_board_spi_r.c"); Lang = "c" }
  }
  # On-device GUI (ST7567 + keys) — both OsCdc and OsRadio
  $script:sources += @{ Path = (Join-Path $Root "platform\stm32wb55\hal\glk_board_display.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\gui\glk_gui.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "apps\device\main_os_cdc.c"); Lang = "c" }
}

$deviceBudget = @(
  "-DUSB_PMASIZE=0x400",
  "-DGLK_HEAP_SIZE=4096",
  "-DGLK_MAX_MISSIONS=4",
  "-DGLK_MAX_MISSION_STEPS=16",
  "-DGLK_MAX_SKILLS=8",
  "-DGLK_MAX_TASKS=6",
  "-DGLK_MAX_MUTEXES=8",
  "-DGLK_MAX_QUEUES=4",
  "-DGLK_MAX_EVENTS=4",
  "-DGLK_MAX_TIMERS=4",
  "-DGLK_MAX_POOLS=2",
  "-DGLK_ML_ARENA_SIZE=256",
  "-DGLK_ML_MODEL_MAX=64",
  "-DGLK_SPECTRUM_MAX_BANDS=4"
)

if ($Profile -eq "Bringup") {
  $sources += @{ Path = (Join-Path $Root "platform\stm32wb55\bringup\system_stm32wb55.c"); Lang = "c" }
  $sources += @{ Path = (Join-Path $Root "platform\stm32wb55\bringup\main.c"); Lang = "c" }
  $outBase = "GrokLink-OS-v3.0.0-bringup"
  $label = "GrokLink OS 3.0 bringup"
  $version = "3.0.0-bringup"
} elseif ($Profile -eq "Cdc") {
  $sources += @{ Path = (Join-Path $Root "platform\stm32wb55\usb\glk_usb_rcc.c"); Lang = "c" }
  $sources += @{ Path = (Join-Path $Root "third_party\libusb_stm32\src\usbd_core.c"); Lang = "c" }
  $sources += @{ Path = (Join-Path $Root "third_party\libusb_stm32\src\usbd_stm32wb55_devfs.c"); Lang = "c" }
  $sources += @{ Path = (Join-Path $Root "apps\device\main_cdc_app.c"); Lang = "c" }
  $outBase = "GrokLink-OS-v3.0.1-cdc"
  $label = "GrokLink OS 3.0.1 CDC"
  $version = "3.0.1-cdc"
  $commonFlags += "-DUSB_PMASIZE=0x400"
} elseif ($Profile -eq "OsRadio") {
  Add-OsCoreSources -WithRealRadio $true
  $outBase = "GrokLink-OS-v3.7.0-radio"
  $label = "GrokLink OS 3.7.0 field research"
  $version = "3.7.0-radio"
  $commonFlags += $deviceBudget
  $commonFlags += "-DGLK_BUILD_RADIO=1"
} elseif ($Profile -eq "BisectLink" -or $Profile -eq "BisectInit") {
  # Full OsRadio object set, but CDC-style main (isolation).
  $script:sources += @{ Path = (Join-Path $Root "platform\stm32wb55\usb\glk_usb_rcc.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "platform\stm32wb55\port\glk_newlib_stubs.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "third_party\libusb_stm32\src\usbd_core.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "third_party\libusb_stm32\src\usbd_stm32wb55_devfs.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "kernel\src\glk_kernel.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\safety\glk_policy.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\audit\glk_audit.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\rpc\glk_rpc.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\agent\glk_agent.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\agent\glk_catalog.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\agent\glk_vault.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\skill\glk_skill.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\ml\glk_ml.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\power\glk_power.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "hal\src\glk_hal_subghz.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "drivers\src\glk_radio.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "drivers\src\glk_cc1101.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "platform\stm32wb55\hal\glk_board_spi_r.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "platform\stm32wb55\hal\glk_board_display.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "services\src\gui\glk_gui.c"); Lang = "c" }
  $script:sources += @{ Path = (Join-Path $Root "apps\device\main_bisect.c"); Lang = "c" }
  $commonFlags += $deviceBudget
  $commonFlags += "-DGLK_BUILD_RADIO=1"
  $commonFlags += "-DUSB_PMASIZE=0x400"
  if ($Profile -eq "BisectInit") {
    $commonFlags += "-DGLK_BISECT_INIT=1"
    $outBase = "GrokLink-OS-v3.6.8-bisect-init"
    $label = "GrokLink OS bisect-init"
    $version = "3.6.8-bisect-init"
  } else {
    $commonFlags += "-DGLK_BISECT_LINK=1"
    $outBase = "GrokLink-OS-v3.6.8-bisect-link"
    $label = "GrokLink OS bisect-link"
    $version = "3.6.8-bisect-link"
  }
} else {
  # OsCdc = RPC + policy + GUI + sim radio
  Add-OsCoreSources -WithRealRadio $false
  $outBase = "GrokLink-OS-v3.7.0-rpc"
  $label = "GrokLink OS 3.7.0 RPC"
  $version = "3.7.0-rpc"
  $commonFlags += $deviceBudget
}

$objs = @()
foreach ($s in $sources) {
  $base = [IO.Path]::GetFileNameWithoutExtension($s.Path)
  $obj = Join-Path $build "$base.o"
  Write-Host "CC $($s.Path)"
  if ($s.Lang -eq "S") {
    & $gcc @commonFlags -x assembler-with-cpp -c $s.Path -o $obj
  } else {
    & $gcc @commonFlags -c $s.Path -o $obj
  }
  if ($LASTEXITCODE -ne 0) { throw "Compile failed: $($s.Path)" }
  $objs += $obj
}

$elf = Join-Path $build "$outBase.elf"
$bin = Join-Path $OutDir "$outBase.bin"
$dfu = Join-Path $OutDir "$outBase.dfu"
$hex = Join-Path $OutDir "$outBase.hex"

Write-Host "LD $elf"
& $gcc @ldflags $objs -o $elf
if ($LASTEXITCODE -ne 0) { throw "Link failed" }

& $objcopy -O binary $elf $bin
& $objcopy -O ihex $elf $hex
& $size $elf

py -3 (Join-Path $Root "tools\bin2dfu.py") -i $bin -o $dfu -a 0x08000000 -l $label

$meta = @{
  product = "GrokLink OS"
  version = $version
  profile = $Profile
  radio = if ($Profile -eq "OsRadio") { "cc1101-spi" } else { "sim" }
  flash_address = "0x08000000"
  warning = "EXPERIMENTAL. Keep GrokLink-v2.1.3.dfu for recovery (tools/recover_flipper.ps1)."
  recover = "Hold BACK + USB DFU, then recover_flipper.ps1"
} | ConvertTo-Json
Set-Content -Path (Join-Path $OutDir "$outBase.json") -Value $meta -Encoding utf8

@"
$outBase
Profile: $Profile
Flash: hold BACK + plug USB (DFU), then:
  .\tools\flash_os_dfu_only.ps1 -DfuPath dist\dfu\$outBase.dfu
  or:  qFlipper-cli firmware $dfu
Recover lab firmware:
  .\tools\recover_flipper.ps1
Probe CDC:
  .\tools\device_probe.ps1
Radio (OsRadio):
  {`"cmd`":`"edu_ack`",`"phrase`":`"I_WILL_USE_ONLY_AUTHORIZED_TARGETS`"}
  {`"cmd`":`"subghz_probe`"}
  {`"cmd`":`"subghz_rx`",`"freq_hz`":433920000,`"ms`":400}
"@ | Set-Content (Join-Path $OutDir "$outBase-README.txt") -Encoding ascii

Write-Host "OK: $dfu"
Get-Item $bin, $dfu | Format-Table Name, Length
