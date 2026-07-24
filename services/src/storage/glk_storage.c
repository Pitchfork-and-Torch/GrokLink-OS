#include "glk_svc/glk_storage.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static char s_root[512];
static bool s_present;

glk_err_t glk_storage_init(const char* root_path) {
    if (!root_path) return GLK_ERR_INVAL;
    strncpy(s_root, root_path, sizeof(s_root) - 1);
#ifdef _WIN32
    s_present = (_access(s_root, 0) == 0);
#else
    struct stat st;
    s_present = (stat(s_root, &st) == 0 && S_ISDIR(st.st_mode));
#endif
    return s_present ? GLK_OK : GLK_ERR_NOTFOUND;
}

const char* glk_storage_root(void) {
    return s_root;
}

bool glk_storage_present(void) {
    return s_present;
}

glk_err_t glk_storage_path(const char* rel, char* out, size_t out_len) {
    if (!rel || !out || out_len < 8) return GLK_ERR_INVAL;
    if (rel[0] == '/' || rel[0] == '\\') rel++;
    snprintf(out, out_len, "%s/%s", s_root, rel);
#ifdef _WIN32
    for (char* p = out; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#endif
    return GLK_OK;
}

glk_err_t glk_storage_write_file(const char* rel, const void* data, size_t len) {
    char path[512], tmp[520];
    if (glk_storage_path(rel, path, sizeof(path)) != GLK_OK) return GLK_ERR_INVAL;
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE* f = fopen(tmp, "wb");
    if (!f) return GLK_ERR_GENERIC;
    if (len && data) fwrite(data, 1, len, f);
    fclose(f);
#ifdef _WIN32
    remove(path);
    if (rename(tmp, path) != 0) return GLK_ERR_GENERIC;
#else
    if (rename(tmp, path) != 0) return GLK_ERR_GENERIC;
#endif
    return GLK_OK;
}

glk_err_t glk_storage_read_file(const char* rel, void* data, size_t cap, size_t* out_len) {
    char path[512];
    if (glk_storage_path(rel, path, sizeof(path)) != GLK_OK) return GLK_ERR_INVAL;
    FILE* f = fopen(path, "rb");
    if (!f) return GLK_ERR_NOTFOUND;
    size_t n = fread(data, 1, cap, f);
    fclose(f);
    if (out_len) *out_len = n;
    return GLK_OK;
}
