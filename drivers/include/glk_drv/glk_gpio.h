#pragma once
#include "glk/glk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
glk_err_t glk_gpio_init(void);
glk_err_t glk_gpio_read(int32_t pin, int* level);
glk_err_t glk_gpio_write(glk_actor_t actor, int32_t pin, int level, const char* confirm_id);
#ifdef __cplusplus
}
#endif
