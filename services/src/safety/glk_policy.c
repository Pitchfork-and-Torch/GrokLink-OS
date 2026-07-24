/**
 * Policy engine — fail-closed, default-deny elevated actions.
 */
#include "glk_svc/glk_policy.h"
#include "glk/glk_kernel.h"
#include "glk_svc/glk_audit.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static glk_policy_state_t* s_global;

void glk_policy_set_global(glk_policy_state_t* st) {
    s_global = st;
}

glk_policy_state_t* glk_policy_global(void) {
    return s_global;
}

static uint32_t now_unix(void) {
#if defined(GLK_PLATFORM_STM32) && !defined(GLK_PLATFORM_HOST)
    /* Monotonic seconds from boot when no RTC */
    return glk_tick_get() / 1000u;
#else
    return (uint32_t)time(NULL);
#endif
}

static uint32_t now_ms(void) {
    return glk_tick_get();
}

glk_err_t glk_policy_init(glk_policy_state_t* st) {
    if (!st) return GLK_ERR_INVAL;
    memset(st, 0, sizeof(*st));
    st->initialized = true;
    st->fail_closed_tx = true;
    st->require_edu_ack = true;
    st->sd_present = false;
    st->blacklist_ok = false;
    st->degraded = true; /* until SD + blacklist load */
    st->medsec_strict = false;
    st->tx_max_ms = GLK_TX_MAX_MS;
    st->tx_cooldown_ms = GLK_TX_COOLDOWN_MS;
    st->rx_cooldown_ms = GLK_RX_COOLDOWN_MS;
    strncpy(st->profile_name, "default", sizeof(st->profile_name) - 1);
    return GLK_OK;
}

void glk_policy_set_medsec_strict(glk_policy_state_t* st, bool on) {
    if (!st) return;
    st->medsec_strict = on;
    st->fail_closed_tx = true;
    if (on) {
        strncpy(st->profile_name, "medsec-strict", sizeof(st->profile_name) - 1);
    } else {
        strncpy(st->profile_name, "default", sizeof(st->profile_name) - 1);
    }
}

bool glk_policy_medsec_strict(const glk_policy_state_t* st) {
    return st && st->medsec_strict;
}

const char* glk_policy_profile_name(const glk_policy_state_t* st) {
    if (!st || !st->profile_name[0]) return "default";
    return st->profile_name;
}

