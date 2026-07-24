#include "glk_svc/glk_skill.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(GLK_PLATFORM_STM32) && !defined(GLK_PLATFORM_HOST)
/* no filesystem scan on bare metal yet */
#elif defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static glk_skill_t s_skills[GLK_MAX_SKILLS];
static size_t s_count;

glk_err_t glk_skill_init(void) {
    memset(s_skills, 0, sizeof(s_skills));
    s_count = 0;
    return GLK_OK;
}

glk_err_t glk_skill_register(const char* id, const char* version, glk_risk_t risk) {
    if (!id || !id[0]) return GLK_ERR_INVAL;
    if (glk_skill_find(id)) return GLK_OK; /* already present */
    if (s_count >= GLK_MAX_SKILLS) return GLK_ERR_FULL;
    glk_skill_t* sk = &s_skills[s_count++];
    memset(sk, 0, sizeof(*sk));
    strncpy(sk->id, id, sizeof(sk->id) - 1);
    if (version) strncpy(sk->version, version, sizeof(sk->version) - 1);
    else strncpy(sk->version, "0", sizeof(sk->version) - 1);
    sk->risk = risk;
    sk->loaded = true;
    sk->signed_ok = !GLK_FEATURE_SIGNED_SKILLS;
    strncpy(sk->path, "rom://catalog", sizeof(sk->path) - 1);
    return GLK_OK;
}

static glk_risk_t parse_risk(const char* s) {
    if (!s) return GLK_RISK_INFO;
    if (strstr(s, "active_tx")) return GLK_RISK_ACTIVE_TX;
    if (strstr(s, "gpio")) return GLK_RISK_GPIO;
    if (strstr(s, "contact")) return GLK_RISK_CONTACT;
    if (strstr(s, "system")) return GLK_RISK_SYSTEM;
    if (strstr(s, "passive")) return GLK_RISK_PASSIVE_RX;
    return GLK_RISK_INFO;
}

static void load_manifest(const char* dir, const char* id) {
    if (s_count >= GLK_MAX_SKILLS) return;
    char path[256];
    snprintf(path, sizeof(path), "%s/%s/manifest.json", dir, id);
    FILE* f = fopen(path, "rb");
    if (!f) return;
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;

    glk_skill_t* sk = &s_skills[s_count++];
    memset(sk, 0, sizeof(*sk));
    strncpy(sk->id, id, sizeof(sk->id) - 1);
    strncpy(sk->path, path, sizeof(sk->path) - 1);
    sk->loaded = true;
    sk->signed_ok = !GLK_FEATURE_SIGNED_SKILLS; /* unsigned OK when enforce off */

    const char* v = strstr(buf, "\"version\"");
    if (v) {
        v = strchr(v + 9, '"');
        if (v) {
            v++;
            const char* e = strchr(v, '"');
            if (e) {
                size_t vn = (size_t)(e - v);
                if (vn >= sizeof(sk->version)) vn = sizeof(sk->version) - 1;
                memcpy(sk->version, v, vn);
            }
        }
    }
    const char* r = strstr(buf, "\"risk_class\"");
    if (!r) r = strstr(buf, "\"risk\"");
    if (r) sk->risk = parse_risk(r);
    else sk->risk = GLK_RISK_PASSIVE_RX;
}

glk_err_t glk_skill_scan(const char* skills_root) {
    if (!skills_root) return GLK_ERR_INVAL;
    s_count = 0;
#if defined(GLK_PLATFORM_STM32) && !defined(GLK_PLATFORM_HOST)
    (void)skills_root;
    return GLK_ERR_NOSUPPORT;
#elif defined(_WIN32)
    char pattern[300];
    snprintf(pattern, sizeof(pattern), "%s\\*", skills_root);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return GLK_ERR_NOTFOUND;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.cFileName[0] == '.') continue;
            load_manifest(skills_root, fd.cFileName);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(skills_root);
    if (!d) return GLK_ERR_NOTFOUND;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        load_manifest(skills_root, ent->d_name);
    }
    closedir(d);
#endif
    return GLK_OK;
}

size_t glk_skill_count(void) {
    return s_count;
}

const glk_skill_t* glk_skill_get(size_t index) {
    return index < s_count ? &s_skills[index] : NULL;
}

const glk_skill_t* glk_skill_find(const char* id) {
    if (!id) return NULL;
    for (size_t i = 0; i < s_count; i++) {
        if (strcmp(s_skills[i].id, id) == 0) return &s_skills[i];
    }
    return NULL;
}

size_t glk_skill_list(char* buf, size_t buflen) {
    if (!buf || buflen == 0) return 0;
    size_t off = 0;
    for (size_t i = 0; i < s_count && off + 40 < buflen; i++) {
        int n = snprintf(buf + off, buflen - off, "%s%s", i ? "," : "", s_skills[i].id);
        if (n > 0) off += (size_t)n;
    }
    return off;
}
