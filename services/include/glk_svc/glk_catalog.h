/**
 * Builtin ROM catalog — lab missions + skills without SD card.
 * Passive-only defaults; authorized educational use.
 */
#pragma once

#include "glk_svc/glk_agent.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register builtin passive lab skills and load default missions into the agent.
 * Safe to call with or without SD; does not replace already-loaded IDs.
 */
glk_err_t glk_catalog_load_defaults(glk_agent_t* ag);

/** Number of builtin skills that would be registered. */
size_t glk_catalog_builtin_skill_count(void);

/** Number of builtin missions that would be loaded. */
size_t glk_catalog_builtin_mission_count(void);

#ifdef __cplusplus
}
#endif
