#include "glk_svc/glk_blockdev.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

typedef struct {
    FILE* fp;
    char path[320];
} glk_bdev_file_t;

static glk_err_t file_read(glk_blockdev_t* d, uint32_t lba, void* buf) {
    glk_bdev_file_t* f = (glk_bdev_file_t*)d->ctx;
    if (!f || !f->fp || !buf) return GLK_ERR_INVAL;
    if (lba >= d->block_count) return GLK_ERR_INVAL;
    if (fseek(f->fp, (long)lba * (long)GLK_BLOCK_SIZE, SEEK_SET) != 0) return GLK_ERR_GENERIC;
    if (fread(buf, 1, GLK_BLOCK_SIZE, f->fp) != GLK_BLOCK_SIZE) return GLK_ERR_GENERIC;
    return GLK_OK;
}

static glk_err_t file_write(glk_blockdev_t* d, uint32_t lba, const void* buf) {
    glk_bdev_file_t* f = (glk_bdev_file_t*)d->ctx;
    if (!f || !f->fp || !buf) return GLK_ERR_INVAL;
    if (lba >= d->block_count) return GLK_ERR_INVAL;
    if (fseek(f->fp, (long)lba * (long)GLK_BLOCK_SIZE, SEEK_SET) != 0) return GLK_ERR_GENERIC;
    if (fwrite(buf, 1, GLK_BLOCK_SIZE, f->fp) != GLK_BLOCK_SIZE) return GLK_ERR_GENERIC;
    return GLK_OK;
}

static glk_err_t file_sync(glk_blockdev_t* d) {
    glk_bdev_file_t* f = (glk_bdev_file_t*)d->ctx;
    if (!f || !f->fp) return GLK_ERR_INVAL;
    if (fflush(f->fp) != 0) return GLK_ERR_GENERIC;
    return GLK_OK;
}

glk_err_t glk_blockdev_file_open(glk_blockdev_t* d, const char* path, uint32_t block_count, bool create) {
    if (!d || !path || !path[0] || block_count < 16) return GLK_ERR_INVAL;
    memset(d, 0, sizeof(*d));
    glk_bdev_file_t* f = (glk_bdev_file_t*)calloc(1, sizeof(glk_bdev_file_t));
    if (!f) return GLK_ERR_NOMEM;
    strncpy(f->path, path, sizeof(f->path) - 1);
    const char* mode = create ? "w+b" : "r+b";
    f->fp = fopen(path, mode);
    if (!f->fp && !create) {
        f->fp = fopen(path, "r+b");
    }
    if (!f->fp) {
        free(f);
        return GLK_ERR_NOTFOUND;
    }
    if (create) {
        uint8_t z[GLK_BLOCK_SIZE];
        memset(z, 0, sizeof(z));
        for (uint32_t i = 0; i < block_count; i++) {
            if (fwrite(z, 1, GLK_BLOCK_SIZE, f->fp) != GLK_BLOCK_SIZE) {
                fclose(f->fp);
                free(f);
                return GLK_ERR_FULL;
            }
        }
        fflush(f->fp);
    }
    d->ctx = f;
    d->block_count = block_count;
    d->present = true;
    d->read = file_read;
    d->write = file_write;
    d->sync = file_sync;
    return GLK_OK;
}

void glk_blockdev_file_close(glk_blockdev_t* d) {
    if (!d || !d->ctx) return;
    glk_bdev_file_t* f = (glk_bdev_file_t*)d->ctx;
    if (f->fp) fclose(f->fp);
    free(f);
    memset(d, 0, sizeof(*d));
}

#if defined(GLK_PLATFORM_HOST)
glk_err_t glk_blockdev_sd_probe(glk_blockdev_t* d) {
    if (d) memset(d, 0, sizeof(*d));
    return GLK_ERR_NOSUPPORT;
}
#endif
