/**
 * Skill host - ROM + SD hot-load, refcount, optional package signature.
 */
#include "glk_svc/glk_skill.h"
#include "glk_svc/glk_storage.h"
#include "glk_svc/glk_audit.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(GLK_PLATFORM_STM32) && !defined(GLK_PLATFORM_HOST)
/* bare metal: ROM only unless storage FS lands */
#elif defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static glk_skill_t s_skills[GLK_MAX_SKILLS];
static size_t s_count;

const char* glk_skill_risk_str(glk_risk_t r) {
    switch (r) {
    case GLK_RISK_PASSIVE_RX: return "passive_rx";
    case GLK_RISK_ACTIVE_TX: return "active_tx";
    case GLK_RISK_GPIO: return "gpio";
    case GLK_RISK_CONTACT: return "contact";
    case GLK_RISK_SYSTEM: return "system";
    case GLK_RISK_INFO:
    default: return "info";
    }
}

glk_err_t glk_skill_init(void) {
    memset(s_skills, 0, sizeof(s_skills));
    s_count = 0;
    return GLK_OK;
}

static glk_skill_t* find_mut(const char* id) {
    if (!id) return NULL;
    for (size_t i = 0; i < s_count; i++) {
        if (strcmp(s_skills[i].id, id) == 0) return &s_skills[i];
    }
    return NULL;
}

glk_err_t glk_skill_register(const char* id, const char* version, glk_risk_t risk) {
    if (!id || !id[0]) return GLK_ERR_INVAL;
    if (find_mut(id)) return GLK_OK;
    if (s_count >= GLK_MAX_SKILLS) return GLK_ERR_FULL;
    glk_skill_t* sk = &s_skills[s_count++];
    memset(sk, 0, sizeof(*sk));
    strncpy(sk->id, id, sizeof(sk->id) - 1);
    if (version) strncpy(sk->version, version, sizeof(sk->version) - 1);
    else strncpy(sk->version, "0", sizeof(sk->version) - 1);
    sk->risk = risk;
    sk->loaded = true;
    sk->signed_ok = true; /* ROM trusted as built-in */
    sk->enforce_sig = false;
    sk->source = GLK_SKILL_SRC_ROM;
    sk->refcount = 1; /* ROM held */
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

static uint32_t hash_bytes(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

glk_err_t glk_skill_verify_package(
    const char* skill_dir,
    const char* id,
    uint32_t* out_hash,
    bool* out_ok) {
    if (!skill_dir || !id) return GLK_ERR_INVAL;
    char man[300];
    snprintf(man, sizeof(man), "%s/%s/manifest.json", skill_dir, id);
#ifdef _WIN32
    for (char* p = man; *p; p++)
        if (*p == '/') *p = '\\';
#endif
    FILE* f = fopen(man, "rb");
    if (!f) {
        if (out_ok) *out_ok = false;
        return GLK_ERR_NOTFOUND;
    }
    uint8_t buf[2048];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    uint32_t h = hash_bytes(buf, n);
    if (out_hash) *out_hash = h;

    char sigpath[320];
    snprintf(sigpath, sizeof(sigpath), "%s/%s/manifest.sig", skill_dir, id);
#ifdef _WIN32
    for (char* p = sigpath; *p; p++)
        if (*p == '/') *p = '\\';
#endif
    FILE* sf = fopen(sigpath, "rb");
    bool ok = false;
    if (sf) {
        char sbuf[64];
        size_t sn = fread(sbuf, 1, sizeof(sbuf) - 1, sf);
        fclose(sf);
        sbuf[sn] = 0;
        unsigned got = 0;
        if (sscanf(sbuf, "GLKSIG1 %x", &got) == 1 && (uint32_t)got == h) {
            ok = true;
        }
    }
    if (out_ok) *out_ok = ok;
#if GLK_FEATURE_SIGNED_SKILLS
    return ok ? GLK_OK : GLK_ERR_DENIED;
#else
    (void)ok;
    return GLK_OK; /* soft-allow load; caller reads out_ok for verified flag */
#endif
}

static void load_manifest(const char* dir, const char* id) {
    if (s_count >= GLK_MAX_SKILLS) return;
    /* skip if already ROM-registered with same id - upgrade path/SD fields */
    glk_skill_t* existing = find_mut(id);

    char path[256];
    snprintf(path, sizeof(path), "%s/%s/manifest.json", dir, id);
#ifdef _WIN32
    for (char* p = path; *p; p++)
        if (*p == '/') *p = '\\';
#endif
    FILE* f = fopen(path, "rb");
    if (!f) return;
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;

    uint32_t ch = 0;
    bool sig_ok = false;
    glk_err_t ve = glk_skill_verify_package(dir, id, &ch, &sig_ok);
#if GLK_FEATURE_SIGNED_SKILLS
    if (ve != GLK_OK) {
        glk_audit_log(GLK_ACTOR_KERNEL, "skill_sig", GLK_RISK_SYSTEM, GLK_POLICY_DENY,
                      "signed_skills_enforce", id);
        return;
    }
#else
    (void)ve;
    if (!sig_ok) {
        glk_audit_log(GLK_ACTOR_KERNEL, "skill_sig", GLK_RISK_INFO, GLK_POLICY_ALLOW,
                      "unsigned_or_unverified", id);
    }
#endif

    glk_skill_t* sk = existing;
    if (!sk) {
        sk = &s_skills[s_count++];
        memset(sk, 0, sizeof(*sk));
        strncpy(sk->id, id, sizeof(sk->id) - 1);
    }
    strncpy(sk->path, path, sizeof(sk->path) - 1);
    sk->signed_ok = sig_ok;
#if GLK_FEATURE_SIGNED_SKILLS
    sk->loaded = sig_ok;
#else
    sk->loaded = true; /* development: unsigned packages load */
#endif
    sk->enforce_sig = (GLK_FEATURE_SIGNED_SKILLS != 0);
    sk->source = GLK_SKILL_SRC_SD;
    sk->content_hash = ch;
    if (sk->refcount == 0) sk->refcount = 1;

    char rules[280];
    snprintf(rules, sizeof(rules), "%s/%s/rules.json", dir, id);
#ifdef _WIN32
    for (char* p = rules; *p; p++)
        if (*p == '/') *p = '\\';
#endif
    FILE* rf = fopen(rules, "rb");
    if (rf) {
        fclose(rf);
        strncpy(sk->rules_path, rules, sizeof(sk->rules_path) - 1);
    }

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
                sk->version[vn] = 0;
            }
        }
    }
    if (!sk->version[0]) strncpy(sk->version, "0", sizeof(sk->version) - 1);

    const char* r = strstr(buf, "\"risk_class\"");
    if (!r) r = strstr(buf, "\"risk\"");
    if (r) sk->risk = parse_risk(r);
    else sk->risk = GLK_RISK_PASSIVE_RX;

    /* Safety: SD skill may not raise ceiling above active_tx without signed + enforce */
    if (sk->risk >= GLK_RISK_ACTIVE_TX && !sk->signed_ok && GLK_FEATURE_SIGNED_SKILLS) {
        sk->loaded = false;
        glk_audit_log(GLK_ACTOR_KERNEL, "skill_risk", GLK_RISK_ACTIVE_TX, GLK_POLICY_DENY,
                      "unsigned_tx_skill", id);
    }
}

