#include "glk_svc/glk_rpc.h"
#include "glk_drv/glk_radio.h"
#include "glk_hal/glk_hal_subghz.h"
#include "glk_svc/glk_audit.h"
#include "glk_svc/glk_skill.h"
#include "glk_svc/glk_vault.h"
#include "glk_svc/glk_catalog.h"
#include "glk/glk_config.h"
#include "glk/glk_kernel.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

glk_err_t glk_rpc_init(glk_rpc_t* rpc, glk_policy_state_t* pol, glk_agent_t* agent, const char* sd_root) {
    if (!rpc || !pol) return GLK_ERR_INVAL;
    memset(rpc, 0, sizeof(*rpc));
    rpc->policy = pol;
    rpc->agent = agent;
    if (sd_root) strncpy(rpc->sd_root, sd_root, sizeof(rpc->sd_root) - 1);
    return GLK_OK;
}

uint32_t glk_rpc_crc32(const void* data, size_t len) {
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

size_t glk_rpc_frame_encode(
    uint16_t msg_type,
    uint16_t seq,
    const void* payload,
    uint32_t len,
    uint8_t* out,
    size_t out_cap) {
    size_t need = 12 + len + 4;
    if (!out || out_cap < need) return 0;
    out[0] = GLK_RPC_MAGIC0;
    out[1] = GLK_RPC_MAGIC1;
    out[2] = GLK_RPC_VERSION;
    out[3] = 0;
    out[4] = (uint8_t)(msg_type & 0xFF);
    out[5] = (uint8_t)(msg_type >> 8);
    out[6] = (uint8_t)(seq & 0xFF);
    out[7] = (uint8_t)(seq >> 8);
    out[8] = (uint8_t)(len & 0xFF);
    out[9] = (uint8_t)((len >> 8) & 0xFF);
    out[10] = (uint8_t)((len >> 16) & 0xFF);
    out[11] = (uint8_t)((len >> 24) & 0xFF);
    if (len && payload) memcpy(out + 12, payload, len);
    uint32_t crc = glk_rpc_crc32(out, 12 + len);
    out[12 + len] = (uint8_t)(crc & 0xFF);
    out[13 + len] = (uint8_t)((crc >> 8) & 0xFF);
    out[14 + len] = (uint8_t)((crc >> 16) & 0xFF);
    out[15 + len] = (uint8_t)((crc >> 24) & 0xFF);
    return need;
}

glk_err_t glk_rpc_frame_decode(
    const uint8_t* in,
    size_t in_len,
    glk_rpc_hdr_t* hdr,
    const uint8_t** payload,
    uint32_t* payload_len) {
    if (!in || in_len < 16 || !hdr) return GLK_ERR_INVAL;
    if (in[0] != GLK_RPC_MAGIC0 || in[1] != GLK_RPC_MAGIC1) return GLK_ERR_CORRUPT;
    hdr->magic[0] = in[0];
    hdr->magic[1] = in[1];
    hdr->version = in[2];
    hdr->flags = in[3];
    hdr->msg_type = (uint16_t)(in[4] | (in[5] << 8));
    hdr->seq = (uint16_t)(in[6] | (in[7] << 8));
    hdr->length = (uint32_t)(in[8] | (in[9] << 8) | (in[10] << 16) | (in[11] << 24));
    if (12 + hdr->length + 4 > in_len) return GLK_ERR_CORRUPT;
    uint32_t crc_calc = glk_rpc_crc32(in, 12 + hdr->length);
    uint32_t crc_got = (uint32_t)(in[12 + hdr->length] | (in[13 + hdr->length] << 8) |
                                  (in[14 + hdr->length] << 16) | (in[15 + hdr->length] << 24));
    if (crc_calc != crc_got) return GLK_ERR_CORRUPT;
    if (payload) *payload = in + 12;
    if (payload_len) *payload_len = hdr->length;
    return GLK_OK;
}

static int json_get_str(const char* j, const char* key, char* out, size_t outlen) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), '"');
    if (!p) return 0;
    p++;
    const char* e = strchr(p, '"');
    if (!e) return 0;
    size_t n = (size_t)(e - p);
    if (n >= outlen) n = outlen - 1;
    memcpy(out, p, n);
    out[n] = 0;
    return 1;
}

