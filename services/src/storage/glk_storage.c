/**
 * Host-first storage service with crash-safe helpers and degraded modes.
 * Device builds without FS remain ABSENT/degraded (ROM passive only).
 */
#include "glk_svc/glk_storage.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define GLK_MKDIR(p) _mkdir(p)
#define GLK_ACCESS(p) (_access((p), 0) == 0)
#else
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#define GLK_MKDIR(p) mkdir((p), 0755)
#define GLK_ACCESS(p) (access((p), F_OK) == 0)
#endif

static char s_root[512];
static bool s_present;
static glk_storage_mode_t s_mode = GLK_STOR_ABSENT;
static uint32_t s_write_errors;
static uint32_t s_layout_ok;

static const char* k_layout_dirs[] = {
    "config",
    "missions",
    "skills",
    "logs",
    "blacklist",
    "vault",
    "state",
    "healthcare",
    NULL,
};

const char* glk_storage_mode_str(glk_storage_mode_t m) {
    switch (m) {
    case GLK_STOR_OK: return "ok";
    case GLK_STOR_DEGRADED: return "degraded";
    case GLK_STOR_CORRUPT: return "corrupt";
    case GLK_STOR_FULL: return "full";
    case GLK_STOR_READONLY: return "readonly";
    case GLK_STOR_ABSENT:
    default: return "absent";
    }
}

static bool path_is_dir(const char* path) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

static glk_err_t mkdir_p_one(const char* path) {
    if (path_is_dir(path)) return GLK_OK;
    if (GLK_MKDIR(path) == 0) return GLK_OK;
#ifdef _WIN32
    if (GLK_ACCESS(path)) return GLK_OK;
#else
    if (errno == EEXIST) return GLK_OK;
#endif
    return GLK_ERR_GENERIC;
}

static void win_slash(char* p) {
#ifdef _WIN32
    for (; p && *p; p++) {
        if (*p == '/') *p = '\\';
    }
#else
    (void)p;
#endif
}

glk_err_t glk_storage_init(const char* root_path) {
    if (!root_path || !root_path[0]) return GLK_ERR_INVAL;
    memset(s_root, 0, sizeof(s_root));
    strncpy(s_root, root_path, sizeof(s_root) - 1);
    s_write_errors = 0;
    s_layout_ok = 0;
    s_present = path_is_dir(s_root);
    if (!s_present) {
        s_mode = GLK_STOR_ABSENT;
        return GLK_ERR_NOTFOUND;
    }
    s_mode = GLK_STOR_DEGRADED; /* until layout confirmed */
    if (glk_storage_ensure_layout() == GLK_OK) {
        s_mode = GLK_STOR_OK;
        s_layout_ok = 1;
    } else {
        s_mode = GLK_STOR_DEGRADED;
    }
    return GLK_OK;
}

glk_err_t glk_storage_refresh(void) {
    if (!s_root[0]) {
        s_present = false;
        s_mode = GLK_STOR_ABSENT;
        return GLK_ERR_NOTFOUND;
    }
    s_present = path_is_dir(s_root);
    if (!s_present) {
        s_mode = GLK_STOR_ABSENT;
        return GLK_ERR_NOTFOUND;
    }
    if (s_mode == GLK_STOR_CORRUPT) return GLK_ERR_CORRUPT;
    if (s_mode == GLK_STOR_FULL) return GLK_ERR_FULL;
    if (glk_storage_ensure_layout() == GLK_OK && s_mode != GLK_STOR_READONLY) {
        if (s_mode != GLK_STOR_CORRUPT && s_mode != GLK_STOR_FULL) s_mode = GLK_STOR_OK;
        s_layout_ok = 1;
    }
    return GLK_OK;
}

const char* glk_storage_root(void) {
    return s_root;
}

bool glk_storage_present(void) {
    return s_present;
}

glk_storage_mode_t glk_storage_mode(void) {
    return s_mode;
}