glk_err_t glk_policy_reload_profile(glk_policy_state_t* st, const char* sd_root) {
    if (!st) return GLK_ERR_INVAL;
    if (!sd_root || !sd_root[0]) {
        glk_policy_set_medsec_strict(st, false);
        return GLK_OK;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/config/profile.json", sd_root);
    FILE* f = fopen(path, "rb");
    if (!f) {
        /* optional file — stay default */
        return GLK_OK;
    }
    char buf[512];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    /* crude detect medsec-strict without full JSON parser */
    if (strstr(buf, "medsec-strict") || strstr(buf, "medsec_strict")) {
        glk_policy_set_medsec_strict(st, true);
    } else {
        glk_policy_set_medsec_strict(st, false);
    }
    return GLK_OK;
}

void glk_policy_set_sd_present(glk_policy_state_t* st, bool present) {
    if (!st) return;
    st->sd_present = present;
    if (!present) {
        st->degraded = true;
    }
}

void glk_policy_set_edu_acked(glk_policy_state_t* st, bool acked) {
    if (!st) return;
    st->edu_acked = acked;
}

bool glk_policy_edu_acked(const glk_policy_state_t* st) {
    return st && st->edu_acked;
}

/* Minimal JSON number array scrape for blacklist files (no full parser dep). */
static void parse_u32_array_from_file(const char* path, uint32_t* out, size_t max, size_t* count) {
    *count = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    /* find numbers */
    for (size_t i = 0; i < n && *count < max;) {
        if (buf[i] >= '0' && buf[i] <= '9') {
            char* end = NULL;
            unsigned long v = strtoul(&buf[i], &end, 10);
            if (end && end != &buf[i]) {
                out[(*count)++] = (uint32_t)v;
                i = (size_t)(end - buf);
                continue;
            }
        }
        i++;
    }
}

static void parse_i32_array_from_file(const char* path, int32_t* out, size_t max, size_t* count) {
    *count = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return;
    char buf[2048];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    for (size_t i = 0; i < n && *count < max;) {
        if (buf[i] == '-' || (buf[i] >= '0' && buf[i] <= '9')) {
            char* end = NULL;
            long v = strtol(&buf[i], &end, 10);
            if (end && end != &buf[i]) {
                out[(*count)++] = (int32_t)v;
                i = (size_t)(end - buf);
                continue;
            }
        }
        i++;
    }
}

glk_err_t glk_policy_reload_blacklist(glk_policy_state_t* st, const char* sd_root) {
    if (!st || !sd_root) return GLK_ERR_INVAL;
    char path[512];
    snprintf(path, sizeof(path), "%s/blacklist/freq_mhz.json", sd_root);
    /* values in file are MHz; store as Hz for checks */
    uint32_t mhz[64];
    size_t mhz_n = 0;
    parse_u32_array_from_file(path, mhz, 64, &mhz_n);
    st->banned_freq_count = 0;
    for (size_t i = 0; i < mhz_n && st->banned_freq_count < 64; i++) {
        /* if value looks like Hz already (>10000), keep; else MHz -> Hz */
        uint32_t hz = mhz[i] > 10000u ? mhz[i] : mhz[i] * 1000000u;
        st->banned_freq_hz[st->banned_freq_count++] = hz;
    }

    snprintf(path, sizeof(path), "%s/blacklist/gpio_pins.json", sd_root);
    parse_i32_array_from_file(path, st->banned_gpio, 32, &st->banned_gpio_count);

    /* protocols: treat empty file as ok; missing is ok for RX */
    st->banned_proto_count = 0;
    snprintf(path, sizeof(path), "%s/blacklist/protocols.json", sd_root);
    FILE* pf = fopen(path, "rb");
    if (pf) {
        fclose(pf);
        /* presence is enough; detailed string parse optional */
    }

    st->blacklist_ok = true;
    st->degraded = !st->sd_present;
    /* Also pick up profile if present */
    (void)glk_policy_reload_profile(st, sd_root);
    return GLK_OK;
}

static bool freq_banned(const glk_policy_state_t* st, uint32_t freq_hz) {
    if (freq_hz == 0) return false;
    for (size_t i = 0; i < st->banned_freq_count; i++) {
        /* ±250 kHz window around banned center */
        uint32_t b = st->banned_freq_hz[i];
        if (freq_hz + 250000u >= b && freq_hz <= b + 250000u) return true;
        if (b + 250000u >= freq_hz && b <= freq_hz + 250000u) return true;
    }
    return false;
}

static bool gpio_banned(const glk_policy_state_t* st, int32_t pin) {
    if (pin < 0) return false;
    for (size_t i = 0; i < st->banned_gpio_count; i++) {
        if (st->banned_gpio[i] == pin) return true;
    }
    return false;
}

static bool confirm_valid(glk_policy_state_t* st, const char* action, const char* id, uint32_t freq, int32_t gpio) {
    if (!id || !id[0]) return false;
    uint32_t now = now_unix();
    for (int i = 0; i < GLK_MAX_CONFIRM_SLOTS; i++) {
        glk_confirm_slot_t* c = &st->confirms[i];
        if (c->used) continue;
        if (c->id[0] == 0) continue;
        if (strcmp(c->id, id) != 0) continue;
        if (strcmp(c->action, action) != 0) continue;
        if (c->expires_unix < now) continue;
        if (c->scope_freq_hz && freq && c->scope_freq_hz != freq) continue;
        if (c->scope_gpio >= 0 && gpio >= 0 && c->scope_gpio != gpio) continue;
        c->used = true;
        return true;
    }
    return false;
}

static glk_policy_decision_t decide(glk_policy_result_t r, const char* why) {
    glk_policy_decision_t d;
    d.result = r;
    memset(d.reason, 0, sizeof(d.reason));
    if (why) strncpy(d.reason, why, sizeof(d.reason) - 1);
    return d;
}

glk_policy_decision_t glk_policy_check(glk_policy_state_t* st, const glk_policy_request_t* req) {
    if (!st || !req || !req->action) {
        return decide(GLK_POLICY_DENY, "invalid_request");
    }
    if (!st->initialized) {
        return decide(GLK_POLICY_DENY, "policy_uninit");
    }

    /* INFO always allowed (still may audit at higher layer) */
    if (req->risk == GLK_RISK_INFO) {
        return decide(GLK_POLICY_ALLOW, "info");
    }

    if (st->require_edu_ack && !st->edu_acked && req->risk != GLK_RISK_INFO) {
        glk_policy_decision_t d = decide(GLK_POLICY_DENY, "edu_ack_required");
        glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, req->rationale);
        return d;
    }

    /* MedSec-strict: all TX / GPIO-out / contact forbidden (passive research only). */
    if (st->medsec_strict &&
        (req->risk == GLK_RISK_ACTIVE_TX || req->risk == GLK_RISK_GPIO ||
         req->risk == GLK_RISK_CONTACT || req->risk == GLK_RISK_SYSTEM)) {
        glk_policy_decision_t d = decide(GLK_POLICY_DENY, "medsec_strict_tx_forbidden");
        glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, "not_a_medical_device");
        return d;
    }

    /* Degraded: no SD -> no TX / no missions actuators */
    if (!st->sd_present && req->risk >= GLK_RISK_ACTIVE_TX) {
        glk_policy_decision_t d = decide(GLK_POLICY_DEGRADED, "no_sd");
        glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
        return d;
    }

    if (st->fail_closed_tx && !st->blacklist_ok && req->risk == GLK_RISK_ACTIVE_TX) {
        glk_policy_decision_t d = decide(GLK_POLICY_DEGRADED, "blacklist_not_ok");
        glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
        return d;
    }

    if (st->radio_faults >= GLK_RADIO_FAULT_BREAKER &&
        (req->risk == GLK_RISK_PASSIVE_RX || req->risk == GLK_RISK_ACTIVE_TX)) {
        glk_policy_decision_t d = decide(GLK_POLICY_DENY, "radio_circuit_breaker");
        glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
        return d;
    }

    if (req->freq_hz) {
        if (req->freq_hz < GLK_SUBGHZ_FREQ_MIN_HZ || req->freq_hz > GLK_SUBGHZ_FREQ_MAX_HZ) {
            glk_policy_decision_t d = decide(GLK_POLICY_DENY, "freq_out_of_window");
            glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
            return d;
        }
        if (freq_banned(st, req->freq_hz)) {
            glk_policy_decision_t d = decide(GLK_POLICY_BLACKLISTED, "freq_blacklisted");
            glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
            return d;
        }
    }

    if (req->gpio_pin >= 0 && gpio_banned(st, req->gpio_pin) && req->risk == GLK_RISK_GPIO) {
        glk_policy_decision_t d = decide(GLK_POLICY_BLACKLISTED, "gpio_blacklisted");
        glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
        return d;
    }

    /* Rate limits */
    uint32_t t = now_ms();
    if (req->risk == GLK_RISK_PASSIVE_RX && st->last_rx_ms &&
        (t - st->last_rx_ms) < st->rx_cooldown_ms) {
        glk_policy_decision_t d = decide(GLK_POLICY_RATE_LIMITED, "rx_cooldown");
        glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
        return d;
    }
    if (req->risk == GLK_RISK_ACTIVE_TX && st->last_tx_ms &&
        (t - st->last_tx_ms) < st->tx_cooldown_ms) {
        glk_policy_decision_t d = decide(GLK_POLICY_RATE_LIMITED, "tx_cooldown");
        glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
        return d;
    }

    /* Elevated need confirm */
    if (req->risk == GLK_RISK_ACTIVE_TX || req->risk == GLK_RISK_GPIO ||
        req->risk == GLK_RISK_CONTACT) {
        if (!confirm_valid(st, req->action, req->confirm_id, req->freq_hz, req->gpio_pin)) {
            glk_policy_decision_t d = decide(GLK_POLICY_CONFIRM_NEEDED, "confirm_required");
            glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
            return d;
        }
    }

    if (req->risk == GLK_RISK_SYSTEM) {
        if (!st->physical_confirm_ok) {
            glk_policy_decision_t d = decide(GLK_POLICY_CONFIRM_NEEDED, "physical_confirm_required");
            glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, NULL);
            return d;
        }
        st->physical_confirm_ok = false; /* one-shot */
    }

    glk_policy_decision_t d = decide(GLK_POLICY_ALLOW, "allow");
    glk_audit_log(req->actor, req->action, req->risk, d.result, d.reason, req->rationale);
    return d;
}

