/**
 * Hot-loadable skill registry with risk classification.
 */
#pragma once

#include "glk/glk_types.h"
#include "glk/glk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char id[GLK_SKILL_ID_MAX];
    char version[16];
    glk_risk_t risk;
    bool loaded;
    bool signed_ok;
    char path[192];
} glk_skill_t;

glk_err_t glk_skill_init(void);
glk_err_t glk_skill_scan(const char* skills_root);

/** Register a skill without filesystem (ROM catalog / host inject). No-op if id exists. */
glk_err_t glk_skill_register(const char* id, const char* version, glk_risk_t risk);

size_t glk_skill_count(void);
const glk_skill_t* glk_skill_get(size_t index);
const glk_skill_t* glk_skill_find(const char* id);
size_t glk_skill_list(char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif
