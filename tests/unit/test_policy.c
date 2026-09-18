#include "glk_svc/glk_policy.h"
#include "glk_svc/glk_audit.h"
#include "glk/glk_kernel.h"
#include <stdio.h>
#include <string.h>

#define REQUIRE(cond)                                                                          \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                    \
            return 1;                                                                          \
        }                                                                                      \
    } while (0)

int main(void) {
    glk_kernel_init();
    glk_audit_init("test_audit.jsonl");
    glk_policy_state_t st;
    REQUIRE(glk_policy_init(&st) == GLK_OK);
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
    REQUIRE(d.result == GLK_POLICY_DENY);

    glk_policy_set_edu_acked(&st, true);
    d = glk_policy_check(&st, &req);
    REQUIRE(d.result == GLK_POLICY_CONFIRM_NEEDED);

    char cid[24];
    REQUIRE(glk_policy_issue_confirm(&st, "subghz_tx", 60, 433920000, -1, cid, sizeof(cid)));
    req.confirm_id = cid;
    d = glk_policy_check(&st, &req);
    REQUIRE(d.result == GLK_POLICY_ALLOW);

    /* single use */
    d = glk_policy_check(&st, &req);
    REQUIRE(d.result == GLK_POLICY_CONFIRM_NEEDED);

    /* passive rx after edu */
    req.action = "subghz_rx";
    req.risk = GLK_RISK_PASSIVE_RX;
    req.confirm_id = NULL;
    d = glk_policy_check(&st, &req);
    REQUIRE(d.result == GLK_POLICY_ALLOW);

    /* medsec-strict: TX forbidden even with fresh confirm */
    glk_policy_set_medsec_strict(&st, true);
    REQUIRE(glk_policy_medsec_strict(&st));
    REQUIRE(strcmp(glk_policy_profile_name(&st), "medsec-strict") == 0);
    req.action = "subghz_tx";
    req.risk = GLK_RISK_ACTIVE_TX;
    REQUIRE(glk_policy_issue_confirm(&st, "subghz_tx", 60, 433920000, -1, cid, sizeof(cid)));
    req.confirm_id = cid;
    d = glk_policy_check(&st, &req);
    REQUIRE(d.result == GLK_POLICY_DENY);
    REQUIRE(strstr(d.reason, "medsec_strict") != NULL);

    /* passive still allowed under medsec-strict */
    req.action = "subghz_rx";
    req.risk = GLK_RISK_PASSIVE_RX;
    req.confirm_id = NULL;
    d = glk_policy_check(&st, &req);
    REQUIRE(d.result == GLK_POLICY_ALLOW);

    glk_policy_set_medsec_strict(&st, false);
    REQUIRE(!glk_policy_medsec_strict(&st));

    /* v3.8 storage op gates */
    d = glk_policy_check_storage_op(&st, GLK_POL_OP_VAULT_CLEAR, GLK_ACTOR_RPC);
    REQUIRE(d.result == GLK_POLICY_ALLOW); /* edu already acked */
    glk_policy_set_edu_acked(&st, false);
    d = glk_policy_check_storage_op(&st, GLK_POL_OP_VAULT_SEAL, GLK_ACTOR_RPC);
    REQUIRE(d.result == GLK_POLICY_DENY);

    printf("test_policy: OK (incl. medsec-strict + storage ops)\n");
    fflush(stdout);
    return 0;
}
