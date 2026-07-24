/**
 * Minimal SystemInit for STM32WB55 — MSI clock, no full CubeMX tree.
 * Enough to run a bring-up image; not production power/clock config.
 */
#include <stdint.h>

/* CMSIS-style register bases (public RM0434) */
#define RCC_BASE        0x58000000u
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x00u))
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x08u))
#define FLASH_BASE_REG  0x58004000u
#define FLASH_ACR       (*(volatile uint32_t *)(FLASH_BASE_REG + 0x00u))
#define SCB_VTOR        (*(volatile uint32_t *)0xE000ED08u)

extern uint32_t g_pfnVectors;

void SystemInit(void) {
    /* Vector table in flash */
    SCB_VTOR = (uint32_t)&g_pfnVectors;

    /* Enable FPU coprocessors CP10/CP11 */
    #define SCB_CPACR (*(volatile uint32_t *)0xE000ED88u)
    SCB_CPACR |= (0xFu << 20);

    /* Leave MSI as default wake clock; set flash latency 1WS conservatively */
    FLASH_ACR = (FLASH_ACR & ~0xFu) | 0x1u;

    /* Ensure MSI on */
    RCC_CR |= (1u << 0);
    while ((RCC_CR & (1u << 1)) == 0) {
        /* wait MSIRDY */
    }
}
