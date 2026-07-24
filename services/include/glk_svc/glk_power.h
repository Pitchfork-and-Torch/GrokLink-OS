/**
 * Mission-aware power management hooks.
 */
#pragma once

#include "glk/glk_types.h"

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

/** Host: no-op sleep; target: STOP/Standby + RTC. */
glk_err_t glk_power_sleep_ms(uint32_t ms);

/** Battery percent estimate 0-100; host returns 100. */
int glk_power_battery_pct(void);

#ifdef __cplusplus
}
#endif
