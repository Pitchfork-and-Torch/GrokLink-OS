/**
 * GrokLink OS — safety / policy engine (non-negotiable).
 * Default-deny for TX, GPIO out, contact, system.
 */
#pragma once

#include "glk/glk_types.h"
#include "glk/glk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLK_POLICY_ALLOW = 0,
    GLK_POLICY_DENY = 1,
    GLK_POLICY_CONFIRM_NEEDED = 2,
    GLK_POLICY_RATE_LIMITED = 3,
    GLK_POLICY_BLACKLISTED = 4,
    GLK_POLICY_DEGRADED = 5,
} glk_policy_result_t;

typedef struct {
    glk_actor_t actor;
    const char* action;
    glk_risk_t risk;
    uint32_t freq_hz; /* 0 if N/A */
    int32_t gpio_pin; /* -1 if N/A */
    const char* protocol;
    const char* confirm_id;
    const char* rationale;
} glk_policy_request_t;

typedef struct {
    glk_policy_result_t result;
    char reason[GLK_REASON_MAX];
} glk_policy_decision_t;

typedef struct {
    char id[24];
    char action[GLK_ACTION_MAX];
    uint32_t expires_unix;
    uint32_t scope_freq_hz;
    int32_t scope_gpio;
    bool used;
} glk_confirm_slot_t;

typedef struct {
    bool initialized;
    bool edu_acked;
    bool fail_closed_tx;
    bool require_edu_ack;
    bool sd_present;
    bool blacklist_ok;
    bool degraded;
    bool physical_confirm_ok;
    bool medsec_strict; /* TX fully forbidden; MedSec hospital-adjacent posture */
    uint32_t tx_max_ms;
    uint32_t tx_cooldown_ms;
    uint32_t last_tx_ms;
    uint32_t rx_cooldown_ms;
    uint32_t last_rx_ms;
    uint32_t radio_faults;
    glk_confirm_slot_t confirms[GLK_MAX_CONFIRM_SLOTS];
    uint32_t banned_freq_hz[64];
    size_t banned_freq_count;
    int32_t banned_gpio[32];
    size_t banned_gpio_count;
    char banned_proto[16][24];
    size_t banned_proto_count;
    char profile_name[24]; /* "default" or "medsec-strict" */
} glk_policy_state_t;

glk_err_t glk_policy_init(glk_policy_state_t* st);
void glk_policy_set_sd_present(glk_policy_state_t* st, bool present);
void glk_policy_set_edu_acked(glk_policy_state_t* st, bool acked);
bool glk_policy_edu_acked(const glk_policy_state_t* st);

/** Enable MedSec-strict profile: all TX forbidden (passive research only). */
void glk_policy_set_medsec_strict(glk_policy_state_t* st, bool on);
bool glk_policy_medsec_strict(const glk_policy_state_t* st);
const char* glk_policy_profile_name(const glk_policy_state_t* st);

/** Load blacklists from SD root mapping; fail-closed on corrupt TX lists. */
glk_err_t glk_policy_reload_blacklist(glk_policy_state_t* st, const char* sd_root);

/** Load profile from sd_root/config/profile.json (optional). */
glk_err_t glk_policy_reload_profile(glk_policy_state_t* st, const char* sd_root);

glk_policy_decision_t glk_policy_check(glk_policy_state_t* st, const glk_policy_request_t* req);

bool glk_policy_issue_confirm(
    glk_policy_state_t* st,
    const char* action,
    uint32_t ttl_sec,
    uint32_t scope_freq_hz,
    int32_t scope_gpio,
    char* out_id,
    size_t out_len);

void glk_policy_physical_confirm_set(glk_policy_state_t* st, bool ok);
bool glk_policy_physical_confirm_pending(const glk_policy_state_t* st);

void glk_policy_note_tx(glk_policy_state_t* st, uint32_t duration_ms);
void glk_policy_note_rx(glk_policy_state_t* st);
void glk_policy_note_radio_fault(glk_policy_state_t* st);
void glk_policy_clear_radio_faults(glk_policy_state_t* st);

/** Singleton used by drivers (set during boot). */
void glk_policy_set_global(glk_policy_state_t* st);
glk_policy_state_t* glk_policy_global(void);

#ifdef __cplusplus
}
#endif
