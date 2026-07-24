/**
 * GrokAgent — event-driven mission interpreter.
 */
#include "glk_svc/glk_agent.h"
#include "glk_drv/glk_radio.h"
#include "glk_svc/glk_audit.h"
#include "glk_svc/glk_ml.h"
#include "glk_svc/glk_vault.h"
#include "glk/glk_kernel.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static glk_agent_t* s_global;
static volatile int s_agent_stop;

static glk_mission_t* find_mut(glk_agent_t* ag, const char* id) {
    if (!ag || !id) return NULL;
    for (size_t i = 0; i < ag->mission_count; i++) {
        if (strcmp(ag->missions[i].id, id) == 0) return &ag->missions[i];
    }
    return NULL;
}

void glk_agent_set_global(glk_agent_t* ag) {
    s_global = ag;
}

glk_agent_t* glk_agent_global(void) {
    return s_global;
}

glk_err_t glk_agent_init(glk_agent_t* ag, glk_policy_state_t* policy) {
    if (!ag || !policy) return GLK_ERR_INVAL;
    memset(ag, 0, sizeof(*ag));
    ag->policy = policy;
    ag->offline_enabled = false;
    glk_vault_init();
    return GLK_OK;
}

void glk_agent_set_offline(glk_agent_t* ag, bool on) {
    if (!ag) return;
    ag->offline_enabled = on;
}

bool glk_agent_offline(const glk_agent_t* ag) {
    return ag && ag->offline_enabled;
}

const glk_mission_t* glk_agent_get(const glk_agent_t* ag, const char* id) {
    if (!ag || !id) return NULL;
    for (size_t i = 0; i < ag->mission_count; i++) {
        if (strcmp(ag->missions[i].id, id) == 0) return &ag->missions[i];
    }
    return NULL;
}

glk_err_t glk_agent_set_autonomous(glk_agent_t* ag, const char* id, bool on) {
    glk_mission_t* m = find_mut(ag, id);
    if (!m) return GLK_ERR_NOTFOUND;
    m->autonomous = on;
    if (on) {
        /* Arm if idle so background tick can start */
        if (m->state == GLK_MISSION_IDLE || m->state == GLK_MISSION_DONE) {
            return glk_agent_arm(ag, id);
        }
    }
    return GLK_OK;
}

glk_err_t glk_agent_load_mission(glk_agent_t* ag, const glk_mission_t* m) {
    if (!ag || !m || !m->id[0]) return GLK_ERR_INVAL;
    glk_mission_t* slot = find_mut(ag, m->id);
    if (!slot) {
        if (ag->mission_count >= GLK_MAX_MISSIONS) return GLK_ERR_FULL;
        slot = &ag->missions[ag->mission_count++];
    }
    *slot = *m;
    slot->loaded = true;
    slot->state = GLK_MISSION_IDLE;
    slot->pc = 0;
    return GLK_OK;
}

/* Very small JSON helpers for mission files */
static const char* find_str(const char* json, const char* key, char* out, size_t outlen) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return NULL;
    p = strchr(p + strlen(pat), '"');
    if (!p) return NULL;
    p++;
    const char* e = strchr(p, '"');
    if (!e) return NULL;
    size_t n = (size_t)(e - p);
    if (n >= outlen) n = outlen - 1;
    memcpy(out, p, n);
    out[n] = 0;
    return e;
}

static int find_bool(const char* json, const char* key, bool* out) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "true", 4) == 0) {
        *out = true;
        return 1;
    }
    if (strncmp(p, "false", 5) == 0) {
        *out = false;
        return 1;
    }
    return 0;
}

