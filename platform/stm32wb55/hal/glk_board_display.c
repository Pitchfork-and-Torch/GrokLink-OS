/**
 * ST7567 128×64 on Flipper F7 SPI_D pins (bit-bang).
 * CS PC11, RST PB0, A0/DI PB1, SCK PD1, MOSI PB15.
 * Buttons: Up PB10, Down PC6, Left PB11, Right PB12, OK PH3, Back PC13 (active low).
 */
#include "glk_board_display.h"
#include "stm32_compat.h"

#include <string.h>

#if defined(GLK_PLATFORM_STM32)

static uint8_t s_fb[GLK_DISP_W * GLK_DISP_H / 8];
static bool s_ready;

/* 5×7 ASCII font (space..~), column-major, LSB top */
static const uint8_t s_font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* sp */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* ! */
    {0x00, 0x07, 0x00, 0x07, 0x00},
    {0x14, 0x7F, 0x14, 0x7F, 0x14},
    {0x24, 0x2A, 0x7F, 0x2A, 0x12},
    {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50},
    {0x00, 0x05, 0x03, 0x00, 0x00},
    {0x00, 0x1C, 0x22, 0x41, 0x00},
    {0x00, 0x41, 0x22, 0x1C, 0x00},
    {0x08, 0x2A, 0x1C, 0x2A, 0x08},
    {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00},
    {0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x00, 0x56, 0x36, 0x00, 0x00},
    {0x00, 0x08, 0x14, 0x22, 0x41},
    {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x41, 0x22, 0x14, 0x08, 0x00},
    {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x32, 0x49, 0x79, 0x41, 0x3E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* A */
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x01, 0x01},
    {0x3E, 0x41, 0x41, 0x51, 0x32},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x04, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x7F, 0x20, 0x18, 0x20, 0x7F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x03, 0x04, 0x78, 0x04, 0x03},
    {0x61, 0x51, 0x49, 0x45, 0x43},
    {0x00, 0x00, 0x7F, 0x41, 0x41},
    {0x02, 0x04, 0x08, 0x10, 0x20},
    {0x41, 0x41, 0x7F, 0x00, 0x00},
    {0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x01, 0x02, 0x04, 0x00},
    {0x20, 0x54, 0x54, 0x54, 0x78}, /* a */
    {0x7F, 0x48, 0x44, 0x44, 0x38},
    {0x38, 0x44, 0x44, 0x44, 0x20},
    {0x38, 0x44, 0x44, 0x48, 0x7F},
    {0x38, 0x54, 0x54, 0x54, 0x18},
    {0x08, 0x7E, 0x09, 0x01, 0x02},
    {0x08, 0x14, 0x54, 0x54, 0x3C},
    {0x7F, 0x08, 0x04, 0x04, 0x78},
    {0x00, 0x44, 0x7D, 0x40, 0x00},
    {0x20, 0x40, 0x44, 0x3D, 0x00},
    {0x00, 0x7F, 0x10, 0x28, 0x44},
    {0x00, 0x41, 0x7F, 0x40, 0x00},
    {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78},
    {0x38, 0x44, 0x44, 0x44, 0x38},
    {0x7C, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7C},
    {0x7C, 0x08, 0x04, 0x04, 0x08},
    {0x48, 0x54, 0x54, 0x54, 0x24},
    {0x04, 0x3F, 0x44, 0x40, 0x20},
    {0x3C, 0x40, 0x40, 0x20, 0x7C},
    {0x1C, 0x20, 0x40, 0x20, 0x1C},
    {0x3C, 0x40, 0x30, 0x40, 0x3C},
    {0x44, 0x28, 0x10, 0x28, 0x44},
    {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44},
};

static void delay_us(uint32_t us) {
    for (volatile uint32_t i = 0; i < us * 4u; i++) {
    }
}

static void pin_out(GPIO_TypeDef* p, unsigned pin, bool high) {
    p->OTYPER &= ~(1u << pin);
    p->OSPEEDR |= (3u << (pin * 2u));
    p->PUPDR &= ~(3u << (pin * 2u));
    if (high) p->BSRR = (1u << pin);
    else p->BSRR = (1u << (pin + 16u));
    p->MODER = (p->MODER & ~(3u << (pin * 2u))) | (1u << (pin * 2u));
}

static void pin_in_pu(GPIO_TypeDef* p, unsigned pin) {
    p->PUPDR = (p->PUPDR & ~(3u << (pin * 2u))) | (1u << (pin * 2u));
    p->MODER &= ~(3u << (pin * 2u));
}

static bool pin_read(GPIO_TypeDef* p, unsigned pin) {
    return (p->IDR & (1u << pin)) != 0u;
}

/* SPI_D bitbang */
static void sck(bool h) {
    if (h) GPIOD->BSRR = (1u << 1);
    else GPIOD->BSRR = (1u << (1 + 16));
}
static void mosi(bool h) {
    if (h) GPIOB->BSRR = (1u << 15);
    else GPIOB->BSRR = (1u << (15 + 16));
}
static void cs(bool assert_low) {
    if (assert_low) GPIOC->BSRR = (1u << (11 + 16));
    else GPIOC->BSRR = (1u << 11);
}
static void a0(bool data) {
    if (data) GPIOB->BSRR = (1u << 1);
    else GPIOB->BSRR = (1u << (1 + 16));
}

static void spi_byte(uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        mosi((b >> i) & 1);
        sck(1);
        sck(0);
    }
}

static void cmd(uint8_t c) {
    cs(1);
    a0(0);
    spi_byte(c);
    cs(0);
}

static void data_buf(const uint8_t* p, unsigned n) {
    cs(1);
    a0(1);
    for (unsigned i = 0; i < n; i++) spi_byte(p[i]);
    cs(0);
}

void glk_display_init(void) {
    if (s_ready) return;
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN |
                    RCC_AHB2ENR_GPIODEN | RCC_AHB2ENR_GPIOHEN;
    (void)RCC->AHB2ENR;

    pin_out(GPIOC, 11, true);  /* CS high */
    pin_out(GPIOB, 0, true);   /* RST high */
    pin_out(GPIOB, 1, false);  /* A0 */
    pin_out(GPIOD, 1, false);  /* SCK */
    pin_out(GPIOB, 15, false); /* MOSI */

    /* Reset pulse */
    GPIOB->BSRR = (1u << (0 + 16));
    delay_us(50);
    GPIOB->BSRR = (1u << 0);
    delay_us(50);

    cmd(0xE2); /* soft reset */
    cmd(0xA2); /* bias 1/9 */
    /* 180° vs early bring-up: panel was upside-down with A1/C0 */
    cmd(0xA0); /* SEG normal */
    cmd(0xC8); /* COM reverse — right-side-up on F7 */
    cmd(0x25); /* regulation ratio */
    cmd(0x81);
    cmd(0x22); /* contrast (slightly higher for readability) */
    cmd(0x2F); /* power control */
    cmd(0x40); /* start line 0 */
    cmd(0xAF); /* display on */
    cmd(0xA6); /* normal (not inverted) */

    memset(s_fb, 0, sizeof(s_fb));
    s_ready = true;
    glk_display_flush();
}

void glk_display_clear(void) {
    memset(s_fb, 0, sizeof(s_fb));
}

void glk_display_set_pixel(int x, int y, bool on) {
    if (x < 0 || y < 0 || x >= GLK_DISP_W || y >= GLK_DISP_H) return;
    unsigned idx = (unsigned)x + ((unsigned)(y >> 3) * GLK_DISP_W);
    uint8_t m = (uint8_t)(1u << (y & 7));
    if (on) s_fb[idx] |= m;
    else s_fb[idx] &= (uint8_t)~m;
}

void glk_display_draw_hline(int x, int y, int w) {
    for (int i = 0; i < w; i++) glk_display_set_pixel(x + i, y, true);
}

void glk_display_invert_rect(int x, int y, int w, int h) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) {
            if (xx < 0 || yy < 0 || xx >= GLK_DISP_W || yy >= GLK_DISP_H) continue;
            unsigned idx = (unsigned)xx + ((unsigned)(yy >> 3) * GLK_DISP_W);
            s_fb[idx] ^= (uint8_t)(1u << (yy & 7));
        }
}

void glk_display_draw_str(int x, int y, const char* s) {
    if (!s) return;
    const unsigned nfont = (unsigned)(sizeof(s_font5x7) / sizeof(s_font5x7[0]));
    int cx = x;
    for (; *s; s++) {
        unsigned idx = 0;
        if (*s >= 32) {
            idx = (unsigned)(*s - 32);
            if (idx >= nfont) idx = (unsigned)('?' - 32);
        }
        const uint8_t* g = s_font5x7[idx];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1u << row)) glk_display_set_pixel(cx + col, y + row, true);
            }
        }
        cx += 6;
        if (cx >= GLK_DISP_W) break;
    }
}

void glk_display_flush(void) {
    if (!s_ready) return;
    for (int page = 0; page < 8; page++) {
        cmd((uint8_t)(0xB0 | page));
        cmd(0x10); /* col high */
        cmd(0x00); /* col low */
        data_buf(&s_fb[page * GLK_DISP_W], GLK_DISP_W);
    }
}

void glk_keys_init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN | RCC_AHB2ENR_GPIOHEN;
    (void)RCC->AHB2ENR;
    pin_in_pu(GPIOB, 10); /* up */
    pin_in_pu(GPIOC, 6);  /* down */
    pin_in_pu(GPIOB, 11); /* left */
    pin_in_pu(GPIOB, 12); /* right */
    pin_in_pu(GPIOH, 3);  /* ok (active high on F7) */
    pin_in_pu(GPIOC, 13); /* back */
}

void glk_keys_sample(glk_keys_t* out) {
    if (!out) return;
    out->up = !pin_read(GPIOB, 10);
    out->down = !pin_read(GPIOC, 6);
    out->left = !pin_read(GPIOB, 11);
    out->right = !pin_read(GPIOB, 12);
    out->ok = pin_read(GPIOH, 3); /* not inverted */
    out->back = !pin_read(GPIOC, 13);
}

#else
void glk_display_init(void) {
}
void glk_display_clear(void) {
}
void glk_display_flush(void) {
}
void glk_display_set_pixel(int x, int y, bool on) {
    (void)x;
    (void)y;
    (void)on;
}
void glk_display_draw_str(int x, int y, const char* s) {
    (void)x;
    (void)y;
    (void)s;
}
void glk_display_draw_hline(int x, int y, int w) {
    (void)x;
    (void)y;
    (void)w;
}
void glk_display_invert_rect(int x, int y, int w, int h) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}
void glk_keys_init(void) {
}
void glk_keys_sample(glk_keys_t* out) {
    if (out) memset(out, 0, sizeof(*out));
}
#endif
