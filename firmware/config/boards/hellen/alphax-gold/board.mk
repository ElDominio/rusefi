

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
DDEFS += -DEFI_STORAGE_SD=FALSE






DDEFS += -DBOOT_BACKDOOR_ENTRY_TIMEOUT_MS=0

# let's start quickly
DDEFS += -DHW_HELLEN_SKIP_BOARD_TYPE=TRUE
DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_ALPHAX_GOLD

DDEFS += -DEFI_EMBED_INI_MSD=FALSE
DDEFS += -DEFI_LOGIC_ANALYZER=FALSE
DDEFS += -DEFI_ENGINE_SNIFFER=FALSE
DDEFS += -DEFI_ENGINE_EMULATOR=FALSE
DDEFS += -DEFI_TOOTH_LOGGER=FALSE

# AlphaX custom features (TS page 5)
DDEFS += -DEFI_VVT_COMPENSATION=TRUE
DDEFS += -DEFI_ADVANCED_FUEL_PUMP=TRUE
DDEFS += -DEFI_EXHAUST_CUTOUT=TRUE
DDEFS += -DEFI_DOWNSHIFT_BLIPPER=TRUE
DDEFS += -DEFI_UPSHIFT_RPM_HOLD=TRUE
DDEFS += -DEFI_ENGINE_STATE_MACHINE=TRUE
DDEFS += -DEFI_MISFIRE_DETECTION=TRUE
DDEFS += -DEFI_CLUTCH_DELAY_VALVE=TRUE
DDEFS += -DEFI_LAUNCH_POWER_RAMP=TRUE
DDEFS += -DEFI_BURST_KNOCK=TRUE
DDEFS += -DEFI_WOT_ENRICHMENT=TRUE
DDEFS += -DEFI_SPORT_PEDAL=TRUE
DDEFS += -DEFI_AC_PRESSURE_FAN=TRUE
DDEFS += -DEFI_OFF_IDLE_RPM_ADDER=TRUE
DDEFS += -DEFI_LUA_LIMITER=TRUE
