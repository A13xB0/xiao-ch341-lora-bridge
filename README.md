# XIAO nRF52840 + Wio-SX1262 as a CH341 USB-to-SPI LoRa bridge

Firmware that makes a Seeed XIAO nRF52840 with a Wio-SX1262 look like a CH341A USB-to-SPI adapter
(`1a86:5512`), so a Linux host drives the SX1262 directly: meshtasticd (`spidev: ch341`, through
libpinedio-usb) or RepeaterTastic's `spi` driver. The XIAO does nothing clever: it moves SPI bytes
and pin levels, and the host runs the radio.

Built on [TinyUSB](https://github.com/hathach/tinyusb) (a git submodule), no SoftDevice, no Arduino.

## Pins as the host sees them

| CH341 pin | Radio  |
| --------- | ------ |
| D0        | NSS    |
| D1        | RXEN   |
| D2        | RESET  |
| D4        | BUSY   |
| D6        | DIO1   |

`boards/lora-usb-xiao-sx1262-ch341.yaml` is the matching board file. RepeaterTastic has it built in
and its `auto` detection picks it from the USB product string; for meshtasticd copy it into
`/etc/meshtasticd/config.d/`.

At boot the firmware probes the two Wio-SX1262 pinouts (the header kit, then the board-to-board
version) by resetting the chip and reading the LoRa sync word register, and uses whichever answers.
Solid red LED: radio found. Fast blink: neither pinout answered.

## Flashing

Double-tap the XIAO's reset button; a `XIAO-SENSE` drive appears. Copy `firmware/ch341_bridge.uf2`
onto it. The board re-enumerates as `QinHeng Electronics CH341 in EPP/MEM/I2C mode`.

To get back to the bootloader without the button, send a vendor control request `0xF7` to the
device (the firmware reboots into UF2 mode), or double-tap reset.

The host needs write access to the USB device: run as root, or add a udev rule:

```
SUBSYSTEM=="usb", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="5512", MODE="0660", GROUP="plugdev"
```

## Building

Needs `arm-none-eabi-gcc`, `make` and Python 3 (for `uf2conv.py`).

```shell
git clone --recursive https://github.com/A13xB0/xiao-ch341-lora-bridge.git
cd xiao-ch341-lora-bridge
make
```

`make` copies `src/` and `bsp/` into the TinyUSB tree (its build system only builds examples and
boards that live inside it), builds, and leaves `build/ch341_bridge.uf2`. The linker script places
the application at `0x27000` for Seeed's UF2 bootloader with SoftDevice S140 v7 (the bootloader is
kept; the SoftDevice is not used).

## Protocol

The subset of the CH341 protocol that libpinedio-usb and RepeaterTastic use, over bulk endpoints
`0x02` out / `0x82` in with 32-byte packets:

| Command       | Bytes                                  | Reply                                   |
| ------------- | -------------------------------------- | --------------------------------------- |
| SPI stream    | `0xA8`, then up to 31 bit-reversed bytes | the same number of bit-reversed MISO bytes |
| UIO stream    | `0xAB`, `0x80\|levels`, `0x40\|dirs`, `0x20` | none; D0/D1/D2 outputs take `levels` |
| Read inputs   | `0xA0`                                 | 6 bytes, D0-D7 levels in the first      |

SPI is bit-banged in mode 0 at a few hundred kHz, enough for command-sized transfers. The bridge
answers one packet at a time, so the host reads each reply before sending the next packet.

## Licence

MIT. TinyUSB is MIT as well.
