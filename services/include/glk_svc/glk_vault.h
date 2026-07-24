/**
 * RAM mission/event vault — offline-capable without SD card.
 * Local-only ring buffer; export via RPC vault_tail.
 */
#pragma once

#include "glk/glk_types.h"
#include "glk/glk_config.h"

#include <stddef.h>
#include <stdint.h>

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
} glk_vault_event_t;

void glk_vault_init(void);
void glk_vault_clear(void);
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

#ifdef __cplusplus
}
#endif