static uint32_t json_get_u32(const char* j, const char* key, uint32_t def) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p) return def;
    p = strchr(p, ':');
    if (!p) return def;
    return (uint32_t)strtoul(p + 1, NULL, 10);
}

glk_err_t glk_rpc_handle_json(glk_rpc_t* rpc, const char* req_json, char* resp, size_t resp_len) {
    if (!rpc || !req_json || !resp || resp_len < 32) return GLK_ERR_INVAL;
    char cmd[64];
    cmd[0] = 0;
    if (!json_get_str(req_json, "cmd", cmd, sizeof(cmd))) {
        snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"missing_cmd\"}");
        return GLK_ERR_INVAL;
    }
    rpc->seq++;

    if (strcmp(cmd, "ping") == 0) {
        snprintf(resp, resp_len, "{\"ok\":true,\"cmd\":\"pong\",\"api\":%d,\"version\":\"%s\"}",
                 GLK_RPC_API, GLK_VERSION_STRING);
        return GLK_OK;
    }

    if (strcmp(cmd, "edu_ack") == 0) {
        char phrase[80];
        phrase[0] = 0;
        json_get_str(req_json, "phrase", phrase, sizeof(phrase));
        if (strcmp(phrase, GL_EDU_ACK_PHRASE) == 0) {
            glk_policy_set_edu_acked(rpc->policy, true);
            rpc->session_open = true;
            snprintf(resp, resp_len, "{\"ok\":true,\"edu\":true}");
        } else {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"bad_edu_phrase\"}");
        }
        return GLK_OK;
    }

    if (strcmp(cmd, "status") == 0) {
        glk_kernel_stats_t ks;
        glk_kernel_stats(&ks);
        snprintf(
            resp,
            resp_len,
            "{\"ok\":true,\"version\":\"%s\",\"api\":%d,\"edu\":%s,\"sd\":%s,"
            "\"blacklist_ok\":%s,\"radio\":%d,\"heap_free\":%u,\"missions\":%u,\"skills\":%u,"
            "\"profile\":\"%s\",\"medsec_strict\":%s,\"not_medical_device\":true,"
            "\"disclaimer\":\"NOT a medical device. Authorized research only.\"}",
            GLK_VERSION_STRING,
            GLK_RPC_API,
            glk_policy_edu_acked(rpc->policy) ? "true" : "false",
            rpc->policy->sd_present ? "true" : "false",
            rpc->policy->blacklist_ok ? "true" : "false",
            (int)glk_radio_state(),
            (unsigned)glk_heap_free_bytes(),
            rpc->agent ? (unsigned)rpc->agent->mission_count : 0,
            (unsigned)glk_skill_count(),
            glk_policy_profile_name(rpc->policy),
            glk_policy_medsec_strict(rpc->policy) ? "true" : "false");
        return GLK_OK;
    }

    if (strcmp(cmd, "confirm_issue") == 0) {
        char action[GLK_ACTION_MAX];
        action[0] = 0;
        json_get_str(req_json, "action", action, sizeof(action));
        uint32_t ttl = json_get_u32(req_json, "ttl_sec", GLK_CONFIRM_DEFAULT_TTL_SEC);
        uint32_t freq = json_get_u32(req_json, "freq_hz", 0);
        char id[24];
        if (glk_policy_issue_confirm(rpc->policy, action, ttl, freq, -1, id, sizeof(id))) {
            snprintf(resp, resp_len, "{\"ok\":true,\"confirm_id\":\"%s\",\"ttl\":%u}", id, ttl);
        } else {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"confirm_issue_failed\"}");
        }
        return GLK_OK;
    }

    if (strcmp(cmd, "subghz_rx") == 0) {
        uint32_t freq = json_get_u32(req_json, "freq_hz", 433920000);
        uint32_t ms = json_get_u32(req_json, "ms", 500);
        glk_radio_result_t r;
        glk_err_t e = glk_radio_rx_sync(GLK_ACTOR_RPC, freq, ms, &r);
        /* Compact device JSON; host packager builds full LLM observation schema. */
        /* v3.5: additive pulse_rate_hz + rssi_min/max for Signal Cognition host packager */
        snprintf(
            resp,
            resp_len,
            "{\"ok\":%s,\"err\":%d,\"freq_hz\":%u,\"ms\":%u,\"pulses\":%d,\"rssi\":%d,"
            "\"pulse_rate_hz\":%ld,\"rssi_min\":%d,\"rssi_max\":%d,"
            "\"sim\":%s,\"ts_ms\":%u,\"kind\":\"rx\"}",
            e == GLK_OK ? "true" : "false",
            (int)e,
            r.freq_hz,
            r.duration_ms,
            r.pulses,
            r.rssi_est,
            (long)r.pulse_rate_hz,
            (int)r.rssi_min,
            (int)r.rssi_max,
            r.simulated ? "true" : "false",
            (unsigned)glk_tick_get());
        return GLK_OK;
    }

    if (strcmp(cmd, "subghz_probe") == 0) {
        uint8_t part = 0, ver = 0;
        glk_err_t e = glk_hal_subghz_probe(&part, &ver);
        snprintf(
            resp,
            resp_len,
            "{\"ok\":%s,\"err\":%d,\"partnum\":%u,\"version\":%u,\"hw\":%s}",
            e == GLK_OK ? "true" : "false",
            (int)e,
            (unsigned)part,
            (unsigned)ver,
#if defined(GLK_RADIO_SIM) || !defined(GLK_PLATFORM_STM32) || defined(GLK_PLATFORM_HOST)
            "false"
#else
            "true"
#endif
        );
        return GLK_OK;
    }

    if (strcmp(cmd, "subghz_tx") == 0) {
        uint32_t freq = json_get_u32(req_json, "freq_hz", 433920000);
        char path[192], conf[24];
        path[0] = conf[0] = 0;
        json_get_str(req_json, "path", path, sizeof(path));
        json_get_str(req_json, "confirm_id", conf, sizeof(conf));
        glk_radio_result_t r;
        glk_err_t e = glk_radio_tx_sync(GLK_ACTOR_RPC, freq, path, conf, &r);
        snprintf(
            resp,
            resp_len,
            "{\"ok\":%s,\"err\":%d,\"tx_mode\":\"ack_file\",\"freq_hz\":%u}",
            e == GLK_OK ? "true" : "false",
            (int)e,
            freq);
        return GLK_OK;
    }

    if (strcmp(cmd, "spectrum") == 0) {
        uint32_t freqs[GLK_SPECTRUM_MAX_BANDS];
        size_t n = 0;
        /* accept "freqs":[a,b,c] rough parse */
        const char* p = strstr(req_json, "\"freqs\"");
        if (p) {
            p = strchr(p, '[');
            if (p) {
                p++;
                while (n < GLK_SPECTRUM_MAX_BANDS && *p && *p != ']') {
                    while (*p == ' ' || *p == ',') p++;
                    if (*p >= '0' && *p <= '9') {
                        freqs[n++] = (uint32_t)strtoul(p, (char**)&p, 10);
                    } else {
                        p++;
                    }
                }
            }
        }
        if (n == 0) {
            freqs[0] = 433920000;
            freqs[1] = 315000000;
            n = 2;
        }
        uint32_t dwell = json_get_u32(req_json, "ms", 400);
        uint32_t settle = json_get_u32(req_json, "settle_ms", 2000);
        glk_radio_result_t results[GLK_SPECTRUM_MAX_BANDS];
        size_t out_n = 0;
        glk_radio_spectrum(GLK_ACTOR_RPC, freqs, n, dwell, settle, results, &out_n);
        /* compact response — include rssi when available for host observability */
        size_t off = 0;
        off += (size_t)snprintf(
            resp + off,
            resp_len - off,
            "{\"ok\":true,\"kind\":\"spectrum\",\"ms\":%u,\"settle_ms\":%u,\"ts_ms\":%u,\"bands\":[",
            (unsigned)dwell,
            (unsigned)settle,
            (unsigned)glk_tick_get());
        for (size_t i = 0; i < out_n && off + 80 < resp_len; i++) {
            off += (size_t)snprintf(
                resp + off,
                resp_len - off,
                "%s{\"freq_hz\":%u,\"pulses\":%d,\"rssi\":%d,\"pulse_rate_hz\":%ld,"
                "\"rssi_min\":%d,\"rssi_max\":%d}",
                i ? "," : "",
                results[i].freq_hz,
                results[i].pulses,
                results[i].rssi_est,
                (long)results[i].pulse_rate_hz,
                (int)results[i].rssi_min,
                (int)results[i].rssi_max);
        }
        snprintf(resp + off, resp_len - off, "]}");
        return GLK_OK;
    }

    if (strcmp(cmd, "mission_list") == 0) {
        char list[512];
        list[0] = 0;
        if (rpc->agent) glk_agent_list(rpc->agent, list, sizeof(list));
        snprintf(
            resp,
            resp_len,
            "{\"ok\":true,\"missions\":\"%s\",\"count\":%u}",
            list,
            rpc->agent ? (unsigned)rpc->agent->mission_count : 0u);
        return GLK_OK;
    }

    if (strcmp(cmd, "mission_arm") == 0) {
        char id[GLK_MISSION_ID_MAX];
        id[0] = 0;
        json_get_str(req_json, "id", id, sizeof(id));
        glk_err_t e = rpc->agent ? glk_agent_arm(rpc->agent, id) : GLK_ERR_GENERIC;
        snprintf(resp, resp_len, "{\"ok\":%s,\"id\":\"%s\"}", e == GLK_OK ? "true" : "false", id);
        return GLK_OK;
    }

    if (strcmp(cmd, "mission_disarm") == 0) {
        char id[GLK_MISSION_ID_MAX];
        id[0] = 0;
        json_get_str(req_json, "id", id, sizeof(id));
        glk_err_t e = rpc->agent ? glk_agent_disarm(rpc->agent, id[0] ? id : NULL) : GLK_ERR_GENERIC;
        snprintf(resp, resp_len, "{\"ok\":%s}", e == GLK_OK ? "true" : "false");
        return GLK_OK;
    }

    if (strcmp(cmd, "mission_status") == 0) {
        char id[GLK_MISSION_ID_MAX];
        id[0] = 0;
        json_get_str(req_json, "id", id, sizeof(id));
        if (!rpc->agent) {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"no_agent\"}");
            return GLK_OK;
        }
        glk_agent_status_json(rpc->agent, id[0] ? id : NULL, resp, resp_len);
        return GLK_OK;
    }

    if (strcmp(cmd, "mission_step") == 0) {
        if (!rpc->agent) {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"no_agent\"}");
            return GLK_OK;
        }
        glk_err_t e = glk_agent_run_once(rpc->agent);
        char st[256];
        glk_agent_status_json(rpc->agent, NULL, st, sizeof(st));
        /* status already a JSON object */
        snprintf(resp, resp_len, "{\"ok\":%s,\"err\":%d,\"status\":%s}", e == GLK_OK ? "true" : "false", (int)e, st);
        return GLK_OK;
    }

    if (strcmp(cmd, "mission_run") == 0) {
        if (!rpc->agent) {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"no_agent\"}");
            return GLK_OK;
        }
        uint32_t n = json_get_u32(req_json, "steps", 8);
        uint32_t ran = 0;
        glk_err_t e = glk_agent_run_steps(rpc->agent, n, &ran);
        char st[256];
        glk_agent_status_json(rpc->agent, NULL, st, sizeof(st));
        snprintf(
            resp,
            resp_len,
            "{\"ok\":%s,\"err\":%d,\"ran\":%u,\"status\":%s}",
            e == GLK_OK ? "true" : "false",
            (int)e,
            (unsigned)ran,
            st);
        return GLK_OK;
    }

    if (strcmp(cmd, "skill_list") == 0) {
        char list[512];
        glk_skill_list(list, sizeof(list));
        snprintf(
            resp,
            resp_len,
            "{\"ok\":true,\"skills\":\"%s\",\"count\":%u}",
            list,
            (unsigned)glk_skill_count());
        return GLK_OK;
    }

    if (strcmp(cmd, "agent_status") == 0) {
        if (!rpc->agent) {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"no_agent\"}");
            return GLK_OK;
        }
        glk_agent_snapshot_json(rpc->agent, resp, resp_len);
        return GLK_OK;
    }

    if (strcmp(cmd, "agent_offline") == 0) {
        if (!rpc->agent) {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"no_agent\"}");
            return GLK_OK;
        }
        /* on:true|false — also accept "enable":true */
        bool on = false;
        if (strstr(req_json, "\"on\":true") || strstr(req_json, "\"enable\":true")) on = true;
        if (strstr(req_json, "\"on\":false") || strstr(req_json, "\"enable\":false")) on = false;
