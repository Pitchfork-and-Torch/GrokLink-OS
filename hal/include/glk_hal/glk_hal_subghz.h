/**
 * SubGHz HAL — CC1101 + RF path for Flipper-class F7 pinout.
 * Host builds use software simulation when GLK_PLATFORM_HOST is set.
 */
#pragma once

#include "glk/glk_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLK_RF_PATH_ISOLATE = 0,
    GLK_RF_PATH_315 = 1,
    GLK_RF_PATH_433 = 2,
    GLK_RF_PATH_868 = 3,
} glk_rf_path_t;

typedef void (*glk_subghz_edge_cb)(bool level, uint32_t dt_us, void* user);

glk_err_t glk_hal_subghz_init(void);
void glk_hal_subghz_deinit(void);

/** Reset chip, load OOK async preset, idle. */
glk_err_t glk_hal_subghz_reset(void);

glk_err_t glk_hal_subghz_idle(void);
glk_err_t glk_hal_subghz_sleep(void);

/** Select antenna matching path. */
void glk_hal_subghz_set_path(glk_rf_path_t path);

/** Auto path from frequency + program CC1101 PLL. Returns programmed Hz. */
uint32_t glk_hal_subghz_set_frequency_and_path(uint32_t freq_hz);

/**
 * Async RX window: count GDO0 edges for duration_ms.
 * Host: synthetic pulses. Target: EXTI on GDO0.
 */
glk_err_t glk_hal_subghz_rx_async(
    uint32_t freq_hz,
    uint32_t duration_ms,
    int32_t* out_pulses,
    int16_t* out_rssi_dbm);

/**
 * v3.5: same as rx_async but also samples RSSI near start/end of dwell
 * for min/max (stack only; no decode). Falls back to single RSSI if needed.
 */
glk_err_t glk_hal_subghz_rx_async_stats(
    uint32_t freq_hz,
    uint32_t duration_ms,
    int32_t* out_pulses,
    int16_t* out_rssi_dbm,
    int16_t* out_rssi_min,
    int16_t* out_rssi_max);

/** TX path: gated higher up; here only programs radio if allow flag is set. */
glk_err_t glk_hal_subghz_tx_carrier_ms(uint32_t freq_hz, uint32_t duration_ms);

bool glk_hal_subghz_is_frequency_valid(uint32_t freq_hz);

/** Chip identity probe (PARTNUM/VERSION). Host returns simulated IDs. */
glk_err_t glk_hal_subghz_probe(uint8_t* partnum, uint8_t* version);

#ifdef __cplusplus
}
#endif
