#include "glk_drv/glk_ir.h"
#include "glk_svc/glk_policy.h"
#include "glk/glk_kernel.h"
#include <string.h>

glk_err_t glk_ir_init(void) { return GLK_OK; }

glk_err_t glk_ir_rx(glk_actor_t actor, uint32_t ms, int32_t* edges) {
    glk_policy_state_t* pol = glk_policy_global();
    glk_policy_request_t req;
    memset(&req, 0, sizeof(req));
    req.actor = actor;
    req.action = "ir_rx";
    req.risk = GLK_RISK_PASSIVE_RX;
    req.gpio_pin = -1;
    if (pol) {
        glk_policy_decision_t d = glk_policy_check(pol, &req);
        if (d.result != GLK_POLICY_ALLOW) return GLK_ERR_DENIED;
    }
    glk_task_sleep_ms(ms > 50 ? 50 : ms);
    if (edges) *edges = (int32_t)(ms % 17);
    return GLK_OK;
}

glk_err_t glk_ir_tx(glk_actor_t actor, const char* confirm_id, const uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    glk_policy_state_t* pol = glk_policy_global();
    if (!pol) return GLK_ERR_DENIED;
    glk_policy_request_t req;
    memset(&req, 0, sizeof(req));
    req.actor = actor;
    req.action = "ir_tx";
    req.risk = GLK_RISK_ACTIVE_TX;
    req.gpio_pin = -1;
    req.confirm_id = confirm_id;
    glk_policy_decision_t d = glk_policy_check(pol, &req);
    if (d.result != GLK_POLICY_ALLOW) return GLK_ERR_DENIED;
    return GLK_OK;
}
