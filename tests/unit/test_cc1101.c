#include "glk_drv/glk_cc1101.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

/* Soft SPI mock: records last writes, returns canned status. */
static uint8_t s_regs[0x40];
static uint8_t s_last_cmd;
static int s_cs;

static void cs_assert(void) { s_cs = 1; }
static void cs_deassert(void) { s_cs = 0; }
static void delay_us(uint32_t us) { (void)us; }

static uint8_t phase;
static uint8_t addr;

static uint8_t xfer(uint8_t tx) {
    if (!s_cs) return 0xFF;
    if (phase == 0) {
        s_last_cmd = tx;
        if ((tx & 0xC0) == 0x80 || (tx & 0xC0) == 0xC0) {
            /* read */
            addr = (uint8_t)(tx & 0x3F);
            phase = 1;
            return 0x0F; /* status */
        }
        if (tx < 0x30) {
            addr = tx;
            phase = 2; /* write data next */
            return 0x0F;
        }
        /* strobe */
        phase = 0;
        return 0x0F;
    }
    if (phase == 1) {
        phase = 0;
        if (addr == CC1101_PARTNUM) return 0x00;
        if (addr == CC1101_VERSION) return 0x14;
        return s_regs[addr & 0x3F];
    }
    if (phase == 2) {
        s_regs[addr & 0x3F] = tx;
        phase = 0;
        return 0x0F;
    }
    return 0x0F;
}

int main(void) {
    /* Frequency math (no SPI) */
    uint32_t w = glk_cc1101_freq_to_word(433920000u);
    assert(w != 0);
    uint32_t back = glk_cc1101_word_to_freq(w);
    /* within ~100 Hz quantization */
    int32_t err = (int32_t)back - 433920000;
    if (err < 0) err = -err;
    assert(err < 200);

    glk_cc1101_bus_t bus = {
        .cs_assert = cs_assert,
        .cs_deassert = cs_deassert,
        .xfer = xfer,
        .delay_us = delay_us,
    };
    assert(glk_cc1101_init(&bus) == GLK_OK);
    assert(glk_cc1101_reset() == GLK_OK);
    assert(glk_cc1101_load_preset(GLK_CC1101_PRESET_OOK_ASYNC) == GLK_OK);
    uint32_t prog = glk_cc1101_set_frequency(433920000u);
    assert(prog != 0);
    assert(s_regs[CC1101_FREQ2] != 0 || s_regs[CC1101_FREQ1] != 0 || s_regs[CC1101_FREQ0] != 0);

    printf("test_cc1101: OK word=0x%06X prog=%u\n", w, prog);
    return 0;
}
