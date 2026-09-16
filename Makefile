# Builds the bridge with TinyUSB's own build system, which only knows examples and boards that live
# inside its tree: copy ours in, build there, copy the UF2 out.
TINYUSB := tinyusb
EXAMPLE := $(TINYUSB)/examples/device/ch341_bridge
BOARD := xiao_nrf52840
BSP := $(TINYUSB)/hw/bsp/nrf

.PHONY: all clean

all: build/ch341_bridge.uf2

build/ch341_bridge.uf2: src/main.c src/tusb_config.h src/usb_descriptors.c bsp/$(BOARD)/board.h bsp/$(BOARD)/board.mk bsp/nrf52840_s140_v7.ld
	@test -f $(TINYUSB)/README.rst || { echo "tinyusb submodule missing: git submodule update --init"; exit 1; }
	@cd $(TINYUSB) && python3 tools/get_deps.py nrf >/dev/null
	mkdir -p $(EXAMPLE)/src $(BSP)/boards/$(BOARD) build
	cp src/*.c src/*.h $(EXAMPLE)/src/
	cp example.mk $(EXAMPLE)/Makefile
	cp bsp/$(BOARD)/board.h bsp/$(BOARD)/board.mk $(BSP)/boards/$(BOARD)/
	cp bsp/nrf52840_s140_v7.ld $(BSP)/linker/
	$(MAKE) -C $(EXAMPLE) BOARD=$(BOARD) all uf2
	cp $(EXAMPLE)/_build/$(BOARD)/ch341_bridge.uf2 build/

clean:
	rm -rf build $(EXAMPLE)/_build
