/**
 * SubGHz HAL implementation.
 * - HOST / GLK_RADIO_SIM: deterministic pulse sim
 * - STM32 (no SIM): CC1101 over SPI_R + GDO0 edge poll
 */
#include "glk_hal/glk_hal_subghz.h"
#include "glk_drv/glk_cc1101.h"
#include "glk/glk_config.h"
#include "glk/glk_kernel.h"

#include <string.h>

/* Band cutovers match Flipper-class matching network (see pins_f7.h). */
#ifndef GLK_RF_PATH_315_MAX_HZ
#define GLK_RF_PATH_315_MAX_HZ 350000000u
#endif
#ifndef GLK_RF_PATH_433_MAX_HZ
#define GLK_RF_PATH_433_MAX_HZ 650000000u
#endif

#if defined(GLK_PLATFORM_STM32) && !defined(GLK_PLATFORM_HOST) && !defined(GLK_RADIO_SIM)
#define GLK_SUBGHZ_HW 1
#else
#define GLK_SUBGHZ_HW 0
#endif

static glk_rf_path_t s_path = GLK_RF_PATH_ISOLATE;
static bool s_inited;

#if GLK_SUBGHZ_HW
/* Weak stubs — board layer (glk_board_spi_r.c) overrides with real GPIO/SPI. */
__attribute__((weak)) void glk_board_spi_r_init(void) {
}
__attribute__((weak)) void glk_board_cc1101_cs(bool assert_low) {
    (void)assert_low;
}
__attribute__((weak)) uint8_t glk_board_spi_r_xfer(uint8_t b) {
    return b;
}
__attribute__((weak)) void glk_board_delay_us(uint32_t us) {
    for (volatile uint32_t i = 0; i < us * 8u; i++) {
    }
}
__attribute__((weak)) void glk_board_rf_sw0(bool high) {
    (void)high;
}
__attribute__((weak)) void glk_board_g0_irq_enable(bool on) {
    (void)on;
}
__attribute__((weak)) uint32_t glk_board_g0_edge_count_take(void) {
    return 0;
}
__attribute__((weak)) uint32_t glk_board_g0_poll_edges_ms(uint32_t duration_ms) {
    glk_task_sleep_ms(duration_ms);
    return 0;
}

static void cs_assert(void) {
    glk_board_cc1101_cs(true);
}
static void cs_deassert(void) {
    glk_board_cc1101_cs(false);
}
static uint8_t xfer(uint8_t b) {
    return glk_board_spi_r_xfer(b);
}
static void delay_us(uint32_t us) {
    glk_board_delay_us(us);
}
#endif

bool glk_hal_subghz_is_frequency_valid(uint32_t freq_hz) {
    return freq_hz >= GLK_SUBGHZ_FREQ_MIN_HZ && freq_hz <= GLK_SUBGHZ_FREQ_MAX_HZ;
}

void glk_hal_subghz_set_path(glk_rf_path_t path) {
    s_path = path;
#if GLK_SUBGHZ_HW
    /* F7: 433 => SW0=0; 315/868 => SW0=1; isolate => 0 */
    if (path == GLK_RF_PATH_433 || path == GLK_RF_PATH_ISOLATE) {
        glk_board_rf_sw0(false);
    } else {
        glk_board_rf_sw0(true);
    }
#else
    (void)path;
#endif
}

uint32_t glk_hal_subghz_set_frequency_and_path(uint32_t freq_hz) {
    if (!glk_hal_subghz_is_frequency_valid(freq_hz)) return 0;
    if (freq_hz < GLK_RF_PATH_315_MAX_HZ) {
        glk_hal_subghz_set_path(GLK_RF_PATH_315);
    } else if (freq_hz < GLK_RF_PATH_433_MAX_HZ) {
        glk_hal_subghz_set_path(GLK_RF_PATH_433);
    } else {
        glk_hal_subghz_set_path(GLK_RF_PATH_868);
    }
#if GLK_SUBGHZ_HW
    return glk_cc1101_set_frequency(freq_hz);
#else
    return freq_hz;
#endif
}

glk_err_t glk_hal_subghz_init(void) {
    if (s_inited) return GLK_OK;
#if GLK_SUBGHZ_HW
    glk_board_spi_r_init();
    glk_cc1101_bus_t bus = {
        .cs_assert = cs_assert,
        .cs_deassert = cs_deassert,
        .xfer = xfer,
        .delay_us = delay_us,
    };
    if (glk_cc1101_init(&bus) != GLK_OK) return GLK_ERR_GENERIC;
    if (glk_cc1101_reset() != GLK_OK) return GLK_ERR_GENERIC;
    glk_cc1101_load_preset(GLK_CC1101_PRESET_OOK_ASYNC);
    glk_hal_subghz_set_path(GLK_RF_PATH_ISOLATE);
    glk_cc1101_idle();
#endif
    s_inited = true;
    return GLK_OK;
}

