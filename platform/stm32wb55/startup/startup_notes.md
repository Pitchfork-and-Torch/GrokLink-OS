# STM32WB55 startup

Wire a CMSIS device pack (STM32CubeWB) for production images:

1. `startup_stm32wb55xx_cm4.s` — vector table + Reset_Handler  
2. `system_stm32wb55xx.c` — SystemInit, clocks (HSE + PLL → 64 MHz typical)  
3. Linker: `linker/stm32wb55_flash.ld` (adjust for BLE stack reserved flash)  
4. Call `glk_board_spi_r_init()` before `glk_hal_subghz_init()`  
5. IPCC / M0+ BLE binary optional; app runs on CM4 only for pure SubGHz labs  

Host simulation does **not** use these files.