glk_err_t glk_agent_load_mission_file(glk_agent_t* ag, const char* path) {
    if (!ag || !path) return GLK_ERR_INVAL;
    FILE* f = fopen(path, "rb");
    if (!f) return GLK_ERR_NOTFOUND;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024) {
        fclose(f);
        return GLK_ERR_CORRUPT;
    }
    char* json = (char*)malloc((size_t)sz + 1);
    if (!json) {
        fclose(f);
        return GLK_ERR_NOMEM;
    }
    fread(json, 1, (size_t)sz, f);
    json[sz] = 0;
    fclose(f);

    glk_mission_t m;
    memset(&m, 0, sizeof(m));
    if (!find_str(json, "id", m.id, sizeof(m.id))) {
        /* try "name" */
        if (!find_str(json, "name", m.id, sizeof(m.id))) {
            free(json);
            return GLK_ERR_CORRUPT;
        }
    }
    find_bool(json, "autonomous", &m.autonomous);
    m.step_timeout_ms = 10000;
    m.wall_deadline_ms = 120000;

    /* Parse steps array: look for "op":"subghz_rx" patterns */
    const char* p = strstr(json, "\"steps\"");
    if (p) {
        while (m.step_count < GLK_MAX_MISSION_STEPS && (p = strstr(p, "\"op\"")) != NULL) {
            char op[32];
            if (!find_str(p, "op", op, sizeof(op))) break;
            glk_mstep_t* s = &m.steps[m.step_count];
            memset(s, 0, sizeof(*s));
            if (strcmp(op, "sleep") == 0 || strcmp(op, "sleep_ms") == 0) {
                s->op = GLK_OP_SLEEP_MS;
                s->a = 1000;
                const char* ms = strstr(p, "\"ms\"");
                if (ms) s->a = (uint32_t)strtoul(strchr(ms, ':') + 1, NULL, 10);
            } else if (strcmp(op, "subghz_rx") == 0 || strcmp(op, "rx") == 0) {
                s->op = GLK_OP_SUBGHZ_RX;
                s->a = 433920000;
                s->b = 500;
                const char* fh = strstr(p, "\"freq_hz\"");
                if (fh) s->a = (uint32_t)strtoul(strchr(fh, ':') + 1, NULL, 10);
                const char* dm = strstr(p, "\"ms\"");
                if (dm) s->b = (uint32_t)strtoul(strchr(dm, ':') + 1, NULL, 10);
            } else if (strcmp(op, "log") == 0) {
                s->op = GLK_OP_LOG;
                find_str(p, "msg", s->note, sizeof(s->note));
            } else if (strcmp(op, "infer") == 0) {
                s->op = GLK_OP_INFER;
            } else if (strcmp(op, "loop") == 0) {
                s->op = GLK_OP_LOOP_BEGIN;
                s->b = 2;
                const char* c = strstr(p, "\"count\"");
                if (c) s->b = (uint32_t)strtoul(strchr(c, ':') + 1, NULL, 10);
            } else if (strcmp(op, "end_loop") == 0) {
                s->op = GLK_OP_LOOP_END;
            } else if (strcmp(op, "if_pulses_gt") == 0) {
                s->op = GLK_OP_IF_PULSES_GT;
                s->a = 10;
                const char* th = strstr(p, "\"threshold\"");
                if (th) s->a = (uint32_t)strtoul(strchr(th, ':') + 1, NULL, 10);
            } else if (strcmp(op, "abort") == 0) {
                s->op = GLK_OP_ABORT;
            } else {
                s->op = GLK_OP_NOP;
            }
            m.step_count++;
            p += 4;
            /* stop at next step object roughly */
            const char* next = strstr(p, "\"op\"");
            if (!next) break;
            /* ensure we advance */
            if (next <= p) break;
        }
    }

    /* Default passive demo if no steps */
    if (m.step_count == 0) {
        m.steps[0].op = GLK_OP_LOG;
        strncpy(m.steps[0].note, "mission_start", sizeof(m.steps[0].note) - 1);
        m.steps[1].op = GLK_OP_SUBGHZ_RX;
        m.steps[1].a = 433920000;
        m.steps[1].b = 400;
        m.steps[2].op = GLK_OP_INFER;
        m.steps[3].op = GLK_OP_LOG;
        strncpy(m.steps[3].note, "mission_end", sizeof(m.steps[3].note) - 1);
        m.step_count = 4;
    }

    free(json);
    return glk_agent_load_mission(ag, &m);
}