bool glk_storage_usable(void) {
    return s_present && (s_mode == GLK_STOR_OK || s_mode == GLK_STOR_DEGRADED ||
                         s_mode == GLK_STOR_READONLY);
}

glk_err_t glk_storage_ensure_layout(void) {
    if (!s_present) return GLK_ERR_NOTFOUND;
    int ok = 0;
    int fail = 0;
    for (int i = 0; k_layout_dirs[i]; i++) {
        char path[560];
        if (glk_storage_path(k_layout_dirs[i], path, sizeof(path)) != GLK_OK) {
            fail++;
            continue;
        }
        if (mkdir_p_one(path) == GLK_OK) ok++;
        else fail++;
    }
    if (ok == 0) return GLK_ERR_GENERIC;
    if (fail > 0) {
        s_mode = GLK_STOR_DEGRADED;
        return GLK_ERR_DEGRADED;
    }
    return GLK_OK;
}

glk_err_t glk_storage_path(const char* rel, char* out, size_t out_len) {
    if (!rel || !out || out_len < 8) return GLK_ERR_INVAL;
    if (!s_root[0]) return GLK_ERR_NOTFOUND;
    if (rel[0] == '/' || rel[0] == '\\') rel++;
    /* reject path escape */
    if (strstr(rel, "..")) return GLK_ERR_DENIED;
    snprintf(out, out_len, "%s/%s", s_root, rel);
    win_slash(out);
    return GLK_OK;
}

glk_err_t glk_storage_write_file(const char* rel, const void* data, size_t len) {
    char path[512], tmp[520];
    if (!glk_storage_usable()) return GLK_ERR_DEGRADED;
    if (s_mode == GLK_STOR_CORRUPT) return GLK_ERR_CORRUPT;
    if (s_mode == GLK_STOR_READONLY) return GLK_ERR_DENIED;
    if (glk_storage_path(rel, path, sizeof(path)) != GLK_OK) return GLK_ERR_INVAL;
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE* f = fopen(tmp, "wb");
    if (!f) {
        s_write_errors++;
        s_mode = GLK_STOR_FULL; /* best-effort classification */
        return GLK_ERR_FULL;
    }
    if (len && data) {
        if (fwrite(data, 1, len, f) != len) {
            fclose(f);
            remove(tmp);
            s_write_errors++;
            s_mode = GLK_STOR_FULL;
            return GLK_ERR_FULL;
        }
    }
    if (fflush(f) != 0) {
        fclose(f);
        remove(tmp);
        s_write_errors++;
        return GLK_ERR_GENERIC;
    }
    fclose(f);
#ifdef _WIN32
    remove(path);
    if (rename(tmp, path) != 0) {
        s_write_errors++;
        return GLK_ERR_GENERIC;
    }
#else
    if (rename(tmp, path) != 0) {
        s_write_errors++;
        return GLK_ERR_GENERIC;
    }
#endif
    if (s_mode == GLK_STOR_FULL) s_mode = GLK_STOR_OK;
    return GLK_OK;
}

glk_err_t glk_storage_append_file(const char* rel, const void* data, size_t len) {
    char path[512];
    if (!glk_storage_usable()) return GLK_ERR_DEGRADED;
    if (s_mode == GLK_STOR_CORRUPT) return GLK_ERR_CORRUPT;
    if (s_mode == GLK_STOR_READONLY) return GLK_ERR_DENIED;
    if (!data && len) return GLK_ERR_INVAL;
    if (glk_storage_path(rel, path, sizeof(path)) != GLK_OK) return GLK_ERR_INVAL;
    FILE* f = fopen(path, "ab");
    if (!f) {
        s_write_errors++;
        return GLK_ERR_GENERIC;
    }
    if (len) {
        if (fwrite(data, 1, len, f) != len) {
            fclose(f);
            s_write_errors++;
            s_mode = GLK_STOR_FULL;
            return GLK_ERR_FULL;
        }
    }
    fclose(f);
    return GLK_OK;
}

