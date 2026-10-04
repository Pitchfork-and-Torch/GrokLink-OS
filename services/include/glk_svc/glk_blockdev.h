/**
 * Block device for Field Card (GLKFS). 512-byte LBAs.
 * Host: file-backed image. Device: SD SPI probe (absent if no card).
 */
#pragma once

#include "glk/glk_types.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLK_BLOCK_SIZE 512u

typedef struct glk_blockdev glk_blockdev_t;

struct glk_blockdev {
    void* ctx;
    uint32_t block_count;
    bool present;
    glk_err_t (*read)(glk_blockdev_t* d, uint32_t lba, void* buf);
    glk_err_t (*write)(glk_blockdev_t* d, uint32_t lba, const void* buf);
    glk_err_t (*sync)(glk_blockdev_t* d);
};

/** Host: open/create a file image with block_count 512-byte sectors. */
glk_err_t glk_blockdev_file_open(glk_blockdev_t* d, const char* path, uint32_t block_count, bool create);

void glk_blockdev_file_close(glk_blockdev_t* d);

/**
 * Device SD probe. Returns NOTFOUND when no card / no SPI response.
 * Host builds return NOSUPPORT (use file image).
 */
glk_err_t glk_blockdev_sd_probe(glk_blockdev_t* d);

#ifdef __cplusplus
}
#endif
