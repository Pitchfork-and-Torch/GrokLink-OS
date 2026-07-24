#include "glk_svc/glk_power.h"
#include "glk/glk_kernel.h"

static glk_power_mode_t s_mode = GLK_PWR_RUN;

glk_err_t glk_power_init(void) {
    s_mode = GLK_PWR_RUN;
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
    } else {
        glk_task_sleep_ms(ms > 10 ? 10 : ms);
    }
    return GLK_OK;
}

int glk_power_battery_pct(void) {
    return 100; /* host sim */
}
