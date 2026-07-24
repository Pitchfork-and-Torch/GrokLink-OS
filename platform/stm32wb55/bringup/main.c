/**
 * GrokLink OS 3.0 — Flipper F7 / STM32WB55 bring-up image.
 *
 * This is an EXPERIMENTAL native image for DFU packaging and recovery testing.
 * It is NOT a full GrokLink OS service stack (no USB RPC, radio, UI yet).
 *
 * After flash the stock Flipper UI will be gone until you reinstall a full
 * firmware DFU (GrokLink-Firmware v2.1.3, Momentum, or official).
 *
 * Identity string is placed in flash for dump/verify.
 */
#include <stdint.h>
#include <stddef.h>

#define GLK_BRINGUP_VERSION "3.0.0-bringup"
#define GLK_BRINGUP_BANNER \
    "GrokLink OS " GLK_BRINGUP_VERSION " — native STM32WB55 bring-up (not full services)"

/* Keep in .rodata so `strings` / hex dump can find it */
__attribute__((used, section(".rodata.glk")))
const char glk_os_identity[] = GLK_BRINGUP_BANNER;
__attribute__((used, section(".rodata.glk")))
const char glk_os_build_tag[] = "GROKLINK_OS_DFU_V3";

/* Flipper F7: PC6 often used as display backlight-ish / power rails vary.
 * We toggle a few common GPIO clocks and pins safely as outputs without
 * assuming a specific LED mapping — prevents hardfault from bad MMIO. */

#define RCC_AHB2ENR   (*(volatile uint32_t *)0x5800004Cu)
#define GPIOA_BASE    0x48000000u
#define GPIOB_BASE    0x48000400u
#define GPIOC_BASE    0x48000800u

static void gpio_enable_clocks(void) {
    RCC_AHB2ENR |= (1u << 0) | (1u << 1) | (1u << 2); /* A B C */
    /* wait */
    volatile uint32_t t = RCC_AHB2ENR;
    (void)t;
}

static void gpio_pin_out(uint32_t gpio_base, unsigned pin) {
    volatile uint32_t *moder = (volatile uint32_t *)(gpio_base + 0x00u);
    volatile uint32_t *otyper = (volatile uint32_t *)(gpio_base + 0x04u);
    uint32_t m = *moder;
    m &= ~(3u << (pin * 2));
    m |= (1u << (pin * 2)); /* output */
    *moder = m;
    *otyper &= ~(1u << pin); /* push-pull */
}

static void gpio_write(uint32_t gpio_base, unsigned pin, int high) {
    volatile uint32_t *bsrr = (volatile uint32_t *)(gpio_base + 0x18u);
    if (high) {
        *bsrr = (1u << pin);
    } else {
        *bsrr = (1u << (pin + 16));
    }
}

static void delay(volatile uint32_t n) {
    while (n--) {
        __asm volatile("nop");
    }
}

/* Magic region for host-side verification of flashed images */
typedef struct {
    uint32_t magic;      /* 'GLK3' */
    uint32_t version;    /* 0x00030000 */
    uint32_t flash_addr; /* 0x08000000 */
    char tag[16];
    char product[32];
    char build_id[24];
} glk_dfu_header_t;

__attribute__((section(".glk_meta"), used))
const glk_dfu_header_t glk_dfu_meta = {
    .magic = 0x334B4C47u, /* 'GLK3' little-endian as G L K 3 */
    .version = 0x00030000u,
    .flash_addr = 0x08000000u,
    .tag = "OS3-BRINGUP",
    .product = "GrokLink OS 3.0.0",
    .build_id = "GROKLINK_OS_DFU_V3",
};

int main(void) {
    gpio_enable_clocks();
    /* Toggle PC4 (RF_SW0 on F7) and PA1 as activity markers — safe outputs */
    gpio_pin_out(GPIOC_BASE, 4);
    gpio_pin_out(GPIOA_BASE, 1);

    /* Prevent unused-symbol stripping of identity */
    volatile const char *keep = glk_os_identity;
    (void)keep;
    (void)glk_os_build_tag;
    (void)glk_dfu_meta;

    for (;;) {
        gpio_write(GPIOC_BASE, 4, 1);
        gpio_write(GPIOA_BASE, 1, 1);
        delay(400000u);
        gpio_write(GPIOC_BASE, 4, 0);
        gpio_write(GPIOA_BASE, 1, 0);
        delay(400000u);
    }
    return 0;
}
