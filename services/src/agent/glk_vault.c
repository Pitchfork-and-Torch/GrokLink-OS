#include "glk_svc/glk_vault.h"
#include "glk/glk_kernel.h"

#include <stdio.h>
#include <string.h>

static glk_vault_event_t s_ev[GLK_VAULT_CAP];
static size_t s_head; /* next write */
static size_t s_count;

void glk_vault_init(void) {
    memset(s_ev, 0, sizeof(s_ev));
    s_head = 0;
    s_count = 0;
}

void glk_vault_clear(void) {
    glk_vault_init();
}

void glk_vault_push(
    const char* mission,
    const char* kind,
    int32_t pulses,
    int32_t infer_label,
    float score) {
    glk_vault_event_t* e = &s_ev[s_head];
    memset(e, 0, sizeof(*e));
    e->ts_ms = glk_tick_get();
    if (mission) strncpy(e->mission, mission, sizeof(e->mission) - 1);
    if (kind) strncpy(e->kind, kind, sizeof(e->kind) - 1);
    e->pulses = pulses;
    e->infer_label = infer_label;
    e->score = score;
    s_head = (s_head + 1) % GLK_VAULT_CAP;
    if (s_count < GLK_VAULT_CAP) s_count++;
}

size_t glk_vault_count(void) {
    return s_count;
}

size_t glk_vault_tail(glk_vault_event_t* out, size_t max) {
    if (!out || max == 0 || s_count == 0) return 0;
    size_t n = s_count < max ? s_count : max;
    /* oldest among the last n */
    size_t start = (s_head + GLK_VAULT_CAP - n) % GLK_VAULT_CAP;
    for (size_t i = 0; i < n; i++) {
        out[i] = s_ev[(start + i) % GLK_VAULT_CAP];
    }
    return n;
}

size_t glk_vault_tail_json(char* buf, size_t buflen, size_t max_events) {
    if (!buf || buflen < 4) return 0;
    glk_vault_event_t tmp[GLK_VAULT_CAP];
    if (max_events == 0 || max_events > GLK_VAULT_CAP) max_events = GLK_VAULT_CAP;
    size_t n = glk_vault_tail(tmp, max_events);
    size_t off = 0;
    off += (size_t)snprintf(buf + off, buflen - off, "[");
    for (size_t i = 0; i < n && off + 80 < buflen; i++) {
        off += (size_t)snprintf(
            buf + off,
            buflen - off,
            "%s{\"ts_ms\":%u,\"mission\":\"%s\",\"kind\":\"%s\",\"pulses\":%ld,"
            "\"infer\":%ld,\"score\":%.2f}",
            i ? "," : "",
            (unsigned)tmp[i].ts_ms,
            tmp[i].mission,
            tmp[i].kind,
            (long)tmp[i].pulses,
            (long)tmp[i].infer_label,
            (double)tmp[i].score);
    }
    if (off + 2 < buflen) {
        buf[off++] = ']';
        buf[off] = 0;
    }
    return off;
}
