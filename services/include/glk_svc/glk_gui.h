/**
 * On-device GUI - status screens for F7 128×64 display.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLK_GUI_PAGE_HOME = 0,
    GLK_GUI_PAGE_RADIO = 1,
    GLK_GUI_PAGE_SAFETY = 2,
    GLK_GUI_PAGE_STORAGE = 3,
    GLK_GUI_PAGE_ABOUT = 4,
    GLK_GUI_PAGE_COUNT = 5
} glk_gui_page_t;

void glk_gui_init(void);
/** Call from main loop; samples keys and refreshes display ~5 - 10 Hz. */
void glk_gui_poll(void);
void glk_gui_set_edu(bool on);
void glk_gui_set_radio_line(const char* line);
void glk_gui_notify_rx(int pulses, int rssi, bool sim);
/** Offline field explorer status lines (unplugged autonomy). */
void glk_gui_set_offline(bool on, const char* mission_id);
/** v3.8: storage / skills / vault / host-mode lines for ST7567. */
void glk_gui_set_storage(bool present, const char* mode_str);
void glk_gui_set_skills(unsigned count);
void glk_gui_set_vault(unsigned count, bool persistent, bool sealed);
void glk_gui_set_usb_host(bool host_active);
/**
 * Latched when operator arms field explore from GUI:
 * SAFETY page + hold OK ~2 s (physical ack of authorized research).
 * Cleared when read.
 */
bool glk_gui_consume_field_explore(void);

#ifdef __cplusplus
}
#endif