bool glk_policy_issue_confirm(
    glk_policy_state_t* st,
    const char* action,
    uint32_t ttl_sec,
    uint32_t scope_freq_hz,
    int32_t scope_gpio,
    char* out_id,
    size_t out_len) {
    if (!st || !action || !out_id || out_len < 12) return false;
    if (!st->edu_acked) return false;
    int slot = -1;
    for (int i = 0; i < GLK_MAX_CONFIRM_SLOTS; i++) {
        if (st->confirms[i].id[0] == 0 || st->confirms[i].used ||
            st->confirms[i].expires_unix < now_unix()) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;
    glk_confirm_slot_t* c = &st->confirms[slot];
    memset(c, 0, sizeof(*c));
    snprintf(c->id, sizeof(c->id), "C%08X", (unsigned)(now_unix() ^ (uint32_t)slot * 0x9E3779B9u));
    strncpy(c->action, action, sizeof(c->action) - 1);
    c->expires_unix = now_unix() + (ttl_sec ? ttl_sec : GLK_CONFIRM_DEFAULT_TTL_SEC);
    c->scope_freq_hz = scope_freq_hz;
    c->scope_gpio = scope_gpio;
    c->used = false;
    strncpy(out_id, c->id, out_len - 1);
    out_id[out_len - 1] = 0;
    glk_audit_log(GLK_ACTOR_RPC, "issue_confirm", GLK_RISK_INFO, GLK_POLICY_ALLOW, action, c->id);
    return true;
}

void glk_policy_physical_confirm_set(glk_policy_state_t* st, bool ok) {
    if (st) st->physical_confirm_ok = ok;
}

bool glk_policy_physical_confirm_pending(const glk_policy_state_t* st) {
    return st && !st->physical_confirm_ok;
}

void glk_policy_note_tx(glk_policy_state_t* st, uint32_t duration_ms) {
    if (!st) return;
    (void)duration_ms;
    st->last_tx_ms = now_ms();
}

void glk_policy_note_rx(glk_policy_state_t* st) {
    if (!st) return;
    st->last_rx_ms = now_ms();
}

void glk_policy_note_radio_fault(glk_policy_state_t* st) {
    if (!st) return;
    if (st->radio_faults < 1000) st->radio_faults++;
}

void glk_policy_clear_radio_faults(glk_policy_state_t* st) {
    if (st) st->radio_faults = 0;
}
