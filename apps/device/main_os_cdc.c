/**
 * GrokLink OS — USB CDC + policy/RPC + ST7567 GUI + CC1101 SPI (OsRadio).
 *
 * USB path mirrors working v3.0.1-cdc:
 *   usbd_enable → usbd_connect → forever usbd_poll
 * Light RPC (ping/status/edu_ack) is handled inside the bulk RX callback so
 * TX flushes on the same eprx (proven CDC pattern). SPI/radio cmds are deferred
 * to main and only run with USB pump during dwells.
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "stm32_compat.h"
#include "usb.h"
#include "usb_cdc.h"
#include "glk_usb_rcc.h"

#include "glk/glk_kernel.h"
#include "glk/glk_config.h"
#include "glk_svc/glk_policy.h"
#include "glk_svc/glk_audit.h"
#include "glk_svc/glk_rpc.h"
#include "glk_svc/glk_agent.h"
#include "glk_svc/glk_skill.h"
#include "glk_svc/glk_catalog.h"
#include "glk_svc/glk_ml.h"
#include "glk_svc/glk_power.h"
#include "glk_svc/glk_gui.h"
#include "glk_drv/glk_radio.h"
#include "glk_board_spi_r.h"

#define CDC_EP0_SIZE 0x08
#define CDC_RXD_EP 0x01
#define CDC_TXD_EP 0x81
#define CDC_DATA_SZ 0x40
#define CDC_NTF_EP 0x82
#define CDC_NTF_SZ 0x08

struct cdc_config {
    struct usb_config_descriptor config;
    struct usb_iad_descriptor comm_iad;
    struct usb_interface_descriptor comm;
    struct usb_cdc_header_desc cdc_hdr;
    struct usb_cdc_call_mgmt_desc cdc_mgmt;
    struct usb_cdc_acm_desc cdc_acm;
    struct usb_cdc_union_desc cdc_union;
    struct usb_endpoint_descriptor comm_ep;
    struct usb_interface_descriptor data;
    struct usb_endpoint_descriptor data_eprx;
    struct usb_endpoint_descriptor data_eptx;
} __attribute__((packed));

#ifndef GLK_USB_VID
#define GLK_USB_VID 0x0483
#endif
#ifndef GLK_USB_PID_CDC
#define GLK_USB_PID_CDC 0x5740
#endif

static const struct usb_device_descriptor device_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_IAD,
    .bDeviceSubClass = USB_SUBCLASS_IAD,
    .bDeviceProtocol = USB_PROTO_IAD,
    .bMaxPacketSize0 = CDC_EP0_SIZE,
    .idVendor = GLK_USB_VID,
    .idProduct = GLK_USB_PID_CDC,
    .bcdDevice = VERSION_BCD(3, 7, 0),
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = INTSERIALNO_DESCRIPTOR,
    .bNumConfigurations = 1,
};

static const struct cdc_config config_desc = {
    .config = {
        .bLength = sizeof(struct usb_config_descriptor),
        .bDescriptorType = USB_DTYPE_CONFIGURATION,
        .wTotalLength = sizeof(struct cdc_config),
        .bNumInterfaces = 2,
        .bConfigurationValue = 1,
        .iConfiguration = NO_DESCRIPTOR,
        .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
        .bMaxPower = USB_CFG_POWER_MA(100),
    },
    .comm_iad = {
        .bLength = sizeof(struct usb_iad_descriptor),
        .bDescriptorType = USB_DTYPE_INTERFASEASSOC,
        .bFirstInterface = 0,
        .bInterfaceCount = 2,
        .bFunctionClass = USB_CLASS_CDC,
        .bFunctionSubClass = USB_CDC_SUBCLASS_ACM,
        .bFunctionProtocol = USB_PROTO_NONE,
        .iFunction = NO_DESCRIPTOR,
    },
    .comm = {
        .bLength = sizeof(struct usb_interface_descriptor),
        .bDescriptorType = USB_DTYPE_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 1,
        .bInterfaceClass = USB_CLASS_CDC,
        .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
        .bInterfaceProtocol = USB_PROTO_NONE,
        .iInterface = NO_DESCRIPTOR,
    },
    .cdc_hdr = {
        .bFunctionLength = sizeof(struct usb_cdc_header_desc),
        .bDescriptorType = USB_DTYPE_CS_INTERFACE,
        .bDescriptorSubType = USB_DTYPE_CDC_HEADER,
        .bcdCDC = VERSION_BCD(1, 1, 0),
    },
    .cdc_mgmt = {
        .bFunctionLength = sizeof(struct usb_cdc_call_mgmt_desc),
        .bDescriptorType = USB_DTYPE_CS_INTERFACE,
        .bDescriptorSubType = USB_DTYPE_CDC_CALL_MANAGEMENT,
        .bmCapabilities = 0,
        .bDataInterface = 1,
    },
    .cdc_acm = {
        .bFunctionLength = sizeof(struct usb_cdc_acm_desc),
        .bDescriptorType = USB_DTYPE_CS_INTERFACE,
        .bDescriptorSubType = USB_DTYPE_CDC_ACM,
        .bmCapabilities = 0,
    },
    .cdc_union = {
        .bFunctionLength = sizeof(struct usb_cdc_union_desc),
        .bDescriptorType = USB_DTYPE_CS_INTERFACE,
        .bDescriptorSubType = USB_DTYPE_CDC_UNION,
        .bMasterInterface0 = 0,
        .bSlaveInterface0 = 1,
    },
    .comm_ep = {
        .bLength = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType = USB_DTYPE_ENDPOINT,
        .bEndpointAddress = CDC_NTF_EP,
        .bmAttributes = USB_EPTYPE_INTERRUPT,
        .wMaxPacketSize = CDC_NTF_SZ,
        .bInterval = 0xFF,
    },
    .data = {
        .bLength = sizeof(struct usb_interface_descriptor),
        .bDescriptorType = USB_DTYPE_INTERFACE,
        .bInterfaceNumber = 1,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_CLASS_CDC_DATA,
        .bInterfaceSubClass = USB_SUBCLASS_NONE,
        .bInterfaceProtocol = USB_PROTO_NONE,
        .iInterface = NO_DESCRIPTOR,
    },
    .data_eprx = {
        .bLength = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType = USB_DTYPE_ENDPOINT,
        .bEndpointAddress = CDC_RXD_EP,
        .bmAttributes = USB_EPTYPE_BULK,
        .wMaxPacketSize = CDC_DATA_SZ,
        .bInterval = 0x01,
    },
    .data_eptx = {
        .bLength = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType = USB_DTYPE_ENDPOINT,
        .bEndpointAddress = CDC_TXD_EP,
        .bmAttributes = USB_EPTYPE_BULK,
        .wMaxPacketSize = CDC_DATA_SZ,
        .bInterval = 0x01,
    },
};

/* Match working CDC-only identity as closely as possible. */
static const struct usb_string_descriptor lang_desc = USB_ARRAY_DESC(USB_LANGID_ENG_US);
static const struct usb_string_descriptor manuf_desc_en = USB_STRING_DESC("Pitchfork-and-Torch");
static const struct usb_string_descriptor prod_desc_en = USB_STRING_DESC("GrokLink OS");
static const struct usb_string_descriptor* const dtable[] = {&lang_desc, &manuf_desc_en, &prod_desc_en};