#if defined(GLK_BOOT_FIELD_EXPLORE) && GLK_BOOT_FIELD_EXPLORE && \
    defined(GLK_FIELD_EXPLORE_STICKY) && GLK_FIELD_EXPLORE_STICKY
        if (!on) {
            /* Personal field unit: offline explorer stays armed (passive only). */
            glk_agent_set_offline(rpc->agent, true);
            snprintf(
                resp,
                resp_len,
                "{\"ok\":true,\"offline\":true,\"sticky\":true,"
                "\"note\":\"field_research_unit_offline_locked\"}");
            return GLK_OK;
        }
#endif
        glk_agent_set_offline(rpc->agent, on);
        snprintf(
            resp,
            resp_len,
            "{\"ok\":true,\"offline\":%s}",
            glk_agent_offline(rpc->agent) ? "true" : "false");
        return GLK_OK;
    }

    if (strcmp(cmd, "agent_auto") == 0) {
        if (!rpc->agent) {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"no_agent\"}");
            return GLK_OK;
        }
        char id[GLK_MISSION_ID_MAX];
        id[0] = 0;
        json_get_str(req_json, "id", id, sizeof(id));
        if (!id[0]) {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"missing_id\"}");
            return GLK_OK;
        }
        bool on = true;
        if (strstr(req_json, "\"on\":false") || strstr(req_json, "\"enable\":false")) on = false;
        /* Safety: only known passive missions may be autonomous */
        if (on && strcmp(id, "lab_passive_433") != 0 && strcmp(id, "lab_spectrum_planner") != 0 &&
            strcmp(id, "lab_passive_watch") != 0 && strcmp(id, "lab_noise_baseline") != 0) {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"not_allowlisted\"}");
            return GLK_OK;
        }
        glk_err_t e = glk_agent_set_autonomous(rpc->agent, id, on);
        if (on) glk_agent_set_offline(rpc->agent, true);
        snprintf(
            resp,
            resp_len,
            "{\"ok\":%s,\"id\":\"%s\",\"autonomous\":%s,\"offline\":%s}",
            e == GLK_OK ? "true" : "false",
            id,
            on ? "true" : "false",
            glk_agent_offline(rpc->agent) ? "true" : "false");
        return GLK_OK;
    }

    if (strcmp(cmd, "vault_tail") == 0) {
        uint32_t n = json_get_u32(req_json, "n", 8);
        if (n > 16) n = 16;
        char arr[900];
        glk_vault_tail_json(arr, sizeof(arr), (size_t)n);
        snprintf(resp, resp_len, "{\"ok\":true,\"count\":%u,\"events\":%s}", (unsigned)glk_vault_count(), arr);
        return GLK_OK;
    }

    if (strcmp(cmd, "vault_clear") == 0) {
        glk_vault_clear();
        snprintf(resp, resp_len, "{\"ok\":true,\"cleared\":true}");
        return GLK_OK;
    }

    if (strcmp(cmd, "catalog_reload") == 0) {
        if (!rpc->agent) {
            snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"no_agent\"}");
            return GLK_OK;
        }
        glk_err_t e = glk_catalog_load_defaults(rpc->agent);
        snprintf(
            resp,
            resp_len,
            "{\"ok\":%s,\"missions\":%u,\"skills\":%u}",
            e == GLK_OK ? "true" : "false",
            (unsigned)rpc->agent->mission_count,
            (unsigned)glk_skill_count());
        return GLK_OK;
    }

    if (strcmp(cmd, "audit_tail") == 0) {
        char tail[1024];
        size_t n = glk_audit_export(tail, sizeof(tail), 20);
        /* escape roughly: return hash only if large */
        snprintf(
            resp,
            resp_len,
            "{\"ok\":true,\"bytes\":%u,\"hash\":\"%s\"}",
            (unsigned)n,
            glk_audit_last_hash());
        return GLK_OK;
    }

    if (strcmp(cmd, "physical_confirm") == 0) {
        glk_policy_physical_confirm_set(rpc->policy, true);
        snprintf(resp, resp_len, "{\"ok\":true,\"physical\":true}");
        return GLK_OK;
    }

    snprintf(resp, resp_len, "{\"ok\":false,\"error\":\"unknown_cmd\",\"cmd\":\"%s\"}", cmd);
    return GLK_ERR_NOSUPPORT;
}
