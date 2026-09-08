/**
 * BLE status skeleton (v3.9). Not a phone companion. Not GATT product.
 * Honest RPC: state is "skeleton" or "unsupported".
 */
#pragma once

#include "glk/glk_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLK_BLE_UNSUPPORTED = 0,
    GLK_BLE_SKELETON = 1,
} glk_ble_state_t;

void glk_ble_init(void);
glk_ble_state_t glk_ble_state(void);
const char* glk_ble_state_str(glk_ble_state_t s);
/** Compact JSON for RPC. */
size_t glk_ble_status_json(char* buf, size_t buflen);

#ifdef __cplusplus
}
#endif