static usbd_device udev;
static uint32_t ubuf[0x20]; /* same as working 3.0.1-cdc */
static char line_buf[384];
static uint32_t line_len;
static uint8_t tx_fifo[768];
static uint32_t tx_len;
static uint8_t banner_sent;
static uint8_t s_services_ready;

/* Deferred heavy RPC (SPI/radio) — never run inside USB callback. */
static char cmd_pending[384];
static volatile uint8_t cmd_ready;

static glk_policy_state_t s_pol;
static glk_agent_t s_agent;
static glk_rpc_t s_rpc;
static char s_resp[1024];

static struct usb_cdc_line_coding cdc_line = {
    .dwDTERate = 230400,
    .bCharFormat = USB_CDC_1_STOP_BITS,
    .bParityType = USB_CDC_NO_PARITY,
    .bDataBits = 8,
};

__attribute__((section(".glk_meta"), used))
static const struct {
    uint32_t magic;
    uint32_t version;
    char tag[16];
} glk_meta = {
    0x334B4C47u,
    0x00030700u,
    "OS3.7.0"
};

static void tx_push(const char* s) {
    size_t n = strlen(s);
    if (tx_len + n > sizeof(tx_fifo)) n = sizeof(tx_fifo) - tx_len;
    memcpy(&tx_fifo[tx_len], s, n);
    tx_len += (uint32_t)n;
}

