/**
 * P1 target: USB CDC + GrokRPC JSON (skeleton).
 *
 * Not linked into the bring-up DFU until libusb_stm32 + 48 MHz USB clock
 * are integrated (see docs/ROADMAP_NATIVE.md Phase P1).
 *
 * Intended loop:
 *   usbd_poll → accumulate lines → glk_rpc_handle_json → write response
 */
#include "glk/glk_config.h"
#include "glk_svc/glk_rpc.h"
#include "glk_svc/glk_policy.h"
#include "glk_svc/glk_agent.h"
#include "glk_svc/glk_audit.h"
#include "glk_svc/glk_ml.h"
#include "glk_svc/glk_skill.h"
#include "glk_svc/glk_power.h"
#include "glk_drv/glk_radio.h"
#include "glk/glk_kernel.h"

#include <string.h>

/* When USB is wired, replace these stubs with real CDC TX/RX. */
__attribute__((weak)) void glk_usb_cdc_init(void) {}
__attribute__((weak)) void glk_usb_cdc_poll(void) {}
__attribute__((weak)) int glk_usb_cdc_read(char* buf, int max) {
    (void)buf;
    (void)max;
    return 0;
}
__attribute__((weak)) int glk_usb_cdc_write(const char* buf, int len) {
    (void)buf;
    return len;
}

static glk_policy_state_t s_pol;
static glk_agent_t s_agent;
static glk_rpc_t s_rpc;
static char s_line[512];
static int s_llen;
static char s_resp[1024];

static void process_line(void) {
    if (s_llen <= 0) return;
    s_line[s_llen] = 0;
    glk_rpc_handle_json(&s_rpc, s_line, s_resp, sizeof(s_resp));
    size_t n = strlen(s_resp);
    glk_usb_cdc_write(s_resp, (int)n);
    glk_usb_cdc_write("\n", 1);
    s_llen = 0;
}

/* Device entry once CDC profile is built (not default bring-up). */
int glk_device_cdc_main(void) {
    glk_kernel_init();
    glk_power_init();
    glk_ml_init();
    glk_skill_init();
    glk_policy_init(&s_pol);
    glk_policy_set_global(&s_pol);
    /* No SD yet → degraded but status/ping/edu work once edu set */
    glk_policy_set_sd_present(&s_pol, false);
    glk_audit_init(NULL);
    glk_agent_init(&s_agent, &s_pol);
    glk_radio_init(&s_pol);
    glk_rpc_init(&s_rpc, &s_pol, &s_agent, "");

    glk_usb_cdc_init();
    const char* banner =
        "\r\nGrokLink OS " GLK_VERSION_STRING " CDC — authorized research only\r\n";
    glk_usb_cdc_write(banner, (int)strlen(banner));

    for (;;) {
        glk_usb_cdc_poll();
        char ch;
        while (glk_usb_cdc_read(&ch, 1) == 1) {
            if (ch == '\n' || ch == '\r') {
                process_line();
            } else if (s_llen + 1 < (int)sizeof(s_line)) {
                s_line[s_llen++] = ch;
            } else {
                s_llen = 0; /* overflow discard */
            }
        }
    }
    return 0;
}
