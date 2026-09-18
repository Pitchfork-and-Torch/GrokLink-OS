#include "glk_svc/glk_rpc.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    uint8_t frame[128];
    const char* payload = "hello";
    size_t n = glk_rpc_frame_encode(GLK_RPC_MSG_PING, 1, payload, 5, frame, sizeof(frame));
    assert(n == 12 + 5 + 4);

    glk_rpc_hdr_t hdr;
    const uint8_t* pl = NULL;
    uint32_t plen = 0;
    assert(glk_rpc_frame_decode(frame, n, &hdr, &pl, &plen) == GLK_OK);
    assert(hdr.msg_type == GLK_RPC_MSG_PING);
    assert(plen == 5);
    assert(memcmp(pl, "hello", 5) == 0);

    /* corrupt CRC */
    frame[n - 1] ^= 0xFF;
    assert(glk_rpc_frame_decode(frame, n, &hdr, &pl, &plen) == GLK_ERR_CORRUPT);

    printf("test_rpc_frame: OK\n");
    return 0;
}
