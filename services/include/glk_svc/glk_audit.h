/**
 * Append-only audit log with hash chaining.
 */
#pragma once

#include "glk/glk_types.h"
#include "glk_svc/glk_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

glk_err_t glk_audit_init(const char* log_path);
void glk_audit_set_enabled(bool enabled);

/** Chain record: prev_hash|fields -> new hash stored. */
void glk_audit_log(
    glk_actor_t actor,
    const char* action,
    glk_risk_t risk,
    glk_policy_result_t decision,
    const char* reason,
    const char* detail);

/** Export last N lines to buffer (host/tests). */
size_t glk_audit_export(char* buf, size_t buf_len, size_t max_lines);

/** Verify chain from file; returns GLK_OK or GLK_ERR_CORRUPT. */
glk_err_t glk_audit_verify(const char* log_path);

const char* glk_audit_last_hash(void);

#ifdef __cplusplus
}
#endif
