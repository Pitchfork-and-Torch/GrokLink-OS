#pragma once
#include "glk/glk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
glk_err_t glk_ir_init(void);
glk_err_t glk_ir_rx(glk_actor_t actor, uint32_t ms, int32_t* edges);
glk_err_t glk_ir_tx(glk_actor_t actor, const char* confirm_id, const uint8_t* data, size_t len);
#ifdef __cplusplus
}
#endif
