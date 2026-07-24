/**
 * Storage service — SD + host FS mapping, crash-safe helpers.
 */
#pragma once

#include "glk/glk_types.h"

#ifdef __cplusplus
extern "C" {
#endif

glk_err_t glk_storage_init(const char* root_path);
const char* glk_storage_root(void);
bool glk_storage_present(void);

/** Build path under root: root/rel -> out */
glk_err_t glk_storage_path(const char* rel, char* out, size_t out_len);

/** Atomic-ish write: write temp then rename. */
glk_err_t glk_storage_write_file(const char* rel, const void* data, size_t len);

glk_err_t glk_storage_read_file(const char* rel, void* data, size_t cap, size_t* out_len);

#ifdef __cplusplus
}
#endif