static void reboot_system_dfu(void) {
    void (*SysMemBootJump)(void);
    uint32_t boot = 0x1FFF0000u;
    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
#if defined(RCC_APB2ENR_SYSCFGEN)
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
#endif
#if defined(SYSCFG_MEMRMP_MEM_MODE_0)
    SYSCFG->MEMRMP = (SYSCFG->MEMRMP & ~SYSCFG_MEMRMP_MEM_MODE) | SYSCFG_MEMRMP_MEM_MODE_0;
#endif
    SysMemBootJump = (void (*)(void))(*((uint32_t*)(boot + 4u)));
    __set_MSP(*((uint32_t*)boot));
    SysMemBootJump();
    for (;;) {
    }
}

/** Safe for USB RX callback: no SPI. */
static int is_light_cmd(const char* line) {
    if (strstr(line, "subghz") || strstr(line, "spectrum") || strstr(line, "mission") ||
        strstr(line, "agent") || strstr(line, "vault") || strstr(line, "confirm")) {
        return 0;
    }
    return strstr(line, "ping") != 0 || strstr(line, "status") != 0 || strstr(line, "help") != 0 ||
           strstr(line, "edu_ack") != 0 || strstr(line, "I_WILL_USE_ONLY_AUTHORIZED_TARGETS") != 0 ||
           strstr(line, "reboot_dfu") != 0;
}

static void handle_line_light(const char* line) {
    if (strstr(line, "reboot_dfu")) {
        tx_push("{\"ok\":true,\"cmd\":\"reboot_dfu\"}\n");
        return;
    }
    if (strstr(line, "ping")) {
        char buf[160];
        snprintf(
            buf,
            sizeof(buf),
            "{\"ok\":true,\"cmd\":\"pong\",\"api\":5,\"version\":\"%s\",\"native\":true}\n",
            GLK_VERSION_STRING);
        tx_push(buf);
        return;
    }
    if (!s_services_ready) {
        if (strstr(line, "status")) {
            tx_push("{\"ok\":true,\"version\":\"" GLK_VERSION_STRING
                    "\",\"api\":5,\"edu\":false,\"note\":\"booting\"}\n");
            return;
        }
        tx_push("{\"ok\":false,\"error\":\"booting\"}\n");
        return;
    }
    /* Full light RPC via service (status/edu_ack/help/…) — still no SPI. */
    glk_rpc_handle_json(&s_rpc, line, s_resp, sizeof(s_resp));
    tx_push(s_resp);
    tx_push("\n");
    if (strstr(line, "edu_ack") && strstr(s_resp, "\"edu\":true")) {
        glk_gui_set_edu(true);
    }
}

/** May SPI — main loop only. */
static void handle_line_heavy(const char* line) {
    if (!s_services_ready) {
        tx_push("{\"ok\":false,\"error\":\"booting\"}\n");
        return;
    }
    glk_rpc_handle_json(&s_rpc, line, s_resp, sizeof(s_resp));
    tx_push(s_resp);
    tx_push("\n");
    if (strstr(line, "subghz_probe") && strstr(s_resp, "\"ok\":true")) {
        glk_gui_set_radio_line("radio: probe ok");
    }
    if (strstr(line, "subghz_rx") && strstr(s_resp, "\"ok\":true")) {
        glk_gui_set_radio_line("radio: rx done");
    }
}

