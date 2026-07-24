/**
 * Board pin map: Flipper Zero F7 class (STM32WB55).
 *
 * Documented for GrokLink OS native bring-up so the CC1101 / SPI / RF path
 * match production Flipper Zero hardware. Values align with open F7 target
 * resources (CC1101 on SPI_R bus).
 *
 * Not a copy of Flipper firmware — pin numbers only for electrical compatibility.
 */
#pragma once

/* ---- CC1101 (SubGHz) ---- */
#define GLK_PIN_CC1101_CS_PORT   'D'
#define GLK_PIN_CC1101_CS_NUM    0   /* PD0 */
#define GLK_PIN_CC1101_G0_PORT   'A'
#define GLK_PIN_CC1101_G0_NUM    1   /* PA1 — async data / GDO0 */

/* RF path switch (antenna matching network) */
#define GLK_PIN_RF_SW0_PORT      'C'
#define GLK_PIN_RF_SW0_NUM       4   /* PC4 */

/* SPI_R — radio bus (SPI1 on WB55 F7 design) */
#define GLK_PIN_SPI_R_SCK_PORT   'A'
#define GLK_PIN_SPI_R_SCK_NUM    5   /* PA5 */
#define GLK_PIN_SPI_R_MISO_PORT  'B'
#define GLK_PIN_SPI_R_MISO_NUM   4   /* PB4 */
#define GLK_PIN_SPI_R_MOSI_PORT  'B'
#define GLK_PIN_SPI_R_MOSI_NUM   5   /* PB5 */

/* SPI_D — display / shared peripherals (reference) */
#define GLK_PIN_SPI_D_SCK_PORT   'D'
#define GLK_PIN_SPI_D_SCK_NUM    1
#define GLK_PIN_SPI_D_MISO_PORT  'C'
#define GLK_PIN_SPI_D_MISO_NUM   2
#define GLK_PIN_SPI_D_MOSI_PORT  'B'
#define GLK_PIN_SPI_D_MOSI_NUM   15

/* Frequency path bands (Hz centers for matching network selection) */
#define GLK_RF_PATH_315_MAX_HZ   350000000u
#define GLK_RF_PATH_433_MAX_HZ   650000000u
/* above 650 MHz -> 868/915 path */

typedef enum {
    GLK_RF_PATH_ISOLATE = 0,
    GLK_RF_PATH_315 = 1,
    GLK_RF_PATH_433 = 2,
    GLK_RF_PATH_868 = 3,
} glk_rf_path_t;
