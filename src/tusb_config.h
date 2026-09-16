#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#define CFG_TUSB_OS           OPT_OS_NONE
#define CFG_TUSB_DEBUG        0

#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_VENDOR        1
// CH341 bulk endpoints are 32 bytes. Unbuffered: each OUT packet reaches tud_vendor_rx_cb whole,
// and OUT is re-armed by hand only after the reply went out (backpressure).
#define CFG_TUD_VENDOR_EPSIZE        32
#define CFG_TUD_VENDOR_RX_BUFSIZE    0
#define CFG_TUD_VENDOR_TX_BUFSIZE    0
#define CFG_TUD_VENDOR_RX_MANUAL_XFER 1

#endif
