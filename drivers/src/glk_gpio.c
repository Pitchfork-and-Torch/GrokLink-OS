/**
 * GPIO driver — all outputs gated by policy.
 */
#include "glk_drv/glk_gpio.h"
#include "glk_svc/glk_policy.h"

#include <string.h>

static uint8_t s_pins[64];

glk_err_t glk_gpio_init(void) {
    memset(s_pins, 0, sizeof(s_pins));
    return GLK_OK;
}

glk_err_t glk_gpio_read(int32_t pin, int* level) {
    if (pin < 0 || pin >= 64 || !level) return GLK_ERR_INVAL;
    *level = s_pins[pin] ? 1 : 0;
    return GLK_OK;
}

glk_err_t glk_gpio_write(glk_actor_t actor, int32_t pin, int level, const char* confirm_id) {
    glk_policy_state_t* pol = glk_policy_global();
    if (!pol) return GLK_ERR_DENIED;
    glk_policy_request_t req;
    memset(&req, 0, sizeof(req));
    req.actor = actor;
    req.action = "gpio_write";
    req.risk = GLK_RISK_GPIO;
    req.gpio_pin = pin;
    req.freq_hz = 0;
    req.confirm_id = confirm_id;
    glk_policy_decision_t d = glk_policy_check(pol, &req);
    if (d.result != GLK_POLICY_ALLOW) return GLK_ERR_DENIED;
    if (pin < 0 || pin >= 64) return GLK_ERR_INVAL;
    s_pins[pin] = level ? 1 : 0;
    return GLK_OK;
}