static void process_rx_bytes(const uint8_t* data, int n) {
    for (int i = 0; i < n; i++) {
        char c = (char)data[i];
        if (c == '\n' || c == '\r') {
            if (line_len > 0) {
                line_buf[line_len] = 0;
                if (is_light_cmd(line_buf)) {
                    /* CDC-style: answer here so cdc_bulk flushes TX on this eprx. */
                    handle_line_light(line_buf);
                } else if (!cmd_ready) {
                    size_t ncopy =
                        line_len < sizeof(cmd_pending) - 1u ? line_len : sizeof(cmd_pending) - 1u;
                    memcpy(cmd_pending, line_buf, ncopy);
                    cmd_pending[ncopy] = 0;
                    cmd_ready = 1;
                }
                line_len = 0;
            }
        } else if (line_len + 1 < sizeof(line_buf)) {
            line_buf[line_len++] = c;
        } else {
            line_len = 0;
        }
    }
}

static void usb_pump_once(void) {
    usbd_poll(&udev);
}

static void cdc_bulk(usbd_device* dev, uint8_t event, uint8_t ep) {
    (void)ep;
    if (event == usbd_evt_eprx) {
        uint8_t tmp[CDC_DATA_SZ];
        int nr = usbd_ep_read(dev, CDC_RXD_EP, tmp, CDC_DATA_SZ);
        if (nr > 0) process_rx_bytes(tmp, nr);
    }
    if (event == usbd_evt_eptx || event == usbd_evt_eprx) {
        if (tx_len > 0) {
            uint32_t chunk = tx_len > CDC_DATA_SZ ? CDC_DATA_SZ : tx_len;
            int w = usbd_ep_write(dev, CDC_TXD_EP, tx_fifo, chunk);
            if (w > 0) {
                memmove(tx_fifo, tx_fifo + w, tx_len - (uint32_t)w);
                tx_len -= (uint32_t)w;
            }
        } else if (event == usbd_evt_eptx) {
            usbd_ep_write(dev, CDC_TXD_EP, 0, 0);
        }
    }
}

static usbd_respond cdc_getdesc(usbd_ctlreq* req, void** address, uint16_t* length) {
    const uint8_t dtype = req->wValue >> 8;
    const uint8_t dnumber = req->wValue & 0xFF;
    const void* desc = 0;
    uint16_t len = 0;
    switch (dtype) {
    case USB_DTYPE_DEVICE:
        desc = &device_desc;
        break;
    case USB_DTYPE_CONFIGURATION:
        desc = &config_desc;
        len = sizeof(config_desc);
        break;
    case USB_DTYPE_STRING:
        if (dnumber < 3) desc = dtable[dnumber];
        else return usbd_fail;
        break;
    default:
        return usbd_fail;
    }
    if (len == 0) len = ((struct usb_header_descriptor*)desc)->bLength;
    *address = (void*)desc;
    *length = len;
    return usbd_ack;
}

