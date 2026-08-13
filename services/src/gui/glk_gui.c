/**
 * Minimal multi-page on-device GUI (authorized research branding).
 * Field explore: SAFETY page + hold OK ~2s arms unplugged passive explorer.
 */
#include "glk_svc/glk_gui.h"
#include "glk/glk_config.h"

#include <stdio.h>
#include <string.h>

#if defined(GLK_PLATFORM_STM32) && !defined(GLK_PLATFORM_HOST)
#include "glk_board_display.h"
#define GLK_GUI_HW 1
#else
#define GLK_GUI_HW 0
#endif

static glk_gui_page_t s_page;
static bool s_edu;
static bool s_offline;
static char s_radio[22];
static char s_last_rx[22];
static char s_offline_line[22];
static char s_stor_line[22];
static char s_skill_line[22];
static char s_vault_line[22];
static char s_host_line[22];
static uint32_t s_tick;
static bool s_inited;
static bool s_hw_ready;
static bool s_field_explore_latched;
static uint32_t s_ok_hold_ticks;

#if GLK_GUI_HW
static glk_keys_t s_prev;
#endif

void glk_gui_init(void) {
    /* Defer ST7567/key GPIO until after USB enumerates (boot must stay light). */
    s_page = GLK_GUI_PAGE_HOME;
    s_edu = false;
    s_offline = false;
    snprintf(s_radio, sizeof(s_radio), "radio: idle");
    snprintf(s_last_rx, sizeof(s_last_rx), "rx: --");
    snprintf(s_offline_line, sizeof(s_offline_line), "field: off");
    snprintf(s_stor_line, sizeof(s_stor_line), "SD: absent");
    snprintf(s_skill_line, sizeof(s_skill_line), "skills: 0");
    snprintf(s_vault_line, sizeof(s_vault_line), "vault: 0");
    snprintf(s_host_line, sizeof(s_host_line), "mode: field");
    s_tick = 0;
    s_hw_ready = false;
    s_field_explore_latched = false;
    s_ok_hold_ticks = 0;
    s_inited = true;
#if GLK_GUI_HW
    memset(&s_prev, 0, sizeof(s_prev));
#endif
}

void glk_gui_set_edu(bool on) {
    s_edu = on;
}

void glk_gui_set_radio_line(const char* line) {
    if (!line) return;
    snprintf(s_radio, sizeof(s_radio), "%s", line);
}

void glk_gui_notify_rx(int pulses, int rssi, bool sim) {
    snprintf(s_last_rx, sizeof(s_last_rx), "rx p=%d r=%d%s", pulses, rssi, sim ? "S" : "");
}

void glk_gui_set_offline(bool on, const char* mission_id) {
    s_offline = on;
    if (on && mission_id && mission_id[0]) {
        snprintf(s_offline_line, sizeof(s_offline_line), "FIELD %s", mission_id);
        /* truncate for 128px */
        s_offline_line[20] = 0;
    } else if (on) {
        snprintf(s_offline_line, sizeof(s_offline_line), "FIELD exploring");
    } else {
        snprintf(s_offline_line, sizeof(s_offline_line), "field: off");
    }
}

void glk_gui_set_storage(bool present, const char* mode_str) {
    if (present && mode_str && mode_str[0])
        snprintf(s_stor_line, sizeof(s_stor_line), "SD: %s", mode_str);
    else if (present)
        snprintf(s_stor_line, sizeof(s_stor_line), "SD: present");
    else
        snprintf(s_stor_line, sizeof(s_stor_line), "SD: absent");
    s_stor_line[20] = 0;
}

void glk_gui_set_skills(unsigned count) {
    snprintf(s_skill_line, sizeof(s_skill_line), "skills: %u", count);
    s_skill_line[20] = 0;
}

void glk_gui_set_vault(unsigned count, bool persistent, bool sealed) {
    snprintf(
        s_vault_line,
        sizeof(s_vault_line),
        "vault:%u%s%s",
        count,
        persistent ? "P" : "",
        sealed ? "S" : "");
    s_vault_line[20] = 0;
}

void glk_gui_set_usb_host(bool host_active) {
    snprintf(s_host_line, sizeof(s_host_line), host_active ? "USB host active" : "mode: field");
    s_host_line[20] = 0;
}

bool glk_gui_consume_field_explore(void) {
    if (!s_field_explore_latched) return false;
    s_field_explore_latched = false;
    return true;
}

