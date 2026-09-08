#include "glk_svc/glk_ble.h"

#include "glk/glk_config.h"

#include <stdio.h>
#include <string.h>

static glk_ble_state_t s_state = GLK_BLE_UNSUPPORTED;

void glk_ble_init(void) {
#if GLK_FEATURE_BLE_STATUS
    s_state = GLK_BLE_SKELETON;
#else
    s_state = GLK_BLE_UNSUPPORTED;
#endif
}

glk_ble_state_t glk_ble_state(void) {
    return s_state;
}

const char* glk_ble_state_str(glk_ble_state_t s) {
    switch (s) {
    case GLK_BLE_SKELETON:
        return "skeleton";
    case GLK_BLE_UNSUPPORTED:
    default:
        return "unsupported";
    }
}

size_t glk_ble_status_json(char* buf, size_t buflen) {
    if (!buf || buflen < 8) return 0;
    return (size_t)snprintf(
        buf,
        buflen,
        "{\"state\":\"%s\",\"gatt\":false,\"phone_app\":false,"
        "\"note\":\"M0+ IPCC skeleton only. Not a companion app.\"}",
        glk_ble_state_str(s_state));
}
