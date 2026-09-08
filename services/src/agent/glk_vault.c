/**
 * RAM vault + durable hash-chained append on storage (v3.8).
 */
#include "glk_svc/glk_vault.h"
#include "glk_svc/glk_storage.h"
#include "glk/glk_kernel.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static glk_vault_event_t s_ev[GLK_VAULT_CAP];
static size_t s_head; /* next write */
static size_t s_count;
static uint32_t s_chain;
static bool s_persistent;
static bool s_sealed;
static uint32_t s_seal_token;
static char s_events_rel[96];
static char s_meta_rel[96];

static uint32_t fnv_mix(uint32_t h, const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; i++) {
        h ^= b[i];
        h *= 16777619u;
    }
    return h;
}

static uint32_t event_hash(uint32_t prev, const glk_vault_event_t* e) {
    uint32_t h = prev ? prev : 2166136261u;
    h = fnv_mix(h, &e->ts_ms, sizeof(e->ts_ms));
    h = fnv_mix(h, e->mission, strlen(e->mission));
    h = fnv_mix(h, e->kind, strlen(e->kind));
    h = fnv_mix(h, &e->pulses, sizeof(e->pulses));
    h = fnv_mix(h, &e->infer_label, sizeof(e->infer_label));
    return h;
}

static void write_meta(void) {
    if (!s_persistent || !glk_storage_usable()) return;
    char line[80];
    int n = snprintf(line, sizeof(line), "GLKVAULT1 root=%08x count=%u sealed=%u\n",
                     (unsigned)s_chain, (unsigned)s_count, s_sealed ? 1u : 0u);
    if (n > 0) (void)glk_storage_write_file(s_meta_rel, line, (size_t)n);
    (void)glk_storage_write_integrity("vault/.integrity", s_chain);
}

static void append_durable(const glk_vault_event_t* e) {
    if (!s_persistent || !glk_storage_usable() || s_sealed) return;
    char line[220];
    int n = snprintf(
        line,
        sizeof(line),
        "{\"ts_ms\":%u,\"mission\":\"%s\",\"kind\":\"%s\",\"pulses\":%ld,"
        "\"infer\":%ld,\"score\":%.2f,\"chain\":\"%08x\"}\n",
        (unsigned)e->ts_ms,
        e->mission,
        e->kind,
        (long)e->pulses,
        (long)e->infer_label,
        (double)e->score,
        (unsigned)e->chain_hash);
    if (n > 0) {
        if (glk_storage_append_file(s_events_rel, line, (size_t)n) != GLK_OK) {
            /* stay RAM-capable; mark storage degraded via refresh path */
            s_persistent = glk_storage_usable();
        } else {
            write_meta();
        }
    }
}

