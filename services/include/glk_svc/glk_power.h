/**
 * Mission-aware power management hooks.
 */
#pragma once

#include "glk/glk_types.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLK_PWR_RUN = 0,
    GLK_PWR_LISTEN_DUTY = 1,
    GLK_PWR_MISSION_IDLE = 2,
    GLK_PWR_DEEP_SLEEP = 3,
} glk_power_mode_t;

glk_err_t glk_power_init(void);
glk_err_t glk_power_set_mode(glk_power_mode_t mode);
glk_power_mode_t glk_power_mode(void);
const char* glk_power_mode_str(glk_power_mode_t m);

/** Host: no-op sleep; target: STOP/Standby + RTC. */
glk_err_t glk_power_sleep_ms(uint32_t ms);

/** Battery percent estimate 0-100; host returns 100 unless GLK_BATTERY_PCT set. */
int glk_power_battery_pct(void);
void glk_power_set_battery_pct(int pct);

/** Soft RTC wake schedule (host: tick compare; device: RTC alarm stub). */
glk_err_t glk_power_schedule_rtc_wake_ms(uint32_t from_now_ms);
bool glk_power_rtc_wake_due(void);

size_t glk_power_status_json(char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif
