/**
 * Flipper F7 SPI_R + CC1101 GPIO board API (STM32WB55).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void glk_board_spi_r_init(void);
void glk_board_cc1101_cs(bool assert_low);
uint8_t glk_board_spi_r_xfer(uint8_t b);
void glk_board_delay_us(uint32_t us);
void glk_board_rf_sw0(bool high);

/** Enable/disable EXTI-based GDO0 edge capture (optional; prefer poll). */
void glk_board_g0_irq_enable(bool on);
uint32_t glk_board_g0_edge_count_take(void);

/**
 * Busy-poll PA1 (GDO0) for edge transitions for ~duration_ms.
 * Works without NVIC (USB path is polled). Returns edge count.
 */
uint32_t glk_board_g0_poll_edges_ms(uint32_t duration_ms);

/** Optional hook so radio dwell can keep USB alive (call usbd_poll). */
typedef void (*glk_board_usb_pump_fn)(void);
void glk_board_set_usb_pump(glk_board_usb_pump_fn fn);

/** Sample GDO0 level (true = high). */
bool glk_board_g0_read(void);

#ifdef __cplusplus
}
#endif
