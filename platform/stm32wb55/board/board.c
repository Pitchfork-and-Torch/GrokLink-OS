#include "board.h"

/* Stubs until linked with CMSIS device pack. */
void glk_board_early_init(void) {}
void glk_board_clock_init(void) {}
void glk_board_gpio_init(void) {}
uint32_t glk_board_random_u32(void) { return 0xA5A5A5A5u; }
