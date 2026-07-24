/**
 * CC1101 driver — TI datasheet algorithms, GrokLink OS owned.
 */
#include "glk_drv/glk_cc1101.h"

#include <string.h>

static const glk_cc1101_bus_t* s_bus;
static bool s_ready;

/* Minimal OOK async-friendly register pairs (addr, value), 0xFF terminator.
 * Tuned for educational pulse-edge capture on GDO0, not protocol decode. */
static const uint8_t s_preset_ook_async[] = {
    CC1101_IOCFG0,   0x0D, /* GDO0 = serial data output */
    CC1101_FIFOTHR,  0x47,
    CC1101_PKTCTRL0, 0x32, /* async serial mode */
    CC1101_FSCTRL1,  0x06,
    CC1101_MDMCFG4,  0xF6, /* wider RX BW for OOK lab use */
    CC1101_MDMCFG3,  0x83,
    CC1101_MDMCFG2,  0x30, /* OOK, no preamble/sync requirement */
    CC1101_MDMCFG1,  0x22,
    CC1101_MDMCFG0,  0xF8,
    CC1101_MCSM0,    0x18,
    CC1101_FOCCFG,   0x16,
    CC1101_AGCCTRL2, 0x04,
    CC1101_AGCCTRL1, 0x00,
    CC1101_AGCCTRL0, 0x92,
    CC1101_FREND1,   0x56,
    CC1101_FREND0,   0x11,
    CC1101_FSCAL3,   0xE9,
    CC1101_FSCAL2,   0x2A,
    CC1101_FSCAL1,   0x00,
    CC1101_FSCAL0,   0x1F,
    CC1101_TEST2,    0x81,
    CC1101_TEST1,    0x35,
    CC1101_TEST0,    0x09,
    0xFF, 0xFF,
};

static const uint8_t s_preset_2fsk_async[] = {
    CC1101_IOCFG0,   0x0D,
    CC1101_FIFOTHR,  0x47,
    CC1101_PKTCTRL0, 0x32,
    CC1101_FSCTRL1,  0x06,
    CC1101_MDMCFG4,  0xC8,
    CC1101_MDMCFG3,  0x93,
    CC1101_MDMCFG2,  0x10, /* 2-FSK */
    CC1101_DEVIATN,  0x34,
    CC1101_MCSM0,    0x18,
    CC1101_FOCCFG,   0x16,
    CC1101_AGCCTRL2, 0x43,
    CC1101_FREND0,   0x10,
    CC1101_FSCAL3,   0xE9,
    CC1101_FSCAL2,   0x2A,
    CC1101_FSCAL1,   0x00,
    CC1101_FSCAL0,   0x1F,
    0xFF, 0xFF,
};

static void bus_delay(uint32_t us) {
    if (s_bus && s_bus->delay_us) s_bus->delay_us(us);
}

static uint8_t spi_txrx(uint8_t b) {
    if (!s_bus || !s_bus->xfer) return 0xFF;
    return s_bus->xfer(b);
}

static void cs_lo(void) {
    if (s_bus && s_bus->cs_assert) s_bus->cs_assert();
}

static void cs_hi(void) {
    if (s_bus && s_bus->cs_deassert) s_bus->cs_deassert();
}

uint32_t glk_cc1101_freq_to_word(uint32_t freq_hz) {
    /* word = freq * 2^16 / f_xosc */
    uint64_t w = ((uint64_t)freq_hz * (uint64_t)CC1101_FDIV) / (uint64_t)CC1101_XOSC_HZ;
    return (uint32_t)(w & CC1101_FMASK);
}

uint32_t glk_cc1101_word_to_freq(uint32_t word) {
    uint64_t f = ((uint64_t)(word & CC1101_FMASK) * (uint64_t)CC1101_XOSC_HZ) / (uint64_t)CC1101_FDIV;
    return (uint32_t)f;
}

glk_err_t glk_cc1101_init(const glk_cc1101_bus_t* bus) {
    if (!bus || !bus->xfer || !bus->cs_assert || !bus->cs_deassert) return GLK_ERR_INVAL;
    s_bus = bus;
    s_ready = false;
    cs_hi();
    bus_delay(100);
    return GLK_OK;
}

uint8_t glk_cc1101_strobe(uint8_t cmd) {
    cs_lo(); /* board waits CHIP_RDYn (MISO low) */
    uint8_t st = spi_txrx(cmd);
    cs_hi();
    return st;
}

