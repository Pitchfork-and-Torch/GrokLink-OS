/**
 * ROM-resident passive lab catalog for SD-less device and host degraded mode.
 */
#include "glk_svc/glk_catalog.h"
#include "glk_svc/glk_skill.h"

#include <string.h>

static void add_passive_433(glk_agent_t* ag) {
    glk_mission_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, "lab_passive_433", sizeof(m.id) - 1);
    m.autonomous = false;
    m.step_timeout_ms = 10000;
    m.wall_deadline_ms = 60000;
    m.steps[0].op = GLK_OP_LOG;
    strncpy(m.steps[0].note, "mission_start", sizeof(m.steps[0].note) - 1);
    m.steps[1].op = GLK_OP_SUBGHZ_RX;
    m.steps[1].a = 433920000u;
    m.steps[1].b = 200u;
    m.steps[2].op = GLK_OP_INFER;
    m.steps[3].op = GLK_OP_LOG;
    strncpy(m.steps[3].note, "mission_end", sizeof(m.steps[3].note) - 1);
    m.step_count = 4;
    (void)glk_agent_load_mission(ag, &m);
}

static void add_lab_spectrum_planner(glk_agent_t* ag) {
    glk_mission_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, "lab_spectrum_planner", sizeof(m.id) - 1);
    m.autonomous = false;
    m.step_timeout_ms = 10000;
    m.wall_deadline_ms = 90000;
    m.steps[0].op = GLK_OP_LOG;
    strncpy(m.steps[0].note, "spectrum_start", sizeof(m.steps[0].note) - 1);
    m.steps[1].op = GLK_OP_SUBGHZ_RX;
    m.steps[1].a = 315000000u;
    m.steps[1].b = 150u;
    m.steps[2].op = GLK_OP_SLEEP_MS;
    m.steps[2].a = 50u;
    m.steps[3].op = GLK_OP_SUBGHZ_RX;
    m.steps[3].a = 433920000u;
    m.steps[3].b = 150u;
    m.steps[4].op = GLK_OP_INFER;
    m.steps[5].op = GLK_OP_LOG;
    strncpy(m.steps[5].note, "spectrum_end", sizeof(m.steps[5].note) - 1);
    m.step_count = 6;
    (void)glk_agent_load_mission(ag, &m);
}

static void add_watchdog_passive(glk_agent_t* ag) {
    /* Short passive watch: loop 2x RX+infer — still no TX. */
    glk_mission_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, "lab_passive_watch", sizeof(m.id) - 1);
    m.autonomous = false;
    m.step_timeout_ms = 8000;
    m.wall_deadline_ms = 45000;
    m.steps[0].op = GLK_OP_LOG;
    strncpy(m.steps[0].note, "watch_start", sizeof(m.steps[0].note) - 1);
    m.steps[1].op = GLK_OP_LOOP_BEGIN;
    m.steps[1].b = 2u;
    m.steps[2].op = GLK_OP_SUBGHZ_RX;
    m.steps[2].a = 433920000u;
    m.steps[2].b = 150u;
    m.steps[3].op = GLK_OP_IF_PULSES_GT;
    m.steps[3].a = 5u;
    m.steps[4].op = GLK_OP_LOG;
    strncpy(m.steps[4].note, "activity_seen", sizeof(m.steps[4].note) - 1);
    m.steps[5].op = GLK_OP_LOOP_END;
    m.steps[6].op = GLK_OP_INFER;
    m.steps[7].op = GLK_OP_LOG;
    strncpy(m.steps[7].note, "watch_end", sizeof(m.steps[7].note) - 1);
    m.step_count = 8;
    (void)glk_agent_load_mission(ag, &m);
}

static void add_noise_baseline(glk_agent_t* ag) {
    /* v3.5 Signal Cognition: short passive dwells for host noise-floor calibration. */
    glk_mission_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, "lab_noise_baseline", sizeof(m.id) - 1);
    m.autonomous = false;
    m.step_timeout_ms = 8000;
    m.wall_deadline_ms = 40000;
    m.steps[0].op = GLK_OP_LOG;
    strncpy(m.steps[0].note, "noise_baseline_start", sizeof(m.steps[0].note) - 1);
    m.steps[1].op = GLK_OP_SUBGHZ_RX;
    m.steps[1].a = 433920000u;
    m.steps[1].b = 100u;
    m.steps[2].op = GLK_OP_SLEEP_MS;
    m.steps[2].a = 50u;
    m.steps[3].op = GLK_OP_SUBGHZ_RX;
    m.steps[3].a = 433920000u;
    m.steps[3].b = 100u;
    m.steps[4].op = GLK_OP_LOG;
    strncpy(m.steps[4].note, "noise_baseline_end", sizeof(m.steps[4].note) - 1);
    m.step_count = 5;
    (void)glk_agent_load_mission(ag, &m);
}

