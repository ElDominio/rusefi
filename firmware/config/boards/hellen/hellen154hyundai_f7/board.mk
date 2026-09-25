# Combine the related files for a specific platform and MCU.

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp
# Set this if you want a default engine type other than normal
ifeq ($(VAR_DEF_ENGINE_TYPE),)
  VAR_DEF_ENGINE_TYPE = -DDEFAULT_ENGINE_TYPE=engine_type_e::HELLEN_154_HYUNDAI_COUPE_BK2
endif


DDEFS += -DBOARD_MC33810_COUNT=1
DDEFS += -DBOARD_TLE9201_COUNT=1

	DDEFS += -DCH_DBG_ENABLE_ASSERTS=FALSE
	DDEFS += -DENABLE_PERF_TRACE=FALSE
	# Persist TS page 5 (and page 4 second tables, e.g. the Lua script) in internal flash on F7.
	# This relocates the config above the first 1.5MB of flash into 128KB sectors,
	# which is the only F7 layout where extra-page piggybacking is valid - see the
	# STM32F7XX guard in storage_flash.cpp::getExtraPageFlashAddr(). Without this the
	# extra pages have no internal-flash backend and reset to defaults every boot.
	include $(PROJECT_DIR)/hw_layer/ports/stm32/2mb_flash.mk
	# SD card is for datalogging only - never use it as a settings/config backend.
	# EFI_STORAGE_SD defaults TRUE (USE_FATFS=yes), which would otherwise register SD
	# as the last storage backend and let a stale custom_page.bin clobber the flash
	# copy of page 5 on read. Datalogging is gated separately by EFI_FILE_LOGGING.
	DDEFS += -DEFI_STORAGE_SD=FALSE



# Add them all together
DDEFS += $(VAR_DEF_ENGINE_TYPE)
DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_HELLEN_154_HYUNDAI

include $(BOARDS_DIR)/hellen/hellen-common-mega144.mk
DDEFS += -DHW_HELLEN_HYUNDAI=1
DDEFS += -DEFI_WOT_ENRICHMENT=TRUE

# F7 default is already EFI_EMBED_INI_MSD FALSE (see config/stm32f7ems/efifeatures.h), but
# firmware/bin/gen_image_board.sh decides whether to build the uncompressed INI ramdisk by
# grepping this board.mk for the literal string, not by evaluating the macro - without an
# explicit copy here it tries (and fails) to fit the multi-hundred-KB INI into the 136KB
# uncompressed ramdisk image (mkfs.fat/create_ini_image.sh error).
DDEFS += -DEFI_EMBED_INI_MSD=FALSE
