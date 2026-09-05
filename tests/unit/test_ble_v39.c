#include "glk_svc/glk_ble.h"
#include "glk_svc/glk_rpc.h"
#include "glk_svc/glk_policy.h"
#include "glk/glk_kernel.h"
#include "glk/glk_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    glk_kernel_init();
    glk_ble_init();
    assert(glk_ble_state() == GLK_BLE_SKELETON);
    assert(strcmp(glk_ble_state_str(glk_ble_state()), "skeleton") == 0);

    char js[240];
    glk_ble_status_json(js, sizeof(js));
    assert(strstr(js, "skeleton") != NULL);
    assert(strstr(js, "\"phone_app\":false") != NULL);

    glk_policy_state_t pol;
    glk_policy_init(&pol);
    glk_rpc_t rpc;
    glk_rpc_init(&rpc, &pol, NULL, "");
    char resp[400];
    assert(glk_rpc_handle_json(&rpc, "{\"cmd\":\"ble_status\"}", resp, sizeof(resp)) == GLK_OK);
    assert(strstr(resp, "\"ok\":true") != NULL);
    assert(strstr(resp, "skeleton") != NULL);

    char ping[160];
    assert(glk_rpc_handle_json(&rpc, "{\"cmd\":\"ping\"}", ping, sizeof(ping)) == GLK_OK);
    assert(strstr(ping, "\"api\":7") != NULL);
    assert(strstr(ping, "3.9.0") != NULL);

    printf("test_ble_v39 ok\n");
    return 0;
}
