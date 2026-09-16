include ../../../hw/bsp/family_support.mk

INC += \
  src \

EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(EXAMPLE_PATH)/, $(EXAMPLE_SOURCE))

include ../../../hw/bsp/family_rules.mk