static usbd_respond cdc_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    (void)callback;
    if (((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
            (USB_REQ_INTERFACE | USB_REQ_CLASS) &&
        req->wIndex == 0) {
        switch (req->bRequest) {
        case USB_CDC_SET_CONTROL_LINE_STATE:
            if (!banner_sent && (req->wValue & 0x01)) {
                banner_sent = 1;
                tx_push("\r\nGrokLink OS " GLK_VERSION_STRING " — authorized research only\r\n");
                tx_push("JSON: ping|status|edu_ack|subghz_probe|subghz_rx|reboot_dfu\r\n");
            }
            return usbd_ack;
        case USB_CDC_SET_LINE_CODING:
            memcpy(&cdc_line, req->data, sizeof(cdc_line));
            return usbd_ack;
        case USB_CDC_GET_LINE_CODING:
            dev->status.data_ptr = &cdc_line;
            dev->status.data_count = sizeof(cdc_line);
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}

static usbd_respond cdc_setconf(usbd_device* dev, uint8_t cfg) {
    switch (cfg) {
    case 0:
        usbd_ep_deconfig(dev, CDC_NTF_EP);
        usbd_ep_deconfig(dev, CDC_TXD_EP);
        usbd_ep_deconfig(dev, CDC_RXD_EP);
        usbd_reg_endpoint(dev, CDC_RXD_EP, 0);
        usbd_reg_endpoint(dev, CDC_TXD_EP, 0);
        return usbd_ack;
    case 1:
        usbd_ep_config(dev, CDC_RXD_EP, USB_EPTYPE_BULK, CDC_DATA_SZ);
        usbd_ep_config(dev, CDC_TXD_EP, USB_EPTYPE_BULK, CDC_DATA_SZ);
        usbd_ep_config(dev, CDC_NTF_EP, USB_EPTYPE_INTERRUPT, CDC_NTF_SZ);
        usbd_reg_endpoint(dev, CDC_RXD_EP, cdc_bulk);
        usbd_reg_endpoint(dev, CDC_TXD_EP, cdc_bulk);
        usbd_ep_write(dev, CDC_TXD_EP, 0, 0);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static void arm_field_explore(const char* reason) {
    static const char* const k_passive[] = {
        "lab_passive_watch",
        "lab_noise_baseline",
        "lab_spectrum_planner",
        "lab_passive_433",
    };
    glk_policy_set_edu_acked(&s_pol, true);
    glk_gui_set_edu(true);
    for (unsigned i = 0; i < sizeof(k_passive) / sizeof(k_passive[0]); i++) {
        (void)glk_agent_set_autonomous(&s_agent, k_passive[i], true);
    }
    (void)glk_agent_arm(&s_agent, "lab_passive_watch");
    glk_agent_set_offline(&s_agent, true);
    glk_gui_set_offline(true, "RESEARCH");
    glk_gui_set_radio_line("FIELD always-on");
    glk_audit_log(
        GLK_ACTOR_AGENT,
        "field_explore_arm",
        GLK_RISK_PASSIVE_RX,
        GLK_POLICY_ALLOW,
        reason ? reason : "field",
        "all_passive_rom");
}

int main(void) {
    (void)glk_meta;

    /*
     * USB FIRST — identical order to working 3.0.1-cdc.
     * CDC-only control proved bulk I/O works; full OS failed when services
     * ran before connect. Bring USB up immediately, poll forever, then
     * bring services online with continuous poll between steps.
     */
    glk_usb_pins_restore();
    usbd_init(&udev, &usbd_hw, CDC_EP0_SIZE, ubuf, sizeof(ubuf));
    usbd_reg_config(&udev, cdc_setconf);
    usbd_reg_control(&udev, cdc_control);
    usbd_reg_descr(&udev, cdc_getdesc);
    usbd_enable(&udev, true);
    usbd_connect(&udev, true);

    /* Pure USB settle (CDC-only does this forever). Ping works here via light path. */
    for (uint32_t i = 0; i < 100000u; i++) {
        usbd_poll(&udev);
    }

    /* Services with poll between each step (never block EP0). */
    glk_kernel_init();
    for (int i = 0; i < 200; i++) usbd_poll(&udev);
    glk_power_init();
    for (int i = 0; i < 200; i++) usbd_poll(&udev);
    glk_ml_init();
    for (int i = 0; i < 200; i++) usbd_poll(&udev);
    glk_skill_init();
    for (int i = 0; i < 200; i++) usbd_poll(&udev);
    glk_policy_init(&s_pol);
    glk_policy_set_global(&s_pol);
    glk_policy_set_sd_present(&s_pol, false);
    s_pol.blacklist_ok = true;
    for (int i = 0; i < 200; i++) usbd_poll(&udev);
    glk_audit_init(NULL);
    for (int i = 0; i < 200; i++) usbd_poll(&udev);
    glk_agent_init(&s_agent, &s_pol);
    glk_agent_set_global(&s_agent);
    for (int i = 0; i < 400; i++) usbd_poll(&udev);
    (void)glk_catalog_load_defaults(&s_agent);
    for (int i = 0; i < 400; i++) usbd_poll(&udev);
    glk_radio_init(&s_pol);
    glk_radio_start_worker();
    for (int i = 0; i < 200; i++) usbd_poll(&udev);
    glk_rpc_init(&s_rpc, &s_pol, &s_agent, "");
    for (int i = 0; i < 200; i++) usbd_poll(&udev);
    glk_gui_init();
    glk_board_set_usb_pump(usb_pump_once);
    glk_usb_pins_restore();
    for (int i = 0; i < 500; i++) usbd_poll(&udev);
    s_services_ready = 1;

    /*
     * Main loop matches working bisect-init: usbd_poll is primary.
     * Bisect proved: services init is fine; early field SPI kills host CDC.
     * While a USB host has opened the port (banner_sent), never run SPI/field.
     * Unplugged / no DTR for a long time → arm passive field research.
     */
    {
        uint32_t loops = 0;
        uint32_t last_agent = 0;
        uint32_t last_sticky = 0;
        uint32_t host_gone_loops = 0;
        bool field_done = false;

        for (;;) {
            usbd_poll(&udev);
            glk_tick_isr();
            loops++;

            /* Host present if DTR banner path ran (SET_CONTROL_LINE_STATE). */
            const int host_present = banner_sent != 0;

            if (cmd_ready) {
                char local[384];
                memcpy(local, cmd_pending, sizeof(local));
                cmd_ready = 0;

                if (strstr(local, "reboot_dfu")) {
                    tx_push("{\"ok\":true,\"cmd\":\"reboot_dfu\"}\n");
                    for (int i = 0; i < 200; i++) usbd_poll(&udev);
                    for (volatile int i = 0; i < 200000; i++) {
                    }
                    reboot_system_dfu();
                } else {
                    handle_line_heavy(local);
                    /* Drain TX between polls only (same as bisect; no nested thrash). */
                    for (int k = 0; k < 8 && tx_len > 0; k++) {
                        uint32_t chunk = tx_len > CDC_DATA_SZ ? CDC_DATA_SZ : tx_len;
                        int w = usbd_ep_write(&udev, CDC_TXD_EP, tx_fifo, chunk);
                        if (w > 0) {
                            memmove(tx_fifo, tx_fifo + (uint32_t)w, tx_len - (uint32_t)w);
                            tx_len -= (uint32_t)w;
                        }
                        usbd_poll(&udev);
                    }
                }
            }

            /* GUI keys only — display still self-defers SPI inside glk_gui. */
            if ((loops % 64u) == 0u) {
                glk_gui_poll();
            }

            if (glk_gui_consume_field_explore()) {
                arm_field_explore("gui_ok_hold_safety");
                field_done = true;
            }

#if GLK_BOOT_FIELD_EXPLORE
            /*
             * Auto field ONLY if a host never claimed DTR (banner_sent).
             * Once Windows opens COM, stay USB-primary forever this boot
             * (field only via GUI hold-OK). Unplugged-from-boot units still
             * get passive research after a long pure-poll wait (~tens of sec).
             */
            if (!field_done && !banner_sent) {
                host_gone_loops++;
                if (host_gone_loops >= 30000000u) {
                    arm_field_explore("boot_field_unplugged");
                    field_done = true;
                }
            }
#endif

#if GLK_BOOT_FIELD_EXPLORE && GLK_FIELD_EXPLORE_STICKY
            /* Sticky re-arm only when unplugged (no host DTR). */
            if (field_done && !host_present) {
                uint32_t now = glk_tick_get();
                if ((now - last_sticky) >= 5000u) {
                    last_sticky = now;
                    if (!glk_agent_offline(&s_agent) || !s_agent.active_id[0]) {
                        arm_field_explore("sticky_rearm");
                    } else {
                        glk_agent_set_offline(&s_agent, true);
                        glk_gui_set_edu(true);
                        glk_gui_set_offline(
                            true, s_agent.active_id[0] ? s_agent.active_id : "RESEARCH");
                    }
                }
            }
#endif

            /* SPI agent ticks only when field armed AND host not using CDC. */
            if (field_done && !host_present && glk_agent_offline(&s_agent)) {
                uint32_t now = glk_tick_get();
                if ((now - last_agent) >= 600u) {
                    last_agent = now;
                    (void)glk_agent_poll_usb_safe(&s_agent);
                    if (s_agent.active_id[0]) {
                        char line[28];
                        snprintf(line, sizeof(line), "ag:%.20s", s_agent.active_id);
                        glk_gui_set_radio_line(line);
                        glk_gui_set_offline(true, s_agent.active_id);
                    }
                }
            }
        }
    }
    return 0;
}
