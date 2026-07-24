/**
 * Portable HAL surface — implemented per BSP.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void glk_hal_init(void);
uint32_t glk_hal_millis(void);
void glk_hal_delay_ms(uint32_t ms);
void glk_hal_watchdog_kick(void);

/* USB CDC byte I/O (target); host uses TCP instead. */
int glk_hal_usb_read(void* buf, size_t n);
int glk_hal_usb_write(const void* buf, size_t n);

/* SD present / block IO abstracted in storage service for host. */
bool glk_hal_sd_present(void);
