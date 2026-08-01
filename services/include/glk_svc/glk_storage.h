/**
 * Storage service - host FS shim + crash-safe helpers for SD layout.
 *
 * Logical root maps to GLK_SD_ROOT (/groklink on device; host: sd_card/groklink).
 * Prefer SD for bulk skills/missions/vault; fail closed when media absent/corrupt.
 * Host simulation is first-class (POSIX/Windows). Device without FS reports degraded.
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

typedef enum {
    GLK_STOR_ABSENT = 0,   /* no card / path missing */
    GLK_STOR_OK = 1,       /* present and layout healthy */
    GLK_STOR_DEGRADED = 2, /* present but partial (missing dirs / write fail) */
    GLK_STOR_CORRUPT = 3,  /* integrity marker failed */
    GLK_STOR_FULL = 4,     /* write rejected for space (best-effort host) */
    GLK_STOR_READONLY = 5, /* present but not writable */
} glk_storage_mode_t;

/** Init with absolute or relative host path to the groklink root. */
glk_err_t glk_storage_init(const char* root_path);

/** Re-probe presence and refresh mode (does not wipe vault/skills). */
glk_err_t glk_storage_refresh(void);

const char* glk_storage_root(void);
bool glk_storage_present(void);
glk_storage_mode_t glk_storage_mode(void);
const char* glk_storage_mode_str(glk_storage_mode_t m);

/** True when storage may be used for elevated persistence (not ABSENT/CORRUPT). */
bool glk_storage_usable(void);

/** Create standard /groklink subdirs if missing. Soft-fail on host errors. */
glk_err_t glk_storage_ensure_layout(void);

/** Build path under root: root/rel -> out (normalizes separators on Windows). */
glk_err_t glk_storage_path(const char* rel, char* out, size_t out_len);

/** Atomic write: write temp then rename (crash-safe on POSIX; best-effort Win). */
glk_err_t glk_storage_write_file(const char* rel, const void* data, size_t len);

/** Append bytes (open-append-close). Not atomic across power loss for the line. */
glk_err_t glk_storage_append_file(const char* rel, const void* data, size_t len);

glk_err_t glk_storage_read_file(const char* rel, void* data, size_t cap, size_t* out_len);

bool glk_storage_exists(const char* rel);
glk_err_t glk_storage_remove(const char* rel);

/** FNV-1a 32-bit over file contents (integrity helper; not a crypto claim). */
glk_err_t glk_storage_file_hash32(const char* rel, uint32_t* out_hash);

/**
 * Write integrity marker vault/.integrity or state/.integrity with hash of payload.
 * format: "GLK1 <hex8>\n"
 */
glk_err_t glk_storage_write_integrity(const char* rel_marker, uint32_t hash);

/** Verify integrity marker; CORRUPT mode if mismatch when require=true. */
glk_err_t glk_storage_check_integrity(const char* rel_marker, uint32_t expected, bool fail_closed);

/** Compact JSON status for RPC/GUI. */
size_t glk_storage_status_json(char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif
