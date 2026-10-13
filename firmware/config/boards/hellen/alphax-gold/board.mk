

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp


# This board has four tle9104
DDEFS += -DBOARD_TLE9104_COUNT=4









ONBOARD_MEMS_TYPE=LIS2DH12

include $(BOARDS_DIR)/hellen/hellen-common-mega144.mk

# Persist TS page 5 (and page 4 second tables) in internal flash on F7.
# This relocates the config above the first 1.5MB of flash into 128KB sectors,
# which is the only F7 layout where extra-page piggybacking is valid - see the
# STM32F7XX guard in storage_flash.cpp::getExtraPageFlashAddr(). Without this the
# extra pages have no internal-flash backend and reset to defaults every boot.
include $(PROJECT_DIR)/hw_layer/ports/stm32/2mb_flash.mk
# SD card is for datalogging only - never use it as a settings/config backend.
# EFI_STORAGE_SD defaults TRUE (USE_FATFS=yes), which would otherwise register SD
# as the last storage backend and let a stale custom_page.bin clobber the flash
# copy of page 5 on read. Datalogging is gated separately by EFI_FILE_LOGGING.






DDEFS += -DBOOT_BACKDOOR_ENTRY_TIMEOUT_MS=0

# let's start quickly
DDEFS += -DHW_HELLEN_SKIP_BOARD_TYPE=TRUE
DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_ALPHAX_GOLD


# AlphaX custom features (TS page 5)

# [tag:alphax_cpu_feature_policy] AlphaX feature-set policy: every F4 board uses
# fw-custom-paralela-master's EFI_* feature set (EFI_EXTERNAL_CAN_ETB excluded: paralela-only
# hardware), every F7 board uses alphax-s550-pnp's (F7 efifeatures defaults, no SD config
# storage, no embedded INI). Board hardware defines stay above. Keep this block in sync with
# those two boards.
ifeq ($(PROJECT_CPU),ARCH_STM32F7)
	# F7 defaults (stm32f7ems/efifeatures.h) enable the AlphaX features; SD is datalogging only
	DDEFS += -DEFI_STORAGE_SD=FALSE
	DDEFS += -DEFI_EMBED_INI_MSD=FALSE
endif
