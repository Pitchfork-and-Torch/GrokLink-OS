#include "glk_svc/glk_fs.h"

#include <string.h>
#include <stdio.h>

#define GLKFS_TABLE_BLOCKS 4u
#define GLKFS_DATA_START (1u + GLKFS_TABLE_BLOCKS)

typedef struct {
    char name[48];
    uint32_t size;
    uint32_t start;
    uint32_t blocks;
    uint32_t crc;
} glk_fs_ent_t;

typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t block_count;
    uint32_t table_blocks;
    uint32_t file_max;
    uint32_t crc;
    uint8_t pad[512 - 28];
} glk_fs_super_t;

static glk_blockdev_t* s_dev;
static glk_fs_ent_t s_tab[GLKFS_MAX_FILES];
static uint32_t s_nfiles;
static bool s_mounted;

static uint32_t crc32_bytes(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static glk_err_t rw_block(int wr, uint32_t lba, void* buf) {
    if (!s_dev || !s_dev->read || !s_dev->write) return GLK_ERR_GENERIC;
    if (wr) return s_dev->write(s_dev, lba, buf);
    return s_dev->read(s_dev, lba, buf);
}

static int find_ent(const char* name) {
    for (uint32_t i = 0; i < GLKFS_MAX_FILES; i++) {
        if (s_tab[i].name[0] && strcmp(s_tab[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static int find_free_ent(void) {
    for (uint32_t i = 0; i < GLKFS_MAX_FILES; i++) {
        if (!s_tab[i].name[0]) return (int)i;
    }
    return -1;
}

static uint32_t alloc_start(uint32_t need) {
    uint32_t cursor = GLKFS_DATA_START;
    for (uint32_t i = 0; i < GLKFS_MAX_FILES; i++) {
        if (!s_tab[i].name[0]) continue;
        uint32_t end = s_tab[i].start + s_tab[i].blocks;
        if (end > cursor) cursor = end;
    }
    if (cursor + need > s_dev->block_count) return 0;
    return cursor;
}

static glk_err_t commit_table(void) {
    uint8_t blk[GLK_BLOCK_SIZE];
    memset(blk, 0, sizeof(blk));
    /* 8 entries per 512-byte block (64 bytes each) */
    for (uint32_t b = 0; b < GLKFS_TABLE_BLOCKS; b++) {
        memset(blk, 0, sizeof(blk));
        memcpy(blk, &s_tab[b * 8], 8 * sizeof(glk_fs_ent_t));
        if (rw_block(1, 1 + b, blk) != GLK_OK) return GLK_ERR_GENERIC;
    }
    uint8_t sblk[GLK_BLOCK_SIZE];
    memset(sblk, 0, sizeof(sblk));
    memcpy(sblk, GLKFS_MAGIC, 6);
    uint32_t meta[4];
    meta[0] = GLKFS_VERSION;
    meta[1] = s_dev->block_count;
    meta[2] = GLKFS_TABLE_BLOCKS;
    meta[3] = GLKFS_MAX_FILES;
    memcpy(sblk + 8, meta, sizeof(meta));
    uint32_t scrc = crc32_bytes(&s_tab[0], sizeof(s_tab));
    memcpy(sblk + 24, &scrc, 4);
    if (rw_block(1, 0, sblk) != GLK_OK) return GLK_ERR_GENERIC;
    if (s_dev->sync) (void)s_dev->sync(s_dev);
    return GLK_OK;
}

glk_err_t glk_fs_format(glk_blockdev_t* d) {
    if (!d || !d->present || d->block_count < 16) return GLK_ERR_INVAL;
    s_dev = d;
    memset(s_tab, 0, sizeof(s_tab));
    s_nfiles = 0;
    s_mounted = false;
    if (commit_table() != GLK_OK) return GLK_ERR_GENERIC;
    s_mounted = true;
    return GLK_OK;
}

glk_err_t glk_fs_mount(glk_blockdev_t* d) {
    if (!d || !d->present) return GLK_ERR_NOTFOUND;
    uint8_t blk[GLK_BLOCK_SIZE];
    s_dev = d;
    if (rw_block(0, 0, blk) != GLK_OK) return GLK_ERR_GENERIC;
    if (memcmp(blk, GLKFS_MAGIC, 6) != 0) return GLK_ERR_CORRUPT;
    uint32_t ver = 0, bcnt = 0;
    memcpy(&ver, blk + 8, 4);
    memcpy(&bcnt, blk + 12, 4);
    if (ver != GLKFS_VERSION) return GLK_ERR_CORRUPT;
    (void)bcnt;
    memset(s_tab, 0, sizeof(s_tab));
    for (uint32_t b = 0; b < GLKFS_TABLE_BLOCKS; b++) {
        if (rw_block(0, 1 + b, blk) != GLK_OK) return GLK_ERR_GENERIC;
        memcpy(&s_tab[b * 8], blk, 8 * sizeof(glk_fs_ent_t));
    }
    uint32_t got = crc32_bytes(&s_tab[0], sizeof(s_tab));
    if (rw_block(0, 0, blk) != GLK_OK) return GLK_ERR_GENERIC;
    uint32_t want = 0;
    memcpy(&want, blk + 24, 4);
    if (got != want) return GLK_ERR_CORRUPT;
    s_nfiles = 0;
    for (uint32_t i = 0; i < GLKFS_MAX_FILES; i++) {
        if (s_tab[i].name[0]) s_nfiles++;
    }
    s_mounted = true;
    return GLK_OK;
}

void glk_fs_unmount(void) {
    s_mounted = false;
    s_dev = NULL;
    memset(s_tab, 0, sizeof(s_tab));
}

bool glk_fs_mounted(void) {
    return s_mounted;
}

static glk_err_t write_bytes(uint32_t start, const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint8_t blk[GLK_BLOCK_SIZE];
    uint32_t off = 0;
    uint32_t lba = start;
    while (off < len) {
        memset(blk, 0, sizeof(blk));
        size_t n = len - off;
        if (n > GLK_BLOCK_SIZE) n = GLK_BLOCK_SIZE;
        if (p) memcpy(blk, p + off, n);
        if (rw_block(1, lba, blk) != GLK_OK) return GLK_ERR_GENERIC;
        off += (uint32_t)n;
        lba++;
    }
    return GLK_OK;
}

static glk_err_t read_bytes(uint32_t start, uint32_t size, void* data, size_t cap, size_t* out_len) {
    uint8_t blk[GLK_BLOCK_SIZE];
    uint32_t want = size;
    if (want > cap) want = (uint32_t)cap;
    uint32_t got = 0;
    uint32_t lba = start;
    uint8_t* out = (uint8_t*)data;
    while (got < want) {
        if (rw_block(0, lba, blk) != GLK_OK) return GLK_ERR_GENERIC;
        uint32_t n = want - got;
        if (n > GLK_BLOCK_SIZE) n = GLK_BLOCK_SIZE;
        if (out) memcpy(out + got, blk, n);
        got += n;
        lba++;
    }
    if (out_len) *out_len = got;
    return GLK_OK;
}

glk_err_t glk_fs_write(const char* name, const void* data, size_t len) {
    if (!s_mounted || !name || !name[0] || strstr(name, "..")) return GLK_ERR_INVAL;
    if (strlen(name) > GLKFS_NAME_MAX) return GLK_ERR_INVAL;
    int idx = find_ent(name);
    if (idx < 0) {
        idx = find_free_ent();
        if (idx < 0) return GLK_ERR_FULL;
    }
    uint32_t need = (uint32_t)((len + GLK_BLOCK_SIZE - 1) / GLK_BLOCK_SIZE);
    if (need == 0) need = 1;
    /* always append new extent (simple, crash-safe commit) */
    uint32_t start = alloc_start(need);
    if (!start) return GLK_ERR_FULL;
    if (write_bytes(start, data, len) != GLK_OK) return GLK_ERR_GENERIC;
    memset(&s_tab[idx], 0, sizeof(s_tab[idx]));
    strncpy(s_tab[idx].name, name, sizeof(s_tab[idx].name) - 1);
    s_tab[idx].size = (uint32_t)len;
    s_tab[idx].start = start;
    s_tab[idx].blocks = need;
    s_tab[idx].crc = crc32_bytes(data, len);
    return commit_table();
}

glk_err_t glk_fs_append(const char* name, const void* data, size_t len) {
    if (!s_mounted) return GLK_ERR_GENERIC;
    uint8_t tmp[2048];
    size_t n = 0;
    if (glk_fs_exists(name)) {
        if (glk_fs_read(name, tmp, sizeof(tmp), &n) != GLK_OK) return GLK_ERR_GENERIC;
        if (n + len > sizeof(tmp)) return GLK_ERR_FULL;
        if (data && len) memcpy(tmp + n, data, len);
        return glk_fs_write(name, tmp, n + len);
    }
    return glk_fs_write(name, data, len);
}

glk_err_t glk_fs_read(const char* name, void* data, size_t cap, size_t* out_len) {
    if (!s_mounted || !name) return GLK_ERR_INVAL;
    int idx = find_ent(name);
    if (idx < 0) return GLK_ERR_NOTFOUND;
    return read_bytes(s_tab[idx].start, s_tab[idx].size, data, cap, out_len);
}

bool glk_fs_exists(const char* name) {
    if (!s_mounted || !name) return false;
    return find_ent(name) >= 0;
}

glk_err_t glk_fs_remove(const char* name) {
    if (!s_mounted || !name) return GLK_ERR_INVAL;
    int idx = find_ent(name);
    if (idx < 0) return GLK_ERR_NOTFOUND;
    memset(&s_tab[idx], 0, sizeof(s_tab[idx]));
    return commit_table();
}
