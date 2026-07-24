#include "glk_drv/glk_nfc.h"
#include "glk_svc/glk_policy.h"
#include <string.h>

glk_err_t glk_nfc_init(void) { return GLK_OK; }

glk_err_t glk_nfc_poll(glk_actor_t actor, uint8_t* uid, size_t cap, size_t* out_len) {
    glk_policy_state_t* pol = glk_policy_global();
    glk_policy_request_t req;
    memset(&req, 0, sizeof(req));
    req.actor = actor;
    req.action = "nfc_poll";
    req.risk = GLK_RISK_PASSIVE_RX;
    req.gpio_pin = -1;
    if (pol) {
        glk_policy_decision_t d = glk_policy_check(pol, &req);
        if (d.result != GLK_POLICY_ALLOW) return GLK_ERR_DENIED;
    }
    if (!uid || cap < 4) return GLK_ERR_INVAL;
    uid[0] = 0x04;
    uid[1] = 0xDE;
    uid[2] = 0xAD;
    uid[3] = 0x01;
    if (out_len) *out_len = 4;
    return GLK_OK;
}
