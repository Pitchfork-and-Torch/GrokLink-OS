/**
 * GrokAgent native — multi-step mission engine with IR opcodes.
 */
#pragma once

#include "glk/glk_types.h"
#include "glk/glk_config.h"
#include "glk_svc/glk_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLK_OP_NOP = 0,
    GLK_OP_SLEEP_MS = 1,
    GLK_OP_SUBGHZ_RX = 2,
    GLK_OP_SUBGHZ_TX = 3,
    GLK_OP_GPIO_READ = 4,
    GLK_OP_GPIO_WRITE = 5,
    GLK_OP_IR_RX = 6,
    GLK_OP_NFC_POLL = 7,
    GLK_OP_LOG = 8,
    GLK_OP_IF_PULSES_GT = 9,
    GLK_OP_LOOP_BEGIN = 10,
    GLK_OP_LOOP_END = 11,
    GLK_OP_PARALLEL_BEGIN = 12,
    GLK_OP_PARALLEL_END = 13,
    GLK_OP_RESERVE = 14,
    GLK_OP_RELEASE = 15,
    GLK_OP_INFER = 16,
    GLK_OP_ABORT = 17,
    GLK_OP_END = 255,
} glk_opcode_t;

typedef struct {
    uint8_t op;
    uint32_t a; /* freq_hz / pin / ms / threshold */
    uint32_t b; /* duration_ms / value / loop count */
    int32_t c;  /* jump / skill index */
    char note[48];
} glk_mstep_t;

typedef enum {
    GLK_MISSION_IDLE = 0,
    GLK_MISSION_ARMED = 1,
    GLK_MISSION_RUNNING = 2,
    GLK_MISSION_PAUSED = 3,
    GLK_MISSION_DONE = 4,
    GLK_MISSION_ERROR = 5,
} glk_mission_state_t;

typedef struct {
    char id[GLK_MISSION_ID_MAX];
    bool autonomous;
    bool loaded;
    glk_mission_state_t state;
    uint16_t step_count;
    uint16_t pc;
    uint16_t loop_stack[8];
    uint8_t loop_sp;
    uint16_t loop_remain[8];
    int32_t last_pulses;
    int32_t last_infer_label;
    float last_infer_score;
    uint32_t step_timeout_ms;
    uint32_t wall_deadline_ms;
    glk_mstep_t steps[GLK_MAX_MISSION_STEPS];
} glk_mission_t;

typedef struct {
    bool running;
    bool offline_enabled; /* master switch for USB-safe background ticks */
    glk_policy_state_t* policy;
    glk_mission_t missions[GLK_MAX_MISSIONS];
    size_t mission_count;
    char active_id[GLK_MISSION_ID_MAX];
    uint32_t ticks;
    uint32_t steps_total;
    uint32_t cycles_done; /* autonomous restarts */
} glk_agent_t;

glk_err_t glk_agent_init(glk_agent_t* ag, glk_policy_state_t* policy);
glk_err_t glk_agent_start_task(glk_agent_t* ag);
void glk_agent_stop(glk_agent_t* ag);

/** Enable/disable background offline ticking (device main loop / host task). */
void glk_agent_set_offline(glk_agent_t* ag, bool on);
bool glk_agent_offline(const glk_agent_t* ag);

/** Mark mission autonomous (will re-arm when DONE if offline enabled). */
glk_err_t glk_agent_set_autonomous(glk_agent_t* ag, const char* id, bool on);

/** Load a simple mission (programmatic). */
glk_err_t glk_agent_load_mission(glk_agent_t* ag, const glk_mission_t* m);

/** Parse minimal mission JSON from file (v2-compatible subset + OS extensions). */
glk_err_t glk_agent_load_mission_file(glk_agent_t* ag, const char* path);

glk_err_t glk_agent_arm(glk_agent_t* ag, const char* id);
glk_err_t glk_agent_disarm(glk_agent_t* ag, const char* id);
glk_err_t glk_agent_run_once(glk_agent_t* ag); /* single step of active mission */
/** Run up to max_steps of the active mission (or until DONE/ERROR). */
glk_err_t glk_agent_run_steps(glk_agent_t* ag, uint32_t max_steps, uint32_t* out_ran);
glk_err_t glk_agent_tick(glk_agent_t* ag);

size_t glk_agent_list(const glk_agent_t* ag, char* buf, size_t buflen);
const glk_mission_t* glk_agent_get(const glk_agent_t* ag, const char* id);

/** Compact status string for RPC (id,state,pc,steps,last_pulses). */
size_t glk_agent_status_json(const glk_agent_t* ag, const char* id, char* buf, size_t buflen);

/** Full agent snapshot for RPC agent_status. */
size_t glk_agent_snapshot_json(const glk_agent_t* ag, char* buf, size_t buflen);

/**
 * USB-safe cooperative tick: at most one IR step.
 * Call from device main loop when USB is idle (not from ISR).
 */
glk_err_t glk_agent_poll_usb_safe(glk_agent_t* ag);

void glk_agent_set_global(glk_agent_t* ag);
glk_agent_t* glk_agent_global(void);

#ifdef __cplusplus
}
#endif
