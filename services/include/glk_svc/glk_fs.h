/**
 * GLKFS - tiny Field Card filesystem over 512-byte blocks.
 * Atomic file replace (write data, then commit table+super CRC).
 * Not littlefs-the-brand; same job: remount-surviving files on a card image.
 */
#pragma once

#include "glk_svc/glk_blockdev.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLKFS_MAGIC "GLKFS1"
#define GLKFS_VERSION 1u
#define GLKFS_MAX_FILES 32u
#define GLKFS_NAME_MAX 47u

glk_err_t glk_fs_format(glk_blockdev_t* d);
glk_err_t glk_fs_mount(glk_blockdev_t* d);
void glk_fs_unmount(void);
bool glk_fs_mounted(void);

glk_err_t glk_fs_write(const char* name, const void* data, size_t len);
glk_err_t glk_fs_append(const char* name, const void* data, size_t len);
glk_err_t glk_fs_read(const char* name, void* data, size_t cap, size_t* out_len);
bool glk_fs_exists(const char* name);
glk_err_t glk_fs_remove(const char* name);

#ifdef __cplusplus
}
#endif
