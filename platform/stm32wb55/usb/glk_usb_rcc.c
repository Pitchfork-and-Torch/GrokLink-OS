/**
 * STM32WB55 clocks for USB FS (HSI16 CPU + HSI48 USB + CRS).
 * Based on libusb_stm32 demo cdc_startup STM32WB55xx path (Apache-2.0 stack).
 *
 * USB FS requires 48 MHz within ±0.25%. Bare HSI48 can drift outside that;
 * CRS auto-trims from USB SOF once the bus is active.
 */
#include "stm32_compat.h"

/** PA11/PA12 AF10 = USB DM/DP. Safe to call again after other GPIO bring-up. */
void glk_usb_pins_restore(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;
    /* MODER: alternate function on pin 11 and 12 */
    GPIOA->MODER = (GPIOA->MODER & ~((3u << 22) | (3u << 24))) | ((2u << 22) | (2u << 24));
    /* OSPEED high for USB edges */
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~((3u << 22) | (3u << 24))) | ((3u << 22) | (3u << 24));
    GPIOA->PUPDR &= ~((3u << 22) | (3u << 24));
    GPIOA->OTYPER &= ~((1u << 11) | (1u << 12));
    /* AFRH: AF10 for PA11 and PA12 */
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((0xFu << 12) | (0xFu << 16))) | ((0xAu << 12) | (0xAu << 16));
}

/** Coarse busy-wait (HSI16 ≈ 16 MHz). */
void glk_usb_delay_ms(uint32_t ms) {
    while (ms--) {
        for (volatile uint32_t i = 0; i < 4000u; i++) {
        }
    }
}

void glk_usb_rcc_init(void) {
    /* HSI16 as CPU clock */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {
    }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (0x01UL << RCC_CFGR_SW_Pos);
    while (((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos) != 0x01UL) {
    }

    /* HSI48 for USB PHY */
    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    while ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) == 0) {
    }
    /* CLK48SEL = 0 → HSI48 */
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_CLK48SEL);

    /*
     * CRS: trim HSI48 from USB SOF (1 kHz).
     * RELOAD = (48e6 / 1000) - 1 = 47999
     * FELIM  = 34 (ST recommended for USB)
     * SYNCSRC = USB SOF (CRS_CFGR_SYNCSRC_1)
     */
    RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN;
    (void)RCC->APB1ENR1;
    CRS->CFGR = (34u << CRS_CFGR_FELIM_Pos) | (47999u << CRS_CFGR_RELOAD_Pos) | CRS_CFGR_SYNCSRC_1;
    CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;

    glk_usb_pins_restore();

    /* Vddusb valid (WB55 PWR on AHB3/AHB4 — CR2 always available when powered) */
    PWR->CR2 |= PWR_CR2_USV;

    /* Flash latency for 16 MHz */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_0WS;
}

void SystemInit(void) {
    SCB->VTOR = 0x08000000u;
    /* FPU */
    SCB->CPACR |= (0xFu << 20);
    glk_usb_rcc_init();
}