glk_err_t glk_agent_arm(glk_agent_t* ag, const char* id) {
    glk_mission_t* m = find_mut(ag, id);
    if (!m) return GLK_ERR_NOTFOUND;
    m->state = GLK_MISSION_ARMED;
    m->pc = 0;
    m->loop_sp = 0;
    m->last_pulses = 0;
    strncpy(ag->active_id, id, sizeof(ag->active_id) - 1);
    return GLK_OK;
}

glk_err_t glk_agent_disarm(glk_agent_t* ag, const char* id) {
    if (!ag) return GLK_ERR_INVAL;
    glk_mission_t* m = id && id[0] ? find_mut(ag, id) : find_mut(ag, ag->active_id);
    if (!m) return GLK_ERR_NOTFOUND;
    m->state = GLK_MISSION_IDLE;
    m->pc = 0;
    if (ag->active_id[0] && strcmp(ag->active_id, m->id) == 0) ag->active_id[0] = 0;
    return GLK_OK;
}

glk_err_t glk_agent_run_steps(glk_agent_t* ag, uint32_t max_steps, uint32_t* out_ran) {
    if (!ag) return GLK_ERR_INVAL;
    if (max_steps == 0) max_steps = 1;
    if (max_steps > 32) max_steps = 32; /* USB-safe bound */
    uint32_t ran = 0;
    for (uint32_t i = 0; i < max_steps; i++) {
        glk_mission_t* m = find_mut(ag, ag->active_id);
        if (!m) break;
        if (m->state == GLK_MISSION_DONE || m->state == GLK_MISSION_ERROR || m->state == GLK_MISSION_IDLE)
            break;
        glk_err_t e = glk_agent_run_once(ag);
        ran++;
        if (e != GLK_OK) {
            if (out_ran) *out_ran = ran;
            return e;
        }
        m = find_mut(ag, ag->active_id);
        if (m && (m->state == GLK_MISSION_DONE || m->state == GLK_MISSION_ERROR)) break;
    }
    if (out_ran) *out_ran = ran;
    return GLK_OK;
}

static const char* mission_state_str(glk_mission_state_t s) {
    switch (s) {
    case GLK_MISSION_IDLE: return "idle";
    case GLK_MISSION_ARMED: return "armed";
    case GLK_MISSION_RUNNING: return "running";
    case GLK_MISSION_PAUSED: return "paused";
    case GLK_MISSION_DONE: return "done";
    case GLK_MISSION_ERROR: return "error";
    default: return "unknown";
    }
}

size_t glk_agent_status_json(const glk_agent_t* ag, const char* id, char* buf, size_t buflen) {
    if (!ag || !buf || buflen < 32) return 0;
    const char* mid = (id && id[0]) ? id : ag->active_id;
    const glk_mission_t* m = glk_agent_get(ag, mid);
    if (!m) {
        return (size_t)snprintf(buf, buflen, "{\"ok\":false,\"error\":\"not_found\"}");
    }
    return (size_t)snprintf(
        buf,
        buflen,
        "{\"ok\":true,\"id\":\"%s\",\"state\":\"%s\",\"pc\":%u,\"steps\":%u,"
        "\"last_pulses\":%ld,\"last_infer\":%ld,\"score\":%.3f,\"active\":\"%s\"}",
        m->id,
        mission_state_str(m->state),
        (unsigned)m->pc,
        (unsigned)m->step_count,
        (long)m->last_pulses,
        (long)m->last_infer_label,
        (double)m->last_infer_score,
        ag->active_id);
}

