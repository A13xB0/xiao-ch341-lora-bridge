// CH341 USB-to-SPI bridge for a Seeed XIAO nRF52840 with a Wio-SX1262.
//
// The XIAO pretends to be a CH341A in SPI mode so a Linux host drives its SX1262 directly:
// meshtasticd (spidev: ch341, through libpinedio-usb) or RepeaterTastic's spi driver. The host
// runs RadioLib or its own driver; this firmware only moves SPI bytes and pin levels.
//
// Protocol (libpinedio-usb, flashrom ch341a_spi):
//   0xA8 + n bytes  SPI stream: each byte bit-reversed; reply n bit-reversed MISO bytes
//   0xAB ... 0x20   UIO stream: 0x80|levels sets D0-D5, 0x40|dirs sets directions
//   0xA0            read inputs: reply 6 bytes, D0-D7 levels in the first
// Pins as the host sees them: D0 = NSS, D1 = RXEN, D2 = RESET, D4 = BUSY, D6 = DIO1.
//
// A vendor control request 0xF7 reboots into the UF2 bootloader, for reflashing.

#include "bsp/board_api.h"
#include "nrf_gpio.h"
#include "tusb.h"

#define P(port, pin) ((port) * 32 + (pin))

#define PIN_SCK  P(1, 13)
#define PIN_MISO P(1, 14)
#define PIN_MOSI P(1, 15)

typedef struct {
  char const *name;
  uint32_t nss, dio1, busy, reset, rxen;
} pinmap_t;

// Wio-SX1262 for XIAO: the header kit (SKU 102010710/113010003), then the board-to-board version.
static pinmap_t const maps[] = {
    {"kit", P(0, 4), P(0, 3), P(0, 29), P(0, 28), P(0, 5)},
    {"btb", P(0, 29), P(0, 2), P(0, 3), P(0, 28), P(0, 4)},
};
static pinmap_t const *pins = &maps[0];

static uint8_t out_levels = 0x05; // D0 (NSS) and D2 (RESET) idle high
static uint8_t pending[CFG_TUD_VENDOR_EPSIZE];
static uint32_t pending_len;
static volatile bool have_pending;
static volatile bool reboot_to_bootloader;

//--------------------------------------------------------------------+
// SPI and pins
//--------------------------------------------------------------------+

// Mode 0, MSB first, bit-banged: a few hundred kHz, plenty for command-sized transfers.
static uint8_t spi_byte(uint8_t out) {
  uint8_t in = 0;
  for (int i = 7; i >= 0; i--) {
    nrf_gpio_pin_write(PIN_MOSI, (out >> i) & 1);
    nrf_gpio_pin_set(PIN_SCK);
    in = (uint8_t) ((in << 1) | nrf_gpio_pin_read(PIN_MISO));
    nrf_gpio_pin_clear(PIN_SCK);
  }
  return in;
}

static uint8_t reverse_bits(uint8_t b) {
  b = (uint8_t) (((b >> 1) & 0x55) | ((b << 1) & 0xAA));
  b = (uint8_t) (((b >> 2) & 0x33) | ((b << 2) & 0xCC));
  return (uint8_t) (((b >> 4) & 0x0F) | ((b << 4) & 0xF0));
}

static void apply_outputs(void) {
  nrf_gpio_pin_write(pins->nss, out_levels & 0x01);
  nrf_gpio_pin_write(pins->rxen, (out_levels >> 1) & 0x01);
  nrf_gpio_pin_write(pins->reset, (out_levels >> 2) & 0x01);
}

static void configure_pins(pinmap_t const *m) {
  nrf_gpio_cfg_output(m->nss);
  nrf_gpio_pin_set(m->nss);
  nrf_gpio_cfg_output(m->reset);
  nrf_gpio_pin_set(m->reset);
  nrf_gpio_cfg_output(m->rxen);
  nrf_gpio_pin_clear(m->rxen);
  nrf_gpio_cfg_input(m->busy, NRF_GPIO_PIN_NOPULL);
  nrf_gpio_cfg_input(m->dio1, NRF_GPIO_PIN_PULLDOWN);
}

static void release_pins(pinmap_t const *m) {
  nrf_gpio_cfg_default(m->nss);
  nrf_gpio_cfg_default(m->rxen);
  nrf_gpio_cfg_default(m->busy);
  nrf_gpio_cfg_default(m->dio1);
}