static void load_durable_tail(void) {
    if (!glk_storage_usable()) return;
    char buf[4096];
    size_t n = 0;
    if (glk_storage_read_file(s_events_rel, buf, sizeof(buf) - 1, &n) != GLK_OK) return;
    buf[n] = 0;
    /* parse last lines coarsely - reload chain root from meta if present */
    char meta[96];
    size_t mn = 0;
    if (glk_storage_read_file(s_meta_rel, meta, sizeof(meta) - 1, &mn) == GLK_OK) {
        meta[mn] = 0;
        unsigned root = 0, cnt = 0, sealed = 0;
        if (sscanf(meta, "GLKVAULT1 root=%x count=%u sealed=%u", &root, &cnt, &sealed) >= 1) {
            s_chain = root;
            s_sealed = sealed != 0;
        }
    }
    /* extract up to GLK_VAULT_CAP recent events from end of buffer */
    const char* p = buf;
    const char* lines[64];
    size_t lc = 0;
    while (*p && lc < 64) {
        lines[lc++] = p;
        const char* nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    size_t start = lc > GLK_VAULT_CAP ? lc - GLK_VAULT_CAP : 0;
    for (size_t i = start; i < lc; i++) {
        const char* line = lines[i];
        glk_vault_event_t e;
        memset(&e, 0, sizeof(e));
        unsigned ts = 0;
        long pulses = 0, infer = 0;
        double score = 0;
        char mission[GLK_MISSION_ID_MAX];
        char kind[12];
        mission[0] = kind[0] = 0;
        /* minimal field extract */
        const char* m = strstr(line, "\"mission\":\"");
        if (m) {
            m += 11;
            size_t j = 0;
            while (*m && *m != '"' && j + 1 < sizeof(mission)) mission[j++] = *m++;
            mission[j] = 0;
        }
        const char* k = strstr(line, "\"kind\":\"");
        if (k) {
            k += 8;
            size_t j = 0;
            while (*k && *k != '"' && j + 1 < sizeof(kind)) kind[j++] = *k++;
            kind[j] = 0;
        }
        const char* t = strstr(line, "\"ts_ms\":");
        if (t) sscanf(t, "\"ts_ms\":%u", &ts);
        const char* pu = strstr(line, "\"pulses\":");
        if (pu) sscanf(pu, "\"pulses\":%ld", &pulses);
        const char* inf = strstr(line, "\"infer\":");
        if (inf) sscanf(inf, "\"infer\":%ld", &infer);
        const char* sc = strstr(line, "\"score\":");
        if (sc) sscanf(sc, "\"score\":%lf", &score);
        e.ts_ms = ts;
        strncpy(e.mission, mission, sizeof(e.mission) - 1);
        strncpy(e.kind, kind, sizeof(e.kind) - 1);
        e.pulses = (int32_t)pulses;
        e.infer_label = (int32_t)infer;
        e.score = (float)score;
        s_chain = event_hash(s_chain, &e);
        e.chain_hash = s_chain;
        s_ev[s_head] = e;
        s_head = (s_head + 1) % GLK_VAULT_CAP;
        if (s_count < GLK_VAULT_CAP) s_count++;
    }
}

void glk_vault_init(void) {
    memset(s_ev, 0, sizeof(s_ev));
    s_head = 0;
    s_count = 0;
    s_chain = 2166136261u;
    s_persistent = false;
    s_sealed = false;
    s_seal_token = 0;
    strncpy(s_events_rel, "vault/events.jsonl", sizeof(s_events_rel) - 1);
    strncpy(s_meta_rel, "vault/chain.meta", sizeof(s_meta_rel) - 1);
}

glk_err_t glk_vault_bind_storage(const char* events_rel) {
    if (events_rel && events_rel[0]) {
        strncpy(s_events_rel, events_rel, sizeof(s_events_rel) - 1);
    }
    if (!glk_storage_usable()) {
        s_persistent = false;
        return GLK_ERR_DEGRADED;
    }
    (void)glk_storage_ensure_layout();
    s_persistent = true;
    load_durable_tail();
    return GLK_OK;
}

void glk_vault_clear(void) {
    if (s_sealed) return;
    memset(s_ev, 0, sizeof(s_ev));
    s_head = 0;
    s_count = 0;
    s_chain = 2166136261u;
}

glk_err_t glk_vault_clear_persistent(void) {
    if (s_sealed) return GLK_ERR_DENIED;
    glk_vault_clear();
    if (s_persistent && glk_storage_usable()) {
        (void)glk_storage_write_file(s_events_rel, "", 0);
        write_meta();
    }
    return GLK_OK;
}

void glk_vault_push(
    const char* mission,
    const char* kind,
    int32_t pulses,
    int32_t infer_label,
    float score) {
    if (s_sealed) return; /* sealed: no new events until unseal */
    glk_vault_event_t* e = &s_ev[s_head];
    memset(e, 0, sizeof(*e));
    e->ts_ms = glk_tick_get();
    if (mission) strncpy(e->mission, mission, sizeof(e->mission) - 1);
    if (kind) strncpy(e->kind, kind, sizeof(e->kind) - 1);
    e->pulses = pulses;
    e->infer_label = infer_label;
    e->score = score;
    s_chain = event_hash(s_chain, e);
    e->chain_hash = s_chain;
    s_head = (s_head + 1) % GLK_VAULT_CAP;
    if (s_count < GLK_VAULT_CAP) s_count++;
    append_durable(e);
}

size_t glk_vault_count(void) {
    return s_count;
}

size_t glk_vault_tail(glk_vault_event_t* out, size_t max) {
    if (!out || max == 0 || s_count == 0) return 0;
    size_t n = s_count < max ? s_count : max;
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
    for (size_t i = 0; i < n && off + 100 < buflen; i++) {
        off += (size_t)snprintf(
            buf + off,
            buflen - off,
            "%s{\"ts_ms\":%u,\"mission\":\"%s\",\"kind\":\"%s\",\"pulses\":%ld,"
            "\"infer\":%ld,\"score\":%.2f,\"chain\":\"%08x\"}",
            i ? "," : "",
            (unsigned)tmp[i].ts_ms,
            tmp[i].mission,
            tmp[i].kind,
            (long)tmp[i].pulses,
            (long)tmp[i].infer_label,
            (double)tmp[i].score,
            (unsigned)tmp[i].chain_hash);
    }
    if (off + 2 < buflen) {
        buf[off++] = ']';
        buf[off] = 0;
    }
    return off;
}

glk_err_t glk_vault_flush(void) {
    if (!s_persistent) return GLK_ERR_DEGRADED;
    write_meta();
    return GLK_OK;
}

glk_err_t glk_vault_seal(const char* secret) {
    if (!secret || !secret[0]) return GLK_ERR_INVAL;
    s_seal_token = fnv_mix(2166136261u, secret, strlen(secret));
    s_sealed = true;
    write_meta();
    return GLK_OK;
}

glk_err_t glk_vault_unseal(const char* secret) {
    if (!secret || !secret[0]) return GLK_ERR_INVAL;
    uint32_t t = fnv_mix(2166136261u, secret, strlen(secret));
    if (!s_sealed) return GLK_OK;
    if (t != s_seal_token) return GLK_ERR_DENIED;
    s_sealed = false;
    s_seal_token = 0;
    write_meta();
    return GLK_OK;
}

bool glk_vault_sealed(void) {
    return s_sealed;
}

bool glk_vault_persistent(void) {
    return s_persistent && glk_storage_usable();
}

uint32_t glk_vault_chain_root(void) {
    return s_chain;
}

size_t glk_vault_status_json(char* buf, size_t buflen) {
    if (!buf || buflen < 8) return 0;
    return (size_t)snprintf(
        buf,
        buflen,
        "{\"count\":%u,\"persistent\":%s,\"sealed\":%s,\"chain\":\"%08x\"}",
        (unsigned)s_count,
        glk_vault_persistent() ? "true" : "false",
        s_sealed ? "true" : "false",
        (unsigned)s_chain);
}
