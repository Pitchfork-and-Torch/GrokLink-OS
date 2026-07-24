#include "glk/glk_kernel.h"
#include "glk_svc/glk_policy.h"
#include "glk_svc/glk_audit.h"
#include "glk_svc/glk_agent.h"
#include "glk_svc/glk_skill.h"
#include "glk_svc/glk_catalog.h"
#include "glk_svc/glk_vault.h"
#include "glk_svc/glk_ml.h"
#include "glk_drv/glk_radio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define REQUIRE(cond)                                                                          \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                    \
            return 1;                                                                          \
        }                                                                                      \
    } while (0)

int main(void) {
    glk_kernel_init();
    glk_audit_init("test_catalog_audit.jsonl");
    glk_ml_init();
    glk_skill_init();

    glk_policy_state_t pol;
    REQUIRE(glk_policy_init(&pol) == GLK_OK);
    glk_policy_set_global(&pol);
    glk_policy_set_edu_acked(&pol, true);
    pol.blacklist_ok = true;
    glk_policy_set_sd_present(&pol, false);

    glk_radio_init(&pol);
    glk_radio_start_worker();

    glk_agent_t ag;
    REQUIRE(glk_agent_init(&ag, &pol) == GLK_OK);
    REQUIRE(glk_catalog_load_defaults(&ag) == GLK_OK);

    REQUIRE(glk_skill_count() >= glk_catalog_builtin_skill_count());
    REQUIRE(ag.mission_count >= glk_catalog_builtin_mission_count());
    REQUIRE(glk_agent_get(&ag, "lab_passive_433") != NULL);
    REQUIRE(glk_agent_get(&ag, "medsec_lab_passive_ism") != NULL);
    REQUIRE(glk_agent_get(&ag, "fac_rf_snapshot_passive") != NULL);
    REQUIRE(glk_agent_get(&ag, "medsec_passive_watch") != NULL);
    REQUIRE(glk_skill_find("lab_passive_listen") != NULL);
    REQUIRE(glk_skill_find("medsec_passive_ism_watch") != NULL);

    REQUIRE(glk_agent_arm(&ag, "lab_passive_433") == GLK_OK);
    uint32_t ran = 0;
    REQUIRE(glk_agent_run_steps(&ag, 16, &ran) == GLK_OK);
    REQUIRE(ran >= 1);
    const glk_mission_t* m = glk_agent_get(&ag, "lab_passive_433");
    REQUIRE(m != NULL);
    REQUIRE(m->state == GLK_MISSION_DONE || m->pc > 0);

    char st[256];
    size_t n = glk_agent_status_json(&ag, "lab_passive_433", st, sizeof(st));
    REQUIRE(n > 0);
    REQUIRE(strstr(st, "lab_passive_433") != NULL);

    REQUIRE(glk_agent_disarm(&ag, "lab_passive_433") == GLK_OK);
    m = glk_agent_get(&ag, "lab_passive_433");
    REQUIRE(m->state == GLK_MISSION_IDLE);

    /* idempotent catalog reload */
    size_t before = ag.mission_count;
    REQUIRE(glk_catalog_load_defaults(&ag) == GLK_OK);
    REQUIRE(ag.mission_count == before);

    /* offline autonomous poll */
    REQUIRE(glk_agent_set_autonomous(&ag, "lab_passive_watch", true) == GLK_OK);
    glk_agent_set_offline(&ag, true);
    REQUIRE(glk_agent_offline(&ag));
    for (int i = 0; i < 12; i++) {
        (void)glk_agent_poll_usb_safe(&ag);
    }
    REQUIRE(glk_vault_count() > 0);
    char snap[256];
    REQUIRE(glk_agent_snapshot_json(&ag, snap, sizeof(snap)) > 0);
    REQUIRE(strstr(snap, "offline\":true") != NULL);

    printf("catalog ok missions=%u skills=%u ran=%u vault=%u\n",
           (unsigned)ag.mission_count,
           (unsigned)glk_skill_count(),
           (unsigned)ran,
           (unsigned)glk_vault_count());
    return 0;
}