glk_err_t glk_storage_read_file(const char* rel, void* data, size_t cap, size_t* out_len) {
    char path[512];
    if (!s_present) return GLK_ERR_NOTFOUND;
    if (glk_storage_path(rel, path, sizeof(path)) != GLK_OK) return GLK_ERR_INVAL;
    FILE* f = fopen(path, "rb");
    if (!f) return GLK_ERR_NOTFOUND;
    size_t n = 0;
    if (data && cap) n = fread(data, 1, cap, f);
    fclose(f);
    if (out_len) *out_len = n;
    return GLK_OK;
}

bool glk_storage_exists(const char* rel) {
    char path[512];
    if (glk_storage_path(rel, path, sizeof(path)) != GLK_OK) return false;
    return GLK_ACCESS(path);
}

glk_err_t glk_storage_remove(const char* rel) {
    char path[512];
    if (!glk_storage_usable()) return GLK_ERR_DEGRADED;
    if (glk_storage_path(rel, path, sizeof(path)) != GLK_OK) return GLK_ERR_INVAL;
    if (remove(path) != 0) return GLK_ERR_NOTFOUND;
    return GLK_OK;
}

glk_err_t glk_storage_file_hash32(const char* rel, uint32_t* out_hash) {
    if (!out_hash) return GLK_ERR_INVAL;
    char path[512];
    if (glk_storage_path(rel, path, sizeof(path)) != GLK_OK) return GLK_ERR_INVAL;
    FILE* f = fopen(path, "rb");
    if (!f) return GLK_ERR_NOTFOUND;
    uint32_t h = 2166136261u;
    uint8_t buf[256];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            h ^= buf[i];
            h *= 16777619u;
        }
    }
    fclose(f);
    *out_hash = h;
    return GLK_OK;
}

glk_err_t glk_storage_write_integrity(const char* rel_marker, uint32_t hash) {
    char line[48];
    int n = snprintf(line, sizeof(line), "GLK1 %08x\n", (unsigned)hash);
    if (n <= 0) return GLK_ERR_GENERIC;
    return glk_storage_write_file(rel_marker, line, (size_t)n);
}

glk_err_t glk_storage_check_integrity(const char* rel_marker, uint32_t expected, bool fail_closed) {
    char buf[64];
    size_t n = 0;
    if (glk_storage_read_file(rel_marker, buf, sizeof(buf) - 1, &n) != GLK_OK) {
        if (fail_closed) {
            s_mode = GLK_STOR_CORRUPT;
            return GLK_ERR_CORRUPT;
        }
        return GLK_ERR_NOTFOUND;
    }
    buf[n] = 0;
    unsigned got = 0;
    if (sscanf(buf, "GLK1 %x", &got) != 1) {
        if (fail_closed) {
            s_mode = GLK_STOR_CORRUPT;
            return GLK_ERR_CORRUPT;
        }
        return GLK_ERR_CORRUPT;
    }
    if ((uint32_t)got != expected) {
        if (fail_closed) s_mode = GLK_STOR_CORRUPT;
        return GLK_ERR_CORRUPT;
    }
    return GLK_OK;
}

size_t glk_storage_status_json(char* buf, size_t buflen) {
    if (!buf || buflen < 8) return 0;
    return (size_t)snprintf(
        buf,
        buflen,
        "{\"present\":%s,\"mode\":\"%s\",\"usable\":%s,\"layout_ok\":%s,"
        "\"write_errors\":%u,\"root\":\"%s\"}",
        s_present ? "true" : "false",
        glk_storage_mode_str(s_mode),
        glk_storage_usable() ? "true" : "false",
        s_layout_ok ? "true" : "false",
        (unsigned)s_write_errors,
        s_root[0] ? s_root : "");
}