void glk_gui_poll(void) {
    if (!s_inited) return;
    s_tick++;

#if GLK_GUI_HW
    /* Defer display/key GPIO until long after USB bind (avoid COM "not functioning"). */
    if (!s_hw_ready) {
        if (s_tick < 30000u) return;
        glk_display_init();
        glk_keys_init();
        s_hw_ready = true;
    }

    glk_keys_t k;
    glk_keys_sample(&k);
    if (k.right && !s_prev.right) {
        s_page = (glk_gui_page_t)(((int)s_page + 1) % (int)GLK_GUI_PAGE_COUNT);
    }
    if (k.left && !s_prev.left) {
        s_page = (glk_gui_page_t)(((int)s_page + (int)GLK_GUI_PAGE_COUNT - 1) % (int)GLK_GUI_PAGE_COUNT);
    }

    /* SAFETY page: hold OK ~2s (~800 poll ticks at main loop rate) for field explore */
    if (s_page == GLK_GUI_PAGE_SAFETY && k.ok) {
        s_ok_hold_ticks++;
        /* glk_gui_poll runs every loop; ~600ms at ~1kHz loop ≈ 600 ticks - use 1200 for ~1 - 2s */
        if (s_ok_hold_ticks >= 1200u && !s_prev.ok) {
            /* edge from not-held long enough handled by count while held */
        }
        if (s_ok_hold_ticks == 1200u) {
            s_field_explore_latched = true;
        }
    } else {
        s_ok_hold_ticks = 0;
    }

    s_prev = k;

    /* ~4 Hz refresh (display bit-bang is heavy) */
    if ((s_tick & 15u) != 0u && s_tick != 200u) return;

    glk_display_clear();
    glk_display_draw_str(0, 0, "GrokLink OS " GLK_VERSION_STRING);
    glk_display_draw_hline(0, 9, 128);

    switch (s_page) {
    case GLK_GUI_PAGE_HOME:
        glk_display_draw_str(0, 14, "HOME  < > pages");
#if GLK_BOOT_FIELD_EXPLORE
        glk_display_draw_str(0, 24, "FIELD RESEARCH unit");
        glk_display_draw_str(0, 34, s_offline ? s_offline_line : "arming...");
        glk_display_draw_str(0, 44, "passive always-on");
        glk_display_draw_str(0, 54, "no auto-TX");
#else
        glk_display_draw_str(0, 24, s_edu ? "edu: ACKED" : "edu: needed");
        glk_display_draw_str(0, 34, s_offline ? s_offline_line : "USB or FIELD mode");
        glk_display_draw_str(0, 44, "auth research only");
        glk_display_draw_str(0, 54, "SAFETY: hold OK=field");
#endif
        break;
    case GLK_GUI_PAGE_RADIO:
        glk_display_draw_str(0, 14, "RADIO");
        glk_display_draw_str(0, 26, s_radio);
        glk_display_draw_str(0, 38, s_last_rx);
        glk_display_draw_str(0, 50, s_offline ? "unplugged OK" : "CC1101 SPI_R");
        break;
    case GLK_GUI_PAGE_SAFETY:
        glk_display_draw_str(0, 14, "SAFETY");
        glk_display_draw_str(0, 26, "default-deny TX");
        glk_display_draw_str(0, 36, "HOLD OK ~2s:");
        glk_display_draw_str(0, 46, s_offline ? "FIELD ARMED ok" : "arm FIELD explore");
        glk_display_draw_str(0, 56, s_offline ? "FIELD ACTIVE" : "NOT medical");
        break;
    case GLK_GUI_PAGE_STORAGE:
        glk_display_draw_str(0, 14, "STORAGE v3.8");
        glk_display_draw_str(0, 26, s_stor_line);
        glk_display_draw_str(0, 36, s_skill_line);
        glk_display_draw_str(0, 46, s_vault_line);
        glk_display_draw_str(0, 56, s_host_line);
        break;
    case GLK_GUI_PAGE_ABOUT:
    default:
        glk_display_draw_str(0, 14, "ABOUT");
        glk_display_draw_str(0, 26, "NOT medical dev");
        glk_display_draw_str(0, 36, "MedSec research");
        glk_display_draw_str(0, 46, "auth targets only");
        glk_display_draw_str(0, 56, "MIT / no care use");
        break;
    }
    glk_display_flush();
#else
    (void)s_page;
    (void)s_edu;
    (void)s_hw_ready;
    (void)s_offline;
#endif
}
