#include "glk_svc/glk_policy.h"
#include "glk_svc/glk_audit.h"
#include "glk/glk_kernel.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    glk_kernel_init();
    glk_audit_init("test_audit.jsonl");
    glk_policy_state_t st;
    assert(glk_policy_init(&st) == GLK_OK);
    glk_policy_set_global(&st);
    glk_policy_set_sd_present(&st, true);
    st.blacklist_ok = true;

    glk_policy_request_t req;
    memset(&req, 0, sizeof(req));
    req.actor = GLK_ACTOR_RPC;
    req.action = "subghz_tx";
    req.risk = GLK_RISK_ACTIVE_TX;
    req.freq_hz = 433920000;
    req.gpio_pin = -1;

    /* no edu */
    glk_policy_decision_t d = glk_policy_check(&st, &req);
    assert(d.result == GLK_POLICY_DENY);

    glk_policy_set_edu_acked(&st, true);
    d = glk_policy_check(&st, &req);
    assert(d.result == GLK_POLICY_CONFIRM_NEEDED);

    char cid[24];
    assert(glk_policy_issue_confirm(&st, "subghz_tx", 60, 433920000, -1, cid, sizeof(cid)));
    req.confirm_id = cid;
    d = glk_policy_check(&st, &req);
    assert(d.result == GLK_POLICY_ALLOW);

    /* single use */
    d = glk_policy_check(&st, &req);
    assert(d.result == GLK_POLICY_CONFIRM_NEEDED);

    /* passive rx after edu */
    req.action = "subghz_rx";
    req.risk = GLK_RISK_PASSIVE_RX;
    req.confirm_id = NULL;
    d = glk_policy_check(&st, &req);
    assert(d.result == GLK_POLICY_ALLOW);

    /* medsec-strict: TX forbidden even with fresh confirm */
    glk_policy_set_medsec_strict(&st, true);
    assert(glk_policy_medsec_strict(&st));
    assert(strcmp(glk_policy_profile_name(&st), "medsec-strict") == 0);
    req.action = "subghz_tx";
    req.risk = GLK_RISK_ACTIVE_TX;
    assert(glk_policy_issue_confirm(&st, "subghz_tx", 60, 433920000, -1, cid, sizeof(cid)));
    req.confirm_id = cid;
    d = glk_policy_check(&st, &req);
    assert(d.result == GLK_POLICY_DENY);
    assert(strstr(d.reason, "medsec_strict") != NULL);

    /* passive still allowed under medsec-strict */
    req.action = "subghz_rx";
    req.risk = GLK_RISK_PASSIVE_RX;
    req.confirm_id = NULL;
    d = glk_policy_check(&st, &req);
    assert(d.result == GLK_POLICY_ALLOW);

    glk_policy_set_medsec_strict(&st, false);
    assert(!glk_policy_medsec_strict(&st));

    printf("test_policy: OK (incl. medsec-strict)\n");
    return 0;
}