void glk_hal_subghz_deinit(void) {
    glk_hal_subghz_sleep();
    s_inited = false;
}

glk_err_t glk_hal_subghz_reset(void) {
#if GLK_SUBGHZ_HW
    glk_cc1101_reset();
    glk_cc1101_load_preset(GLK_CC1101_PRESET_OOK_ASYNC);
    return glk_cc1101_idle();
#else
    return GLK_OK;
#endif
}

glk_err_t glk_hal_subghz_idle(void) {
#if GLK_SUBGHZ_HW
    return glk_cc1101_idle();
#else
    return GLK_OK;
#endif
}

glk_err_t glk_hal_subghz_sleep(void) {
#if GLK_SUBGHZ_HW
    glk_hal_subghz_set_path(GLK_RF_PATH_ISOLATE);
    return glk_cc1101_sleep();
#else
    return GLK_OK;
#endif
}

#if !GLK_SUBGHZ_HW
static int32_t sim_pulses(uint32_t freq_hz, uint32_t ms) {
    uint32_t h = (freq_hz / 1000u) ^ (ms * 2654435761u);
    int32_t base = (int32_t)(h % 40u);
    if (freq_hz >= 433000000u && freq_hz <= 434000000u) base += 15;
    if (freq_hz >= 300000000u && freq_hz <= 310000000u) base += 10;
    return base;
}
#endif

#if GLK_SUBGHZ_HW
/** Minimal write: header + value (no full driver state). */
static void hw_wr(uint8_t reg, uint8_t val) {
    glk_board_cc1101_cs(true);
    (void)glk_board_spi_r_xfer(reg);
    (void)glk_board_spi_r_xfer(val);
    glk_board_cc1101_cs(false);
}
static uint8_t hw_strobe(uint8_t cmd) {
    glk_board_cc1101_cs(true);
    uint8_t st = glk_board_spi_r_xfer(cmd);
    glk_board_cc1101_cs(false);
    return st;
}
static uint8_t hw_status(uint8_t reg) {
    glk_board_cc1101_cs(true);
    (void)glk_board_spi_r_xfer((uint8_t)(reg | 0xC0)); /* READ|BURST */
    uint8_t v = glk_board_spi_r_xfer(0x00);
    glk_board_cc1101_cs(false);
    return v;
}
#endif

static int16_t rssi_from_raw(uint8_t raw) {
    if (raw >= 128) return (int16_t)((int16_t)(raw - 256) / 2 - 74);
    return (int16_t)(raw / 2 - 74);
}

glk_err_t glk_hal_subghz_rx_async(
    uint32_t freq_hz,
    uint32_t duration_ms,
    int32_t* out_pulses,
    int16_t* out_rssi_dbm) {
    return glk_hal_subghz_rx_async_stats(
        freq_hz, duration_ms, out_pulses, out_rssi_dbm, NULL, NULL);
}

