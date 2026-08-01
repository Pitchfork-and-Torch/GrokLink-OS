/**
 * STM32WB55 board profile hooks (Flipper Zero class pinout abstract).
 * Full register init lives with vendor CMSIS pack during silicon bring-up.
 */
#pragma once

#include <stdint.h>

#define GLK_BOARD_NAME "stm32wb55-research"
#define GLK_BOARD_HAS_CC1101 1
#define GLK_BOARD_HAS_NFC 1
#define GLK_BOARD_HAS_IR 1
#define GLK_BOARD_HAS_SD 1
#define GLK_BOARD_HAS_USB 1
#define GLK_BOARD_HAS_BLE_M0 1

void glk_board_early_init(void);
void glk_board_clock_init(void);
void glk_board_gpio_init(void);
uint32_t glk_board_random_u32(void);