glk_err_t glk_cc1101_write_reg(uint8_t reg, uint8_t val) {
    cs_lo();
    spi_txrx(reg);
    spi_txrx(val);
    cs_hi();
    return GLK_OK;
}

glk_err_t glk_cc1101_read_reg(uint8_t reg, uint8_t* val) {
    if (!val) return GLK_ERR_INVAL;
    cs_lo();
    spi_txrx((uint8_t)(reg | CC1101_READ));
    *val = spi_txrx(0x00);
    cs_hi();
    return GLK_OK;
}

glk_err_t glk_cc1101_read_status(uint8_t reg, uint8_t* val) {
    if (!val) return GLK_ERR_INVAL;
    /* Status regs: header = addr | READ | BURST (same as Flipper) */
    cs_lo();
    spi_txrx((uint8_t)(reg | CC1101_READ | CC1101_BURST));
    *val = spi_txrx(0x00);
    cs_hi();
    return GLK_OK;
}

glk_err_t glk_cc1101_reset(void) {
    if (!s_bus) return GLK_ERR_INVAL;
    /* TI power-on / manual reset (keep short so USB poll is not starved) */
    cs_hi();
    bus_delay(80);
    cs_lo();
    bus_delay(40);
    cs_hi();
    bus_delay(40);
    glk_cc1101_strobe(CC1101_SRES);
    bus_delay(200);
    glk_cc1101_strobe(CC1101_SNOP);
    bus_delay(20);
    s_ready = true;
    return GLK_OK;
}

static glk_err_t load_pairs(const uint8_t* pairs) {
    for (size_t i = 0; pairs[i] != 0xFF || pairs[i + 1] != 0xFF; i += 2) {
        if (pairs[i] == 0xFF) break;
        glk_cc1101_write_reg(pairs[i], pairs[i + 1]);
    }
    return GLK_OK;
}

glk_err_t glk_cc1101_load_preset(glk_cc1101_preset_t preset) {
    if (!s_ready && glk_cc1101_reset() != GLK_OK) return GLK_ERR_GENERIC;
    glk_cc1101_strobe(CC1101_SIDLE);
    if (preset == GLK_CC1101_PRESET_2FSK_ASYNC) return load_pairs(s_preset_2fsk_async);
    return load_pairs(s_preset_ook_async);
}

uint32_t glk_cc1101_set_frequency(uint32_t freq_hz) {
    if (freq_hz < 281000000u || freq_hz > 928000000u) return 0;
    uint32_t word = glk_cc1101_freq_to_word(freq_hz);
    glk_cc1101_write_reg(CC1101_FREQ2, (uint8_t)((word >> 16) & 0xFF));
    glk_cc1101_write_reg(CC1101_FREQ1, (uint8_t)((word >> 8) & 0xFF));
    glk_cc1101_write_reg(CC1101_FREQ0, (uint8_t)(word & 0xFF));
    return glk_cc1101_word_to_freq(word);
}

glk_err_t glk_cc1101_idle(void) {
    glk_cc1101_strobe(CC1101_SIDLE);
    return GLK_OK;
}

glk_err_t glk_cc1101_rx(void) {
    glk_cc1101_strobe(CC1101_SFRX);
    glk_cc1101_strobe(CC1101_SRX);
    return GLK_OK;
}

glk_err_t glk_cc1101_tx(void) {
    glk_cc1101_strobe(CC1101_SFTX);
    glk_cc1101_strobe(CC1101_STX);
    return GLK_OK;
}

glk_err_t glk_cc1101_sleep(void) {
    glk_cc1101_strobe(CC1101_SIDLE);
    glk_cc1101_strobe(CC1101_SPWD);
    return GLK_OK;
}

uint8_t glk_cc1101_get_partnum(void) {
    uint8_t v = 0;
    glk_cc1101_read_status(CC1101_PARTNUM, &v);
    return v;
}

uint8_t glk_cc1101_get_version(void) {
    uint8_t v = 0;
    glk_cc1101_read_status(CC1101_VERSION, &v);
    return v;
}

int16_t glk_cc1101_get_rssi_dbm(void) {
    uint8_t raw = 0;
    glk_cc1101_read_status(CC1101_RSSI, &raw);
    int16_t rssi;
    if (raw >= 128) rssi = (int16_t)((int16_t)(raw - 256) / 2 - 74);
    else rssi = (int16_t)(raw / 2 - 74);
    return rssi;
}

bool glk_cc1101_ready(void) {
    return s_ready;
}