glk_err_t glk_agent_run_once(glk_agent_t* ag) {
    if (!ag || !ag->active_id[0]) return GLK_ERR_INVAL;
    glk_mission_t* m = find_mut(ag, ag->active_id);
    if (!m) return GLK_ERR_NOTFOUND;
    if (m->state != GLK_MISSION_ARMED && m->state != GLK_MISSION_RUNNING) return GLK_ERR_BUSY;
    m->state = GLK_MISSION_RUNNING;
    if (m->pc >= m->step_count) {
        m->state = GLK_MISSION_DONE;
        return GLK_OK;
    }
    glk_mstep_t* s = &m->steps[m->pc];
    switch (s->op) {
    case GLK_OP_NOP:
        m->pc++;
        break;
    case GLK_OP_SLEEP_MS:
        glk_task_sleep_ms(s->a > 5000 ? 5000 : s->a);
        m->pc++;
        break;
    case GLK_OP_SUBGHZ_RX: {
        glk_radio_result_t r;
        glk_err_t e = glk_radio_rx_sync(GLK_ACTOR_AGENT, s->a, s->b, &r);
        if (e == GLK_OK) m->last_pulses = r.pulses;
        glk_vault_push(m->id, "rx", m->last_pulses, m->last_infer_label, m->last_infer_score);
#if defined(GLK_PLATFORM_STM32) && !defined(GLK_PLATFORM_HOST)
        {
            extern void glk_gui_notify_rx(int pulses, int rssi, bool sim);
            glk_gui_notify_rx((int)m->last_pulses, (int)r.rssi_est, r.simulated);
        }
#endif
        m->pc++;
        break;
    }
    case GLK_OP_SUBGHZ_TX:
        /* Agent cannot TX without external confirm in policy — will deny. */
        {
            glk_radio_result_t r;
            glk_radio_tx_sync(GLK_ACTOR_AGENT, s->a, NULL, NULL, &r);
            m->pc++;
        }
        break;
    case GLK_OP_LOG:
        glk_audit_log(GLK_ACTOR_AGENT, "mission_log", GLK_RISK_INFO, GLK_POLICY_ALLOW, s->note, m->id);
        m->pc++;
        break;
    case GLK_OP_IF_PULSES_GT:
        if (m->last_pulses > (int32_t)s->a) {
            m->pc++;
        } else {
            m->pc += 2; /* skip next */
            if (m->pc > m->step_count) m->pc = m->step_count;
        }
        break;
    case GLK_OP_LOOP_BEGIN:
        if (m->loop_sp < 8) {
            m->loop_stack[m->loop_sp] = (uint16_t)(m->pc + 1);
            m->loop_remain[m->loop_sp] = (uint16_t)(s->b ? s->b : 1);
            m->loop_sp++;
        }
        m->pc++;
        break;
    case GLK_OP_LOOP_END:
        if (m->loop_sp > 0) {
            uint8_t sp = (uint8_t)(m->loop_sp - 1);
            if (m->loop_remain[sp] > 1) {
                m->loop_remain[sp]--;
                m->pc = m->loop_stack[sp];
            } else {
                m->loop_sp--;
                m->pc++;
            }
        } else {
            m->pc++;
        }
        break;
    case GLK_OP_INFER: {
        glk_ml_result_t mr;
        float feats[8];
        feats[0] = (float)m->last_pulses;
        feats[1] = (float)(m->last_pulses % 7);
        glk_ml_infer(feats, 2, &mr);
        m->last_infer_label = mr.label;
        m->last_infer_score = mr.score;
        /* ML never actuates — log only */
        char det[64];
        snprintf(det, sizeof(det), "label=%d score=%.3f", mr.label, mr.score);
        glk_audit_log(GLK_ACTOR_AGENT, "ml_infer", GLK_RISK_INFO, GLK_POLICY_ALLOW, det, m->id);
        glk_vault_push(m->id, "infer", m->last_pulses, m->last_infer_label, m->last_infer_score);
        m->pc++;
        break;
    }
    case GLK_OP_ABORT:
        m->state = GLK_MISSION_ERROR;
        return GLK_ERR_GENERIC;
    case GLK_OP_PARALLEL_BEGIN:
    case GLK_OP_PARALLEL_END:
    case GLK_OP_RESERVE:
    case GLK_OP_RELEASE:
        /* Reserved for multi-resource missions; treat as NOP in v3.0.0 */
        m->pc++;
        break;
    default:
        m->pc++;
        break;
    }
    if (m->pc >= m->step_count) {
        m->state = GLK_MISSION_DONE;
        glk_vault_push(m->id, "done", m->last_pulses, m->last_infer_label, m->last_infer_score);
        ag->cycles_done++;
    }
    ag->steps_total++;
    return GLK_OK;
}

