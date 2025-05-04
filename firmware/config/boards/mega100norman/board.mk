
# List of all the board related files.
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp


# defines SHORT_BOARD_NAME
include $(BOARD_DIR)/meta-info.env

# one day when we are grown ups and can coordinate a real life test we shall revisit https://github.com/rusefi/rusefi/issues/6008
DDEFS += -DDISABLE_PIN_STATE_VALIDATION=TRUE

# reduce memory usage monitoring
DDEFS += -DRAM_UNUSED_SIZE=100

# assign critical LED to a non-existent pin
DDEFS += -DLED_CRITICAL_ERROR_BRAIN_PIN=Gpio::I15

# MAJOR HACK? SOMETHING IS FUNNY HERE?!
DDEFS += -DDISABLE_PIN_STATE_VALIDATION=TRUE

# temporary or not? (pps noise stuff)
DDEFS += -DETB_INTERMITTENT_LIMIT=60001

DDEFS += -DSTM32_ADC_USE_ADC3=TRUE
# todo: make knock pin software-selectable?
# todo: DDEFS += -DEFI_SOFTWARE_KNOCK=TRUE

DDEFS += -DEFI_SOFTWARE_KNOCK=TRUE -DSTM32_ADC_USE_ADC3=TRUE
