/**
 * GrokLink OS 3.0.1 — USB CDC shell (Phase P1).
 * Enumerates as STM32 VCP 0483:5740, JSON-line RPC subset.
 *
 * Flash only with recovery DFU ready (GrokLink-v2.1.3.dfu).
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "usb.h"
#include "usb_cdc.h"

#define CDC_EP0_SIZE 0x08
#define CDC_RXD_EP 0x01
#define CDC_TXD_EP 0x81
#define CDC_DATA_SZ 0x40
#define CDC_NTF_EP 0x82
#define CDC_NTF_SZ 0x08

#define GLK_CDC_VERSION "3.0.1-cdc"

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

static const struct usb_device_descriptor device_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_IAD,
    .bDeviceSubClass = USB_SUBCLASS_IAD,
    .bDeviceProtocol = USB_PROTO_IAD,
    .bMaxPacketSize0 = CDC_EP0_SIZE,
    .idVendor = 0x0483,
    .idProduct = 0x5740,
    .bcdDevice = VERSION_BCD(3, 0, 1),
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = INTSERIALNO_DESCRIPTOR,
    .bNumConfigurations = 1,
};

static const struct cdc_config config_desc = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct cdc_config),
            .bNumInterfaces = 2,
            .bConfigurationValue = 1,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
            .bMaxPower = USB_CFG_POWER_MA(100),
        },
    .comm_iad =
        {
            .bLength = sizeof(struct usb_iad_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFASEASSOC,
            .bFirstInterface = 0,
            .bInterfaceCount = 2,
            .bFunctionClass = USB_CLASS_CDC,
            .bFunctionSubClass = USB_CDC_SUBCLASS_ACM,
            .bFunctionProtocol = USB_PROTO_NONE,
            .iFunction = NO_DESCRIPTOR,
        },
    .comm =
        {
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
    .cdc_hdr =
        {
            .bFunctionLength = sizeof(struct usb_cdc_header_desc),
            .bDescriptorType = USB_DTYPE_CS_INTERFACE,
            .bDescriptorSubType = USB_DTYPE_CDC_HEADER,
            .bcdCDC = VERSION_BCD(1, 1, 0),
        },
    .cdc_mgmt =
        {
            .bFunctionLength = sizeof(struct usb_cdc_call_mgmt_desc),
            .bDescriptorType = USB_DTYPE_CS_INTERFACE,
            .bDescriptorSubType = USB_DTYPE_CDC_CALL_MANAGEMENT,
            .bmCapabilities = 0,
            .bDataInterface = 1,
        },
    .cdc_acm =
        {
            .bFunctionLength = sizeof(struct usb_cdc_acm_desc),
            .bDescriptorType = USB_DTYPE_CS_INTERFACE,
            .bDescriptorSubType = USB_DTYPE_CDC_ACM,
            .bmCapabilities = 0,
        },
    .cdc_union =
        {
            .bFunctionLength = sizeof(struct usb_cdc_union_desc),
            .bDescriptorType = USB_DTYPE_CS_INTERFACE,
            .bDescriptorSubType = USB_DTYPE_CDC_UNION,
            .bMasterInterface0 = 0,
            .bSlaveInterface0 = 1,
        },
    .comm_ep =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = CDC_NTF_EP,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = CDC_NTF_SZ,
            .bInterval = 0xFF,
        },
    .data =
        {
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
    .data_eprx =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = CDC_RXD_EP,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = CDC_DATA_SZ,
            .bInterval = 0x01,
        },
    .data_eptx =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = CDC_TXD_EP,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = CDC_DATA_SZ,
            .bInterval = 0x01,
        },
};

static const struct usb_string_descriptor lang_desc = USB_ARRAY_DESC(USB_LANGID_ENG_US);
static const struct usb_string_descriptor manuf_desc_en = USB_STRING_DESC("Pitchfork-and-Torch");
static const struct usb_string_descriptor prod_desc_en = USB_STRING_DESC("GrokLink OS CDC");
static const struct usb_string_descriptor* const dtable[] = {
    &lang_desc,
    &manuf_desc_en,
    &prod_desc_en,
};

static usbd_device udev;
static uint32_t ubuf[0x20];
static uint8_t rx_accum[512];
static uint32_t rx_len;
static char line_buf[256];
static uint32_t line_len;
static uint8_t tx_fifo[512];
static uint32_t tx_len;
static uint8_t edu_ok;
static uint8_t banner_sent;

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
} glk_meta = {0x334B4C47u, 0x00030001u, "OS3-CDC"};

static void tx_push(const char* s) {
    size_t n = strlen(s);
    if (tx_len + n > sizeof(tx_fifo)) n = sizeof(tx_fifo) - tx_len;
    memcpy(&tx_fifo[tx_len], s, n);
    tx_len += (uint32_t)n;
}

/* Jump to STM32 system bootloader (DFU). */
static void reboot_system_dfu(void) {
    void (*SysMemBootJump)(void);
    uint32_t boot = 0x1FFF0000u; /* STM32WB55 system memory */
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

static void handle_line(const char* line) {
    char resp[384];
    if (strstr(line, "reboot_dfu") || strstr(line, "dfu")) {
        tx_push("{\"ok\":true,\"cmd\":\"reboot_dfu\"}\n");
        /* allow TX drain */
        for (volatile int i = 0; i < 200000; i++) {
        }
        reboot_system_dfu();
        return;
    }
    if (strstr(line, "ping")) {
        snprintf(
            resp,
            sizeof(resp),
            "{\"ok\":true,\"cmd\":\"pong\",\"api\":3,\"version\":\"%s\",\"native\":true}\n",
            GLK_CDC_VERSION);
        tx_push(resp);
        return;
    }
    if (strstr(line, "edu_ack") || strstr(line, "I_WILL_USE_ONLY_AUTHORIZED_TARGETS")) {
        edu_ok = 1;
        tx_push("{\"ok\":true,\"edu\":true}\n");
        return;
    }
    if (strstr(line, "status")) {
        snprintf(
            resp,
            sizeof(resp),
            "{\"ok\":true,\"version\":\"%s\",\"api\":3,\"edu\":%s,\"sd\":false,"
            "\"agent\":\"cdc-shell\",\"note\":\"P1 USB CDC - full agent not linked yet\","
            "\"safety\":\"strict\"}\n",
            GLK_CDC_VERSION,
            edu_ok ? "true" : "false");
        tx_push(resp);
        return;
    }
    if (strstr(line, "help")) {
        tx_push("{\"ok\":true,\"cmds\":[\"ping\",\"status\",\"edu_ack\"]}\n");
        return;
    }
    tx_push("{\"ok\":false,\"error\":\"unknown_cmd\"}\n");
}

static void process_rx_bytes(const uint8_t* data, int n) {
    for (int i = 0; i < n; i++) {
        char c = (char)data[i];
        if (c == '\n' || c == '\r') {
            if (line_len > 0) {
                line_buf[line_len] = 0;
                handle_line(line_buf);
                line_len = 0;
            }
        } else if (line_len + 1 < sizeof(line_buf)) {
            line_buf[line_len++] = c;
        } else {
            line_len = 0;
        }
    }
}

static void cdc_bulk(usbd_device* dev, uint8_t event, uint8_t ep) {
    (void)ep;
    if (event == usbd_evt_eprx) {
        uint8_t tmp[CDC_DATA_SZ];
        int n = usbd_ep_read(dev, CDC_RXD_EP, tmp, CDC_DATA_SZ);
        if (n > 0) process_rx_bytes(tmp, n);
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
            /* keep IN endpoint primed */
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
                tx_push("\r\nGrokLink OS " GLK_CDC_VERSION " — authorized research only\r\n");
                tx_push("JSON: {\"cmd\":\"ping\"}|status|edu_ack\r\n");
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

int main(void) {
    (void)glk_meta;
    (void)rx_accum;
    (void)rx_len;
    usbd_init(&udev, &usbd_hw, CDC_EP0_SIZE, ubuf, sizeof(ubuf));
    usbd_reg_config(&udev, cdc_setconf);
    usbd_reg_control(&udev, cdc_control);
    usbd_reg_descr(&udev, cdc_getdesc);
    usbd_enable(&udev, true);
    usbd_connect(&udev, true);
    for (;;) {
        usbd_poll(&udev);
    }
    return 0;
}