// probe resets the SX1262 through a pin map and reads the LoRa sync word register, which is
// 0x1424 after reset.
static bool probe(pinmap_t const *m) {
  configure_pins(m);
  nrf_gpio_pin_clear(m->reset);
  board_delay(2);
  nrf_gpio_pin_set(m->reset);
  board_delay(10);
  uint32_t start = tusb_time_millis_api();
  while (nrf_gpio_pin_read(m->busy)) {
    if (tusb_time_millis_api() - start > 300) {
      release_pins(m);
      return false;
    }
  }
  uint8_t const tx[] = {0x1D, 0x07, 0x40, 0x00, 0x00, 0x00};
  uint8_t rx[sizeof(tx)];
  nrf_gpio_pin_clear(m->nss);
  for (size_t i = 0; i < sizeof(tx); i++) {
    rx[i] = spi_byte(tx[i]);
  }
  nrf_gpio_pin_set(m->nss);
  if (rx[4] == 0x14 && rx[5] == 0x24) {
    return true;
  }
  release_pins(m);
  return false;
}

//--------------------------------------------------------------------+
// CH341 commands
//--------------------------------------------------------------------+

static void write_reply(uint8_t const *buf, uint32_t len) {
  while (tud_vendor_mounted()) {
    if (tud_vendor_write(buf, len) == len) {
      return;
    }
    tud_task();
  }
}

static void handle_packet(uint8_t const *buf, uint32_t len) {
  uint32_t i = 0;
  while (i < len) {
    switch (buf[i]) {
      case 0xA8: { // SPI stream: the rest of the packet
        uint8_t reply[CFG_TUD_VENDOR_EPSIZE];
        uint32_t n = 0;
        for (i++; i < len; i++) {
          reply[n++] = reverse_bits(spi_byte(reverse_bits(buf[i])));
        }
        write_reply(reply, n);
        return;
      }
      case 0xAB: // UIO stream, up to 0x20
        for (i++; i < len && buf[i] != 0x20; i++) {
          if (buf[i] & 0x80) {
            out_levels = buf[i] & 0x3F;
            apply_outputs();
          }
          // 0x40|directions: pin roles are fixed here, nothing to do
        }
        i++;
        break;
      case 0xA0: { // read inputs
        uint8_t levels = out_levels & 0x07;
        levels |= (uint8_t) (nrf_gpio_pin_read(pins->busy) << 4);
        levels |= (uint8_t) (nrf_gpio_pin_read(pins->dio1) << 6);
        uint8_t reply[6] = {levels, 0, 0, 0, 0, 0};
        write_reply(reply, sizeof(reply));
        i++;
        break;
      }
      default:
        i++;
        break;
    }
  }
}

//--------------------------------------------------------------------+
// USB callbacks
//--------------------------------------------------------------------+

void tud_mount_cb(void) {
  tud_vendor_read_xfer();
}

void tud_vendor_rx_cb(uint8_t idx, uint8_t const *buffer, uint32_t bufsize) {
  (void) idx;
  if (buffer == NULL || bufsize == 0) {
    tud_vendor_read_xfer();
    return;
  }
  memcpy(pending, buffer, bufsize > sizeof(pending) ? sizeof(pending) : bufsize);
  pending_len = bufsize;
  have_pending = true;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_VENDOR) {
    return false;
  }
  if (request->bRequest == 0xF7) {
    if (stage == CONTROL_STAGE_SETUP) {
      return tud_control_status(rhport, request);
    }
    if (stage == CONTROL_STAGE_ACK) {
      reboot_to_bootloader = true;
    }
    return true;
  }
  return false;
}

//--------------------------------------------------------------------+

int main(void) {
  board_init();

  nrf_gpio_cfg_output(PIN_SCK);
  nrf_gpio_pin_clear(PIN_SCK);
  nrf_gpio_cfg_output(PIN_MOSI);
  nrf_gpio_cfg_input(PIN_MISO, NRF_GPIO_PIN_NOPULL);

  // Find the radio: kit pinout, then board-to-board. Blink fast if neither answers.
  bool found = false;
  for (int attempt = 0; attempt < 3 && !found; attempt++) {
    for (size_t m = 0; m < sizeof(maps) / sizeof(maps[0]); m++) {
      if (probe(&maps[m])) {
        pins = &maps[m];
        found = true;
        break;
      }
    }
  }
  if (!found) {
    configure_pins(pins);
  }
  apply_outputs();
  board_led_write(found);

  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  board_init_after_tusb();

  uint32_t blink = 0;
  while (1) {
    tud_task();
    if (have_pending) {
      handle_packet(pending, pending_len);
      have_pending = false;
      tud_vendor_read_xfer();
    }
    if (reboot_to_bootloader) {
      board_delay(50);
      NRF_POWER->GPREGRET = 0x57; // Adafruit bootloader: stay in UF2 mode
      NVIC_SystemReset();
    }
    if (!found && tusb_time_millis_api() - blink > 150) {
      blink = tusb_time_millis_api();
      static bool on;
      on = !on;
      board_led_write(on);
    }
  }
}
