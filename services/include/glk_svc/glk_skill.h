/**
 * Hot-loadable skill registry with risk classification, refcounting, and
 * optional signed-package verification (GLK_FEATURE_SIGNED_SKILLS).
 *
 * ROM catalog remains always-available passive fallback via glk_skill_register.
 * SD skills: scan skills/<id>/manifest.json (+ rules.json, optional .sig).
 */
#pragma once

#include "glk/glk_types.h"
#include "glk/glk_config.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLK_SKILL_SRC_ROM = 0,
    GLK_SKILL_SRC_SD = 1,
} glk_skill_src_t;

typedef struct {
    char id[GLK_SKILL_ID_MAX];
    char version[16];
    glk_risk_t risk;
    bool loaded;
    bool signed_ok;
    bool enforce_sig;
    glk_skill_src_t source;
    uint16_t refcount;
    char path[192];       /* manifest path or rom:// */
    char rules_path[192]; /* optional rules.json */
    uint32_t content_hash;
} glk_skill_t;

glk_err_t glk_skill_init(void);

/**
 * Scan skills directory (absolute host path or under storage).
 * Merges with existing ROM entries (does not wipe ROM). Soft-fails missing root.
 */
glk_err_t glk_skill_scan(const char* skills_root);

/** Scan using storage root + "skills" relative layout. */
glk_err_t glk_skill_scan_storage(void);

/** Register a skill without filesystem (ROM catalog). No-op if id exists. */
glk_err_t glk_skill_register(const char* id, const char* version, glk_risk_t risk);

/** Retain / release refcount; unload when 0 and source is SD (ROM never unloads). */
glk_err_t glk_skill_retain(const char* id);
glk_err_t glk_skill_release(const char* id);
glk_err_t glk_skill_unload(const char* id);

/**
 * Verify skill package signature.
 * Signature file: skills/<id>/manifest.sig - text "GLKSIG1 <hex8>\n" matching
 * FNV-1a32 of manifest.json bytes. When GLK_FEATURE_SIGNED_SKILLS=0, soft-fail
 * logs unsigned_ok; when 1, reject load if missing/mismatch.
 */
glk_err_t glk_skill_verify_package(const char* skill_dir, const char* id, uint32_t* out_hash, bool* out_ok);

size_t glk_skill_count(void);
size_t glk_skill_loaded_count(void);
const glk_skill_t* glk_skill_get(size_t index);
const glk_skill_t* glk_skill_find(const char* id);
size_t glk_skill_list(char* buf, size_t buflen);

/** Compact JSON for RPC skill_status. */
size_t glk_skill_status_json(char* buf, size_t buflen);

/** Map risk class to policy ceiling label (static string). */
const char* glk_skill_risk_str(glk_risk_t r);

#ifdef __cplusplus
}
#endif
