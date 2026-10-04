#pragma once
#include <stdint.h>

/** Full USB clock + pin setup (called from SystemInit). */
void glk_usb_rcc_init(void);

/** Re-assert PA11/PA12 AF10 after other board GPIO init. */
void glk_usb_pins_restore(void);

/** Busy-wait milliseconds (HSI16). */
void glk_usb_delay_ms(uint32_t ms);
