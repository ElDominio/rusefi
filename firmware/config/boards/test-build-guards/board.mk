BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp

DDEFS += -DFIRMWARE_ID=\"t-b-g\"
DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_TBG

DDEFS += -DHAL_USE_ADC=FALSE
DDEFS += -DHAL_USE_GPT=FALSE

# INI no longer fits the embedded ramdisk image (Disk full)
DDEFS += -DEFI_EMBED_INI_MSD=FALSE
