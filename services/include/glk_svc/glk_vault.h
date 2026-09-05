/**
 * Persistent mission/event vault - RAM ring + optional durable storage.
 *
 * Crash-safe append to vault/events.jsonl with hash chain root in vault/chain.meta.
 * Survives reboot when storage usable. Optional operator-secret seal (HMAC-like
 * FNV mix of secret - not a cryptographic vault claim; host tools do PHI seal).
 *
 * Local research only. NOT for medical records / PHI care systems.
 */
#pragma once

#include "glk/glk_types.h"
#include "glk/glk_config.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GLK_VAULT_CAP
#define GLK_VAULT_CAP 16
#endif

typedef struct {
    uint32_t ts_ms;
    char mission[GLK_MISSION_ID_MAX];
    char kind[12]; /* rx | done | log | infer | auto */
    int32_t pulses;
    int32_t infer_label;
    float score;
    uint32_t chain_hash; /* hash after this event */
} glk_vault_event_t;

void glk_vault_init(void);

/**
 * Bind vault to storage (rel path under root, default "vault/events.jsonl").
 * Loads tail into RAM if present. Safe to call when storage absent (RAM-only).
 */
glk_err_t glk_vault_bind_storage(const char* events_rel);

void glk_vault_clear(void);
/** Clear RAM + truncate durable file if bound and unsealed. */
glk_err_t glk_vault_clear_persistent(void);

void glk_vault_push(
    const char* mission,
    const char* kind,
    int32_t pulses,
    int32_t infer_label,
    float score);

size_t glk_vault_count(void);
/** Copy up to max newest events into out; returns count copied (oldest-first among selected). */
size_t glk_vault_tail(glk_vault_event_t* out, size_t max);

/** Compact JSON array into buf for RPC. */
size_t glk_vault_tail_json(char* buf, size_t buflen, size_t max_events);

/** Force flush chain meta / integrity marker. */
glk_err_t glk_vault_flush(void);

/** Seal with operator secret (blocks clear/push from remote until unseal). */
glk_err_t glk_vault_seal(const char* secret);
glk_err_t glk_vault_unseal(const char* secret);
bool glk_vault_sealed(void);
bool glk_vault_persistent(void);
uint32_t glk_vault_chain_root(void);

size_t glk_vault_status_json(char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif
