/**
 * CC1101 low-level driver (SPI-portable).
 * Target: real SPI via HAL; Host: optional soft-mock for unit tests.
 */
#pragma once

#include "glk/glk_types.h"
#include "cc1101_regs.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** SPI transport hooks provided by BSP. */
typedef struct {
    void (*cs_assert)(void);   /* CS low */
    void (*cs_deassert)(void); /* CS high */
    uint8_t (*xfer)(uint8_t tx); /* full-duplex one byte */
    void (*delay_us)(uint32_t us);
} glk_cc1101_bus_t;

typedef enum {
    GLK_CC1101_PRESET_OOK_ASYNC = 0,
    GLK_CC1101_PRESET_2FSK_ASYNC = 1,
} glk_cc1101_preset_t;

glk_err_t glk_cc1101_init(const glk_cc1101_bus_t* bus);
glk_err_t glk_cc1101_reset(void);
uint8_t glk_cc1101_strobe(uint8_t cmd);
glk_err_t glk_cc1101_write_reg(uint8_t reg, uint8_t val);
glk_err_t glk_cc1101_read_reg(uint8_t reg, uint8_t* val);
glk_err_t glk_cc1101_read_status(uint8_t reg, uint8_t* val);

/** Load a built-in async RX-friendly preset (TI-style OOK/2FSK). */
glk_err_t glk_cc1101_load_preset(glk_cc1101_preset_t preset);

/**
 * Program carrier frequency (Hz). Returns programmed Hz (quantized) or 0 on error.
 * Freq word = f * 2^16 / f_xosc
 */
uint32_t glk_cc1101_set_frequency(uint32_t freq_hz);

glk_err_t glk_cc1101_idle(void);
glk_err_t glk_cc1101_rx(void);
glk_err_t glk_cc1101_tx(void);
glk_err_t glk_cc1101_sleep(void);

uint8_t glk_cc1101_get_partnum(void);
uint8_t glk_cc1101_get_version(void);
int16_t glk_cc1101_get_rssi_dbm(void);

/** Convert freq_hz to register word (testable without SPI). */
uint32_t glk_cc1101_freq_to_word(uint32_t freq_hz);
uint32_t glk_cc1101_word_to_freq(uint32_t word);

bool glk_cc1101_ready(void);

#ifdef __cplusplus
}
#endif
