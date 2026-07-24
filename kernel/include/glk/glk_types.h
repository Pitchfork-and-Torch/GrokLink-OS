/**
 * GrokLink OS — common types.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t glk_err_t;

#define GLK_OK 0
#define GLK_ERR_GENERIC -1
#define GLK_ERR_NOMEM -2
#define GLK_ERR_BUSY -3
#define GLK_ERR_TIMEOUT -4
#define GLK_ERR_INVAL -5
#define GLK_ERR_DENIED -6
#define GLK_ERR_NOTFOUND -7
#define GLK_ERR_CORRUPT -8
#define GLK_ERR_NOSUPPORT -9
#define GLK_ERR_FULL -10
#define GLK_ERR_EMPTY -11
#define GLK_ERR_BREAKER -12
#define GLK_ERR_DEGRADED -13

typedef uint32_t glk_tick_t;
typedef uint8_t glk_prio_t;

typedef enum {
    GLK_RISK_INFO = 0,
    GLK_RISK_PASSIVE_RX = 1,
    GLK_RISK_ACTIVE_TX = 2,
    GLK_RISK_GPIO = 3,
    GLK_RISK_CONTACT = 4,
    GLK_RISK_SYSTEM = 5,
} glk_risk_t;

typedef enum {
    GLK_ACTOR_RPC = 0,
    GLK_ACTOR_AGENT = 1,
    GLK_ACTOR_CLI = 2,
    GLK_ACTOR_UI = 3,
    GLK_ACTOR_KERNEL = 4,
} glk_actor_t;

#ifdef __cplusplus
}
#endif
