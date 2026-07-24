/**
 * Flipper F7 ST7567 display (128×64) + 5-way buttons.
 * Electrical pin map only (open F7 resources).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLK_DISP_W 128
#define GLK_DISP_H 64

void glk_display_init(void);
void glk_display_clear(void);
void glk_display_flush(void);
void glk_display_set_pixel(int x, int y, bool on);
void glk_display_draw_str(int x, int y, const char* s);
void glk_display_draw_hline(int x, int y, int w);
void glk_display_invert_rect(int x, int y, int w, int h);

/** Active-low keys on F7 (true = pressed). */
typedef struct {
    bool up, down, left, right, ok, back;
} glk_keys_t;

void glk_keys_init(void);
void glk_keys_sample(glk_keys_t* out);

#ifdef __cplusplus
}
#endif