/* MedSec / healthcare Phase 1 — passive only. NOT a medical device. */
static void add_medsec_lab_passive_ism(glk_agent_t* ag) {
    glk_mission_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, "medsec_lab_passive_ism", sizeof(m.id) - 1);
    m.autonomous = false;
    m.step_timeout_ms = 10000;
    m.wall_deadline_ms = 90000;
    m.steps[0].op = GLK_OP_LOG;
    strncpy(m.steps[0].note, "medsec_ism_start", sizeof(m.steps[0].note) - 1);
    m.steps[1].op = GLK_OP_SUBGHZ_RX;
    m.steps[1].a = 433920000u;
    m.steps[1].b = 200u;
    m.steps[2].op = GLK_OP_SLEEP_MS;
    m.steps[2].a = 100u;
    m.steps[3].op = GLK_OP_SUBGHZ_RX;
    m.steps[3].a = 315000000u;
    m.steps[3].b = 200u;
    m.steps[4].op = GLK_OP_IF_PULSES_GT;
    m.steps[4].a = 20u;
    m.steps[5].op = GLK_OP_LOG;
    strncpy(m.steps[5].note, "elevated_edges", sizeof(m.steps[5].note) - 1);
    m.steps[6].op = GLK_OP_INFER;
    m.steps[7].op = GLK_OP_LOG;
    strncpy(m.steps[7].note, "medsec_ism_end", sizeof(m.steps[7].note) - 1);
    m.step_count = 8;
    (void)glk_agent_load_mission(ag, &m);
}

static void add_fac_rf_snapshot_passive(glk_agent_t* ag) {
    glk_mission_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, "fac_rf_snapshot_passive", sizeof(m.id) - 1);
    m.autonomous = false;
    m.step_timeout_ms = 10000;
    m.wall_deadline_ms = 120000;
    m.steps[0].op = GLK_OP_LOG;
    strncpy(m.steps[0].note, "fac_snap_start", sizeof(m.steps[0].note) - 1);
    m.steps[1].op = GLK_OP_SUBGHZ_RX;
    m.steps[1].a = 315000000u;
    m.steps[1].b = 150u;
    m.steps[2].op = GLK_OP_SLEEP_MS;
    m.steps[2].a = 80u;
    m.steps[3].op = GLK_OP_SUBGHZ_RX;
    m.steps[3].a = 433920000u;
    m.steps[3].b = 150u;
    m.steps[4].op = GLK_OP_SLEEP_MS;
    m.steps[4].a = 80u;
    m.steps[5].op = GLK_OP_SUBGHZ_RX;
    m.steps[5].a = 868350000u;
    m.steps[5].b = 150u;
    m.steps[6].op = GLK_OP_INFER;
    m.steps[7].op = GLK_OP_LOG;
    strncpy(m.steps[7].note, "fac_snap_end", sizeof(m.steps[7].note) - 1);
    m.step_count = 8;
    (void)glk_agent_load_mission(ag, &m);
}

static void add_medsec_passive_watch(glk_agent_t* ag) {
    /* Short loop watch for MedSec lab — still no TX. */
    glk_mission_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, "medsec_passive_watch", sizeof(m.id) - 1);
    m.autonomous = false;
    m.step_timeout_ms = 8000;
    m.wall_deadline_ms = 60000;
    m.steps[0].op = GLK_OP_LOG;
    strncpy(m.steps[0].note, "medsec_watch_start", sizeof(m.steps[0].note) - 1);
    m.steps[1].op = GLK_OP_LOOP_BEGIN;
    m.steps[1].b = 2u;
    m.steps[2].op = GLK_OP_SUBGHZ_RX;
    m.steps[2].a = 433920000u;
    m.steps[2].b = 150u;
    m.steps[3].op = GLK_OP_IF_PULSES_GT;
    m.steps[3].a = 5u;
    m.steps[4].op = GLK_OP_LOG;
    strncpy(m.steps[4].note, "activity_seen", sizeof(m.steps[4].note) - 1);
    m.steps[5].op = GLK_OP_LOOP_END;
    m.steps[6].op = GLK_OP_INFER;
    m.steps[7].op = GLK_OP_LOG;
    strncpy(m.steps[7].note, "medsec_watch_end", sizeof(m.steps[7].note) - 1);
    m.step_count = 8;
    (void)glk_agent_load_mission(ag, &m);
}

glk_err_t glk_catalog_load_defaults(glk_agent_t* ag) {
    if (!ag) return GLK_ERR_INVAL;

    (void)glk_skill_register("lab_passive_listen", "3.5.0", GLK_RISK_PASSIVE_RX);
    (void)glk_skill_register("lab_anomaly_watch", "3.5.0", GLK_RISK_PASSIVE_RX);
    (void)glk_skill_register("lab_gpio_event", "3.5.0", GLK_RISK_GPIO); /* listed; not auto-armed */
    (void)glk_skill_register("medsec_passive_ism_watch", "0.2.0", GLK_RISK_PASSIVE_RX);
    (void)glk_skill_register("fac_rf_baseline_watch", "0.2.0", GLK_RISK_PASSIVE_RX);
    (void)glk_skill_register("med_asset_uid_inventory", "0.2.0", GLK_RISK_PASSIVE_RX);

    if (!glk_agent_get(ag, "lab_passive_433")) add_passive_433(ag);
    if (!glk_agent_get(ag, "lab_spectrum_planner")) add_lab_spectrum_planner(ag);
    if (!glk_agent_get(ag, "lab_passive_watch")) add_watchdog_passive(ag);
    if (!glk_agent_get(ag, "lab_noise_baseline")) add_noise_baseline(ag);
    if (!glk_agent_get(ag, "medsec_lab_passive_ism")) add_medsec_lab_passive_ism(ag);
    if (!glk_agent_get(ag, "fac_rf_snapshot_passive")) add_fac_rf_snapshot_passive(ag);
    if (!glk_agent_get(ag, "medsec_passive_watch")) add_medsec_passive_watch(ag);

    return GLK_OK;
}

size_t glk_catalog_builtin_skill_count(void) {
    return 6;
}

size_t glk_catalog_builtin_mission_count(void) {
    return 7;
}