glk_err_t glk_hal_subghz_rx_async_stats(
    uint32_t freq_hz,
    uint32_t duration_ms,
    int32_t* out_pulses,
    int16_t* out_rssi_dbm,
    int16_t* out_rssi_min,
    int16_t* out_rssi_max) {
    if (!glk_hal_subghz_is_frequency_valid(freq_hz)) return GLK_ERR_INVAL;
    if (duration_ms == 0) duration_ms = 200;
    if (duration_ms > 500) duration_ms = 500; /* keep USB responsive */

#if GLK_SUBGHZ_HW
    /*
     * Ultra-light RX: same SPI style as working probe. Avoid SRES/SPWD and long
     * preset sequences that hard-faulted some builds. Cap window at 100 ms.
     * v3.5: sample RSSI early + late for min/max (no decode, tiny stack).
     */
    if (duration_ms > 100) duration_ms = 100;
    glk_board_spi_r_init();
    /* RF path SW0 only — no heavy switch logic */
    glk_board_rf_sw0(freq_hz >= GLK_RF_PATH_315_MAX_HZ && freq_hz < GLK_RF_PATH_433_MAX_HZ ? false : true);

    hw_wr(0x02, 0x0D); /* IOCFG0 = serial async out on GDO0 */
    {
        /* word = freq * 2^16 / 26e6 */
        uint32_t word = (uint32_t)(((uint64_t)freq_hz * 65536ull) / 26000000ull);
        hw_wr(0x0D, (uint8_t)((word >> 16) & 0xFF));
        hw_wr(0x0E, (uint8_t)((word >> 8) & 0xFF));
        hw_wr(0x0F, (uint8_t)(word & 0xFF));
    }
    (void)hw_strobe(0x34); /* SRX */

    /* Early RSSI sample (after brief settle) */
    glk_task_sleep_ms(2);
    int16_t rssi_a = rssi_from_raw(hw_status(0x34));

    uint32_t edges = 0;
    if (duration_ms > 4) {
        edges = glk_board_g0_poll_edges_ms(duration_ms - 2);
    } else {
        edges = glk_board_g0_poll_edges_ms(duration_ms);
    }

    int16_t rssi_b = rssi_from_raw(hw_status(0x34));
    int16_t rssi = rssi_b;
    int16_t rmin = rssi_a < rssi_b ? rssi_a : rssi_b;
    int16_t rmax = rssi_a > rssi_b ? rssi_a : rssi_b;

    (void)hw_strobe(0x36); /* SIDLE */
    glk_board_rf_sw0(false);

    if (out_pulses) *out_pulses = (int32_t)edges;
    if (out_rssi_dbm) *out_rssi_dbm = rssi;
    if (out_rssi_min) *out_rssi_min = rmin;
    if (out_rssi_max) *out_rssi_max = rmax;
    return GLK_OK;
#else
    if (!s_inited) glk_hal_subghz_init();
    (void)s_path;
    uint32_t sleep_ms = duration_ms > 100 ? 100 : duration_ms;
    glk_task_sleep_ms(sleep_ms);
    int32_t p = sim_pulses(freq_hz, duration_ms);
    int16_t rssi = (int16_t)(-40 - (p % 30));
    if (out_pulses) *out_pulses = p;
    if (out_rssi_dbm) *out_rssi_dbm = rssi;
    if (out_rssi_min) *out_rssi_min = (int16_t)(rssi - 2);
    if (out_rssi_max) *out_rssi_max = (int16_t)(rssi + 1);
    return GLK_OK;
#endif
}

glk_err_t glk_hal_subghz_tx_carrier_ms(uint32_t freq_hz, uint32_t duration_ms) {
    /* Actual RF TX only on hardware; still requires policy at service layer. */
    if (!glk_hal_subghz_is_frequency_valid(freq_hz)) return GLK_ERR_INVAL;
#if GLK_SUBGHZ_HW
    if (!s_inited) glk_hal_subghz_init();
    glk_cc1101_idle();
    glk_cc1101_load_preset(GLK_CC1101_PRESET_OOK_ASYNC);
    if (glk_hal_subghz_set_frequency_and_path(freq_hz) == 0) return GLK_ERR_INVAL;
    glk_cc1101_tx();
    glk_task_sleep_ms(duration_ms > GLK_TX_MAX_MS ? GLK_TX_MAX_MS : duration_ms);
    glk_cc1101_idle();
    glk_hal_subghz_set_path(GLK_RF_PATH_ISOLATE);
    glk_cc1101_sleep();
    return GLK_OK;
#else
    (void)duration_ms;
    /* Host: no RF — success means "path validated" only */
    return GLK_OK;
#endif
}

glk_err_t glk_hal_subghz_probe(uint8_t* partnum, uint8_t* version) {
#if GLK_SUBGHZ_HW
    /*
     * Proven live path (3.0.8): board SPI only — no full preset load here.
     * Full glk_cc1101_init/reset/preset runs on first RX/TX.
     */
    glk_board_spi_r_init();
    glk_board_cc1101_cs(true);
    uint8_t st = glk_board_spi_r_xfer(0x3D); /* SNOP status */
    glk_board_cc1101_cs(false);
    glk_board_delay_us(20);
    /* VERSION status: 0x31 | READ | BURST = 0xF1 */
    glk_board_cc1101_cs(true);
    (void)glk_board_spi_r_xfer(0xF1);
    uint8_t ver = glk_board_spi_r_xfer(0x00);
    glk_board_cc1101_cs(false);
    /* PARTNUM status: 0x30 | READ | BURST = 0xF0 */
    glk_board_cc1101_cs(true);
    (void)glk_board_spi_r_xfer(0xF0);
    uint8_t pn = glk_board_spi_r_xfer(0x00);
    glk_board_cc1101_cs(false);
    (void)st;
    if (partnum) *partnum = pn;
    if (version) *version = ver;
#else
    if (!s_inited) glk_hal_subghz_init();
    if (partnum) *partnum = 0x00; /* sim */
    if (version) *version = 0x14;
#endif
    return GLK_OK;
}
