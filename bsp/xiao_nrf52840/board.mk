MCU_VARIANT = nrf52840
CFLAGS += -DNRF52840_XXAA

# Seeed's UF2 bootloader with SoftDevice S140 v7: the application starts at 0x27000.
LD_FILE = ${FAMILY_PATH}/linker/nrf52840_s140_v7.ld
