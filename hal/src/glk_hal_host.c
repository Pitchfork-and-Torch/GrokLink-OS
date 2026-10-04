#include "glk_hal/glk_hal.h"
#include "glk/glk_kernel.h"

void glk_hal_init(void) {}
uint32_t glk_hal_millis(void) { return glk_tick_get(); }
void glk_hal_delay_ms(uint32_t ms) { glk_task_sleep_ms(ms); }
void glk_hal_watchdog_kick(void) {}
int glk_hal_usb_read(void* buf, size_t n) { (void)buf; (void)n; return 0; }
int glk_hal_usb_write(const void* buf, size_t n) { (void)buf; return (int)n; }
bool glk_hal_sd_present(void) { return true; }
