// USB identity of a CH341A in SPI/I2C mode (1a86:5512): one vendor interface with bulk OUT 0x02
// and IN 0x82, 32-byte packets, as libpinedio-usb and RepeaterTastic's CH341 driver expect.

#include "bsp/board_api.h"
#include "tusb.h"

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0110,
    .bDeviceClass       = TUSB_CLASS_VENDOR_SPECIFIC,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x1A86,
    .idProduct          = 0x5512,
    .bcdDevice          = 0x0304,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_VENDOR_DESCRIPTOR(0, 0, 0x02, 0x82, CFG_TUD_VENDOR_EPSIZE),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return desc_configuration;
}

static char const *string_desc_arr[] = {
    (const char[]) {0x09, 0x04},
    "RepeaterTastic",
    "XIAO-SX1262-CH341",
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;
  if (index == 0) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else if (index == 3) {
    // 8 hex digits: libpinedio-usb compares USB_Serialnum over 8 characters.
    uint8_t uid[16];
    board_get_unique_id(uid, sizeof(uid));
    static char const hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < 8; i++) {
      _desc_str[1 + i] = hex[(uid[i / 2] >> (i % 2 ? 0 : 4)) & 0x0F];
    }
    chr_count = 8;
  } else {
    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
      return NULL;
    }
    char const *str = string_desc_arr[index];
    chr_count = strlen(str);
    if (chr_count > 32) {
      chr_count = 32;
    }
    for (size_t i = 0; i < chr_count; i++) {
      _desc_str[1 + i] = str[i];
    }
  }
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}
