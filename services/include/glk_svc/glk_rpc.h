/**
 * GrokRPC v3 — framed sessions over USB/TCP with streaming hooks.
 */
#pragma once

#include "glk/glk_types.h"
#include "glk_svc/glk_policy.h"
#include "glk_svc/glk_agent.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLK_RPC_MAGIC0 0x47 /* 'G' */
#define GLK_RPC_MAGIC1 0x4C /* 'L' */
#define GLK_RPC_VERSION 3

typedef enum {
    GLK_RPC_MSG_PING = 1,
    GLK_RPC_MSG_PONG = 2,
    GLK_RPC_MSG_STATUS = 3,
    GLK_RPC_MSG_EDU_ACK = 4,
    GLK_RPC_MSG_CONFIRM_ISSUE = 5,
    GLK_RPC_MSG_SUBGHZ_RX = 6,
    GLK_RPC_MSG_SUBGHZ_TX = 7,
    GLK_RPC_MSG_SPECTRUM = 8,
    GLK_RPC_MSG_MISSION_LIST = 9,
    GLK_RPC_MSG_MISSION_ARM = 10,
    GLK_RPC_MSG_MISSION_STATUS = 11,
    GLK_RPC_MSG_SKILL_LIST = 12,
    GLK_RPC_MSG_AUDIT_TAIL = 13,
    GLK_RPC_MSG_STREAM_EVENT = 14,
    GLK_RPC_MSG_JSON = 100, /* JSON body debug mode */
    GLK_RPC_MSG_ERROR = 255,
} glk_rpc_msg_t;

typedef struct {
    uint8_t magic[2];
    uint8_t version;
    uint8_t flags;
    uint16_t msg_type;
    uint16_t seq;
    uint32_t length;
    /* payload follows; CRC32 after payload */
} glk_rpc_hdr_t;

typedef struct {
    glk_policy_state_t* policy;
    glk_agent_t* agent;
    uint16_t seq;
    bool session_open;
    char sd_root[256];
} glk_rpc_t;

glk_err_t glk_rpc_init(glk_rpc_t* rpc, glk_policy_state_t* pol, glk_agent_t* agent, const char* sd_root);

/** Handle one JSON request line (debug-friendly, v2 bridge compatible spirit). */
glk_err_t glk_rpc_handle_json(glk_rpc_t* rpc, const char* req_json, char* resp, size_t resp_len);

/** Binary frame encode/decode helpers. */
size_t glk_rpc_frame_encode(
    uint16_t msg_type,
    uint16_t seq,
    const void* payload,
    uint32_t len,
    uint8_t* out,
    size_t out_cap);

glk_err_t glk_rpc_frame_decode(
    const uint8_t* in,
    size_t in_len,
    glk_rpc_hdr_t* hdr,
    const uint8_t** payload,
    uint32_t* payload_len);

uint32_t glk_rpc_crc32(const void* data, size_t len);

#ifdef __cplusplus
}
#endif
