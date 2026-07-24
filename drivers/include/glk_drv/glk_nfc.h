#pragma once
#include "glk/glk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
glk_err_t glk_nfc_init(void);
/** Passive poll — UID bytes into out. */
glk_err_t glk_nfc_poll(glk_actor_t actor, uint8_t* uid, size_t cap, size_t* out_len);
#ifdef __cplusplus
}
#endif
