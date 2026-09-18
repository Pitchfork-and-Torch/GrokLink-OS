/**
 * Mission-aware power management hooks (host sim + device stubs).
 * Modes: run / listen_duty / mission_idle / deep_sleep.
 */
#include "glk_svc/glk_power.h"
#include "glk/glk_kernel.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static glk_power_mode_t s_mode = GLK_PWR_RUN;
static int s_battery_pct = 100;
static uint32_t s_rtc_wake_ms;

glk_err_t glk_power_init(void) {
    s_mode = GLK_PWR_RUN;
    s_battery_pct = 100;
    s_rtc_wake_ms = 0;
    const char* env = getenv("GLK_BATTERY_PCT");
    if (env) s_battery_pct = atoi(env);
    return GLK_OK;
}

glk_err_t glk_power_set_mode(glk_power_mode_t mode) {
    s_mode = mode;
    return GLK_OK;
}

glk_power_mode_t glk_power_mode(void) {
    return s_mode;
}

glk_err_t glk_power_sleep_ms(uint32_t ms) {
    if (s_mode == GLK_PWR_DEEP_SLEEP || s_mode == GLK_PWR_MISSION_IDLE) {
        glk_task_sleep_ms(ms);
    } else if (s_mode == GLK_PWR_LISTEN_DUTY) {
        /* duty: longer sleeps between listen windows */
        glk_task_sleep_ms(ms > 50 ? ms : 50);
    } else {
        glk_task_sleep_ms(ms > 10 ? 10 : ms);
    }
    return GLK_OK;
}

int glk_power_battery_pct(void) {
    return s_battery_pct;
}

/** Host/test: inject battery for deferral tests. */
void glk_power_set_battery_pct(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    s_battery_pct = pct;
}

glk_err_t glk_power_schedule_rtc_wake_ms(uint32_t from_now_ms) {
    s_rtc_wake_ms = glk_tick_get() + from_now_ms;
    return GLK_OK;
}

bool glk_power_rtc_wake_due(void) {
    if (s_rtc_wake_ms == 0) return false;
    return glk_tick_get() >= s_rtc_wake_ms;
}

const char* glk_power_mode_str(glk_power_mode_t m) {
    switch (m) {
    case GLK_PWR_LISTEN_DUTY: return "listen_duty";
    case GLK_PWR_MISSION_IDLE: return "mission_idle";
    case GLK_PWR_DEEP_SLEEP: return "deep_sleep";
    case GLK_PWR_RUN:
    default: return "run";
    }
}

size_t glk_power_status_json(char* buf, size_t buflen) {
    if (!buf || buflen < 8) return 0;
    return (size_t)snprintf(
        buf,
        buflen,
        "{\"mode\":\"%s\",\"battery_pct\":%d,\"rtc_wake_ms\":%u}",
        glk_power_mode_str(s_mode),
        s_battery_pct,
        (unsigned)s_rtc_wake_ms);
}