glk_err_t glk_skill_scan(const char* skills_root) {
    if (!skills_root) return GLK_ERR_INVAL;
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

glk_err_t glk_skill_scan_storage(void) {
    if (!glk_storage_usable()) return GLK_ERR_DEGRADED;
    char path[512];
    if (glk_storage_path("skills", path, sizeof(path)) != GLK_OK) return GLK_ERR_INVAL;
    return glk_skill_scan(path);
}

glk_err_t glk_skill_retain(const char* id) {
    glk_skill_t* sk = find_mut(id);
    if (!sk || !sk->loaded) return GLK_ERR_NOTFOUND;
    if (sk->refcount < 0xFFFFu) sk->refcount++;
    return GLK_OK;
}

glk_err_t glk_skill_release(const char* id) {
    glk_skill_t* sk = find_mut(id);
    if (!sk) return GLK_ERR_NOTFOUND;
    if (sk->source == GLK_SKILL_SRC_ROM) return GLK_OK; /* never drop ROM */
    if (sk->refcount > 0) sk->refcount--;
    if (sk->refcount == 0) return glk_skill_unload(id);
    return GLK_OK;
}

glk_err_t glk_skill_unload(const char* id) {
    glk_skill_t* sk = find_mut(id);
    if (!sk) return GLK_ERR_NOTFOUND;
    if (sk->source == GLK_SKILL_SRC_ROM) return GLK_ERR_DENIED;
    if (sk->refcount > 1) return GLK_ERR_BUSY;
    /* compact array */
    size_t idx = (size_t)(sk - s_skills);
    for (size_t i = idx + 1; i < s_count; i++) s_skills[i - 1] = s_skills[i];
    s_count--;
    memset(&s_skills[s_count], 0, sizeof(s_skills[0]));
    glk_audit_log(GLK_ACTOR_KERNEL, "skill_unload", GLK_RISK_INFO, GLK_POLICY_ALLOW,
                  "hot_unload", id);
    return GLK_OK;
}

size_t glk_skill_count(void) {
    return s_count;
}

size_t glk_skill_loaded_count(void) {
    size_t n = 0;
    for (size_t i = 0; i < s_count; i++)
        if (s_skills[i].loaded) n++;
    return n;
}

const glk_skill_t* glk_skill_get(size_t index) {
    return index < s_count ? &s_skills[index] : NULL;
}

const glk_skill_t* glk_skill_find(const char* id) {
    return find_mut(id);
}

size_t glk_skill_list(char* buf, size_t buflen) {
    if (!buf || buflen == 0) return 0;
    size_t off = 0;
    for (size_t i = 0; i < s_count && off + 40 < buflen; i++) {
        if (!s_skills[i].loaded) continue;
        int n = snprintf(buf + off, buflen - off, "%s%s", off ? "," : "", s_skills[i].id);
        if (n > 0) off += (size_t)n;
    }
    return off;
}

size_t glk_skill_status_json(char* buf, size_t buflen) {
    if (!buf || buflen < 8) return 0;
    char list[400];
    glk_skill_list(list, sizeof(list));
    return (size_t)snprintf(
        buf,
        buflen,
        "{\"count\":%u,\"loaded\":%u,\"signed_enforce\":%s,\"ids\":\"%s\"}",
        (unsigned)s_count,
        (unsigned)glk_skill_loaded_count(),
        GLK_FEATURE_SIGNED_SKILLS ? "true" : "false",
        list);
}
