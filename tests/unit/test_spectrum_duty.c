/**
 * v3.8 spectrum planner: min settle, duty budget, breaker fail-closed.
 */
#include "glk_drv/glk_radio.h"
#include "glk_svc/glk_policy.h"
#include "glk_svc/glk_audit.h"
#include "glk/glk_kernel.h"
#include "glk/glk_config.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    glk_kernel_init();
    glk_audit_init("test_spectrum_audit.jsonl");
    glk_policy_state_t pol;
    assert(glk_policy_init(&pol) == GLK_OK);
    glk_policy_set_global(&pol);
    glk_policy_set_edu_acked(&pol, true);
    pol.blacklist_ok = true;
    pol.sd_present = true;
    pol.degraded = false;

    assert(glk_radio_init(&pol) == GLK_OK);
    assert(glk_radio_start_worker() == GLK_OK);

    uint32_t freqs[] = {433920000u, 315000000u, 868000000u};
    glk_radio_result_t results[8];
    size_t out_n = 0;

    /* Request tiny settle - planner must enforce GLK_SPECTRUM_SETTLE_MS minimum */
    glk_err_t e = glk_radio_spectrum(GLK_ACTOR_RPC, freqs, 2, 50, 1, results, &out_n);
    assert(e == GLK_OK || out_n > 0);
    glk_radio_planner_status_t st;
    glk_radio_planner_status(&st);
    assert(st.last_settle_ms >= GLK_SPECTRUM_SETTLE_MS);
    assert(st.last_bands >= 1);

    char js[256];
    assert(glk_radio_planner_status_json(js, sizeof(js)) > 0);
    assert(strstr(js, "min_settle_ms") != NULL);

    /* Trip breaker */
    for (int i = 0; i < GLK_RADIO_FAULT_BREAKER + 1; i++) glk_policy_note_radio_fault(&pol);
    out_n = 0;
    e = glk_radio_spectrum(GLK_ACTOR_RPC, freqs, 2, 50, 2000, results, &out_n);
    assert(e == GLK_ERR_BREAKER);
    glk_radio_planner_status(&st);
    assert(st.breaker_trips >= 1);

    glk_policy_clear_radio_faults(&pol);
    assert(glk_radio_arbiter_acquire(100));
    assert(glk_radio_arbiter_held());
    glk_radio_arbiter_release();
    assert(!glk_radio_arbiter_held());

    printf("test_spectrum_duty: OK\n");
    return 0;
}
