/**
 * STM32WB55 SPI_R + CC1101 GPIO board glue (Flipper F7 pinout).
 *
 * Pins (F7 resources):
 *   CS PD0, G0 PA1, RF_SW0 PC4
 *   SCK PA5, MISO PB4, MOSI PB5
 *
 * Uses **bit-bang SPI** (mode 0) for reliable CC1101 bring-up on HSI16.
 * Hardware SPI1 hung RXNE on earlier builds and starved the USB poll loop.
 *
 * After CS↓ wait for MISO low (CHIP_RDYn) before clocking — TI + Flipper.
 */
#include "glk_board_spi_r.h"
#include "board/pins_f7.h"
#include "stm32_compat.h"

#include <stdint.h>
#include <stdbool.h>

#if defined(GLK_PLATFORM_STM32)

static volatile uint32_t s_g0_edges;
static volatile bool s_g0_irq_on;
static bool s_spi_ready;
static glk_board_usb_pump_fn s_usb_pump;

void glk_board_set_usb_pump(glk_board_usb_pump_fn fn) {
    s_usb_pump = fn;
}

/* ---- GPIO ---- */

static void gpio_enable_clocks(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN |
                    RCC_AHB2ENR_GPIODEN;
    (void)RCC->AHB2ENR;
    (void)GPIOA->MODER;
    (void)GPIOB->MODER;
    (void)GPIOC->MODER;
    (void)GPIOD->MODER;
}

static void pin_out_pp(GPIO_TypeDef* port, unsigned pin, bool high) {
    port->OTYPER &= ~(1u << pin);
    port->OSPEEDR = (port->OSPEEDR & ~(3u << (pin * 2u))) | (3u << (pin * 2u));
    port->PUPDR &= ~(3u << (pin * 2u));
    if (high) {
        port->BSRR = (1u << pin);
    } else {
        port->BSRR = (1u << (pin + 16u));
    }
    port->MODER = (port->MODER & ~(3u << (pin * 2u))) | (1u << (pin * 2u));
}

static void pin_in_float(GPIO_TypeDef* port, unsigned pin) {
    port->PUPDR &= ~(3u << (pin * 2u));
    port->MODER &= ~(3u << (pin * 2u));
}

static void pin_in_pu(GPIO_TypeDef* port, unsigned pin) {
    port->PUPDR = (port->PUPDR & ~(3u << (pin * 2u))) | (1u << (pin * 2u)); /* pull-up */
    port->MODER &= ~(3u << (pin * 2u));
}

void glk_board_delay_us(uint32_t us) {
    if (us == 0u) return;
    /* HSI16: ~16 cycles/µs; empty loop ~3–4 cycles → *4 */
    for (volatile uint32_t i = 0; i < us * 4u; i++) {
    }
}

static void sck_lo(void) {
    GPIOA->BSRR = (1u << (5u + 16u));
}
static void sck_hi(void) {
    GPIOA->BSRR = (1u << 5u);
}
static void mosi_write(bool hi) {
    if (hi) {
        GPIOB->BSRR = (1u << 5u);
    } else {
        GPIOB->BSRR = (1u << (5u + 16u));
    }
}
static bool miso_read(void) {
    return (GPIOB->IDR & (1u << 4u)) != 0u;
}

/** Wait CHIP_RDYn (SO low). Bounded; do not call USB from here (reentrancy). */
static bool wait_chip_ready(void) {
    for (uint32_t i = 0; i < 100u; i++) {
        if (!miso_read()) {
            return true;
        }
        glk_board_delay_us(1);
    }
    return !miso_read();
}

void glk_board_cc1101_cs(bool assert_low) {
    if (assert_low) {
        GPIOD->BSRR = (1u << (0u + 16u)); /* PD0 low */
        /* Short settle only — do not spin forever on CHIP_RDYn (can brick USB path). */
        glk_board_delay_us(10);
        (void)wait_chip_ready(); /* bounded ~100 µs */
    } else {
        sck_lo();
        GPIOD->BSRR = (1u << 0u); /* PD0 high */
        glk_board_delay_us(2);
    }
}

uint8_t glk_board_spi_r_xfer(uint8_t b) {
    if (!s_spi_ready) {
        return 0xFF;
    }
    uint8_t rx = 0;
    /* Mode 0: SCK idle low, sample on rising, change on falling. ~1 MHz bitbang. */
    sck_lo();
    for (int i = 7; i >= 0; i--) {
        mosi_write((b >> i) & 1u);
        for (volatile int d = 0; d < 2; d++) {
        }
        sck_hi();
        for (volatile int d = 0; d < 2; d++) {
        }
        if (miso_read()) {
            rx |= (uint8_t)(1u << i);
        }
        sck_lo();
    }
    return rx;
}

void glk_board_rf_sw0(bool high) {
    if (high) {
        GPIOC->BSRR = (1u << 4u);
    } else {
        GPIOC->BSRR = (1u << (4u + 16u));
    }
}

bool glk_board_g0_read(void) {
    return (GPIOA->IDR & (1u << 1u)) != 0u;
}

void glk_board_g0_irq_enable(bool on) {
    s_g0_irq_on = on;
    if (!on) {
        EXTI->IMR1 &= ~EXTI_IMR1_IM1;
        NVIC_DisableIRQ(EXTI1_IRQn);
        return;
    }
    s_g0_edges = 0;
    SYSCFG->EXTICR[0] =
        (SYSCFG->EXTICR[0] & ~SYSCFG_EXTICR1_EXTI1) | SYSCFG_EXTICR1_EXTI1_PA;
    EXTI->RTSR1 |= EXTI_RTSR1_RT1;
    EXTI->FTSR1 |= EXTI_FTSR1_FT1;
    EXTI->PR1 = EXTI_PR1_PIF1;
    EXTI->IMR1 |= EXTI_IMR1_IM1;
    NVIC_SetPriority(EXTI1_IRQn, 2);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

uint32_t glk_board_g0_edge_count_take(void) {
    uint32_t v = s_g0_edges;
    s_g0_edges = 0;
    return v;
}

uint32_t glk_board_g0_poll_edges_ms(uint32_t duration_ms) {
    if (duration_ms == 0u) duration_ms = 1u;
    /* Cap so USB is not starved for multi-second RX windows on device */
    if (duration_ms > 100u) duration_ms = 100u;
    uint32_t edges = 0;
    bool last = glk_board_g0_read();
    for (uint32_t ms = 0; ms < duration_ms; ms++) {
        /* Keep CDC alive while bit-banging G0 (offline agent / RPC RX). */
        if (s_usb_pump) {
            s_usb_pump();
        }
        for (uint32_t k = 0; k < 200u; k++) {
            bool level = glk_board_g0_read();
            if (level != last) {
                edges++;
                last = level;
            }
            for (volatile uint32_t d = 0; d < 4u; d++) {
            }
        }
    }
    return edges;
}

void EXTI1_IRQHandler(void) {
    if (EXTI->PR1 & EXTI_PR1_PIF1) {
        EXTI->PR1 = EXTI_PR1_PIF1;
        if (s_g0_irq_on) {
            s_g0_edges++;
        }
    }
}

void glk_board_spi_r_init(void) {
    if (s_spi_ready) return;

    gpio_enable_clocks();

    /* CS high idle, RF SW0 low, GDO0 input — never touch PA11/PA12 (USB FS). */
    pin_out_pp(GPIOD, 0, true);
    pin_out_pp(GPIOC, 4, false);
    pin_in_float(GPIOA, 1);

    /* Bit-bang: SCK/MOSI out low, MISO in (pull-up helps idle high when CS high) */
    pin_out_pp(GPIOA, 5, false); /* SCK low — PA5 only */
    pin_out_pp(GPIOB, 5, false); /* MOSI low */
    pin_in_pu(GPIOB, 4);         /* MISO */

    /* Belt-and-suspenders: keep USB pins as AF10 after any AHB2 GPIO clock churn */
    extern void glk_usb_pins_restore(void);
    glk_usb_pins_restore();

    s_g0_edges = 0;
    s_g0_irq_on = false;
    s_spi_ready = true;

    glk_board_delay_us(100);
    /* Wake pulse */
    GPIOD->BSRR = (1u << (0u + 16u));
    glk_board_delay_us(50);
    (void)wait_chip_ready();
    GPIOD->BSRR = (1u << 0u);
    glk_board_delay_us(50);
}

#else
void glk_board_spi_r_init(void) {
}
void glk_board_cc1101_cs(bool assert_low) {
    (void)assert_low;
}
uint8_t glk_board_spi_r_xfer(uint8_t b) {
    return b;
}
void glk_board_delay_us(uint32_t us) {
    (void)us;
}
void glk_board_rf_sw0(bool high) {
    (void)high;
}
void glk_board_g0_irq_enable(bool on) {
    (void)on;
}
uint32_t glk_board_g0_edge_count_take(void) {
    return 0;
}
uint32_t glk_board_g0_poll_edges_ms(uint32_t duration_ms) {
    (void)duration_ms;
    return 0;
}
bool glk_board_g0_read(void) {
    return false;
}
#endif