glk_err_t glk_agent_tick(glk_agent_t* ag) {
    if (!ag) return GLK_ERR_INVAL;
    ag->ticks++;
    if (!ag->active_id[0]) {
        /* arm first autonomous mission */
        for (size_t i = 0; i < ag->mission_count; i++) {
            if (ag->missions[i].autonomous &&
                (ag->missions[i].state == GLK_MISSION_IDLE || ag->missions[i].state == GLK_MISSION_DONE)) {
                glk_agent_arm(ag, ag->missions[i].id);
                glk_vault_push(ag->missions[i].id, "auto", 0, 0, 0.f);
                break;
            }
        }
    }
    glk_mission_t* m = find_mut(ag, ag->active_id);
    if (m && m->state == GLK_MISSION_DONE && m->autonomous) {
        /* Continuous offline cycle */
        glk_agent_arm(ag, m->id);
        m = find_mut(ag, ag->active_id);
    }
    if (m && (m->state == GLK_MISSION_ARMED || m->state == GLK_MISSION_RUNNING)) {
        return glk_agent_run_once(ag);
    }
    return GLK_OK;
}

glk_err_t glk_agent_poll_usb_safe(glk_agent_t* ag) {
    if (!ag || !ag->offline_enabled) return GLK_OK;
    /* Single step only — caller throttles cadence for USB fairness */
    return glk_agent_tick(ag);
}

size_t glk_agent_snapshot_json(const glk_agent_t* ag, char* buf, size_t buflen) {
    if (!ag || !buf || buflen < 48) return 0;
    const glk_mission_t* m = glk_agent_get(ag, ag->active_id);
    return (size_t)snprintf(
        buf,
        buflen,
        "{\"ok\":true,\"offline\":%s,\"running\":%s,\"ticks\":%u,\"steps_total\":%u,"
        "\"cycles\":%u,\"missions\":%u,\"active\":\"%s\",\"active_state\":\"%s\","
        "\"last_pulses\":%ld,\"vault\":%u}",
        ag->offline_enabled ? "true" : "false",
        ag->running ? "true" : "false",
        (unsigned)ag->ticks,
        (unsigned)ag->steps_total,
        (unsigned)ag->cycles_done,
        (unsigned)ag->mission_count,
        ag->active_id,
        m ? mission_state_str(m->state) : "none",
        m ? (long)m->last_pulses : 0L,
        (unsigned)glk_vault_count());
}

static void agent_task(void* arg) {
    glk_agent_t* ag = (glk_agent_t*)arg;
    ag->running = true;
    while (!s_agent_stop) {
        glk_agent_tick(ag);
        glk_task_sleep_ms(50); /* event-driven cadence (not 1 Hz only) */
    }
    ag->running = false;
}

glk_err_t glk_agent_start_task(glk_agent_t* ag) {
    s_agent_stop = 0;
    glk_task_t* t = NULL;
    return glk_task_create(&t, "agent", agent_task, ag, GLK_PRIO_AGENT, 2048);
}

void glk_agent_stop(glk_agent_t* ag) {
    s_agent_stop = 1;
    if (ag) ag->running = false;
}

size_t glk_agent_list(const glk_agent_t* ag, char* buf, size_t buflen) {
    if (!ag || !buf || buflen == 0) return 0;
    size_t off = 0;
    for (size_t i = 0; i < ag->mission_count && off + 40 < buflen; i++) {
        int n = snprintf(buf + off, buflen - off, "%s%s", i ? "," : "", ag->missions[i].id);
        if (n > 0) off += (size_t)n;
    }
    return off;
}
