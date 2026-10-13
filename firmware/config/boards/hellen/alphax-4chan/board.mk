# Combine the related files for a specific platform and MCU.

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp


# Add them all together

# pretty temporary?
DDEFS += -DDISABLE_PIN_STATE_VALIDATION=TRUE

# temporary or not?
DDEFS += -DETB_INTERMITTENT_LIMIT=60001

# quick board start-up with less fancy bootloader
DDEFS += -DBOOT_BACKDOOR_ENTRY_TIMEOUT_MS=0

# 1mb is not enough for everything we have
DDEFS += -DEFI_LOGIC_ANALYZER=FALSE
DDEFS += -DEFI_HPFP=FALSE
MODULE_MIL = no
MODULE_DTC_MANAGER = no

include $(BOARDS_DIR)/hellen/hellen-common-mega144.mk

ifeq ($(PROJECT_CPU),ARCH_STM32F7)
  # need boot times of under 350ms for car that expects fast CAN https://github.com/rusefi/alphax-4chan/issues/184
  DDEFS += -DHW_HELLEN_SKIP_BOARD_TYPE=TRUE
	DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_ALPHAX_4CHAN_F7
    # TODO do we only support serial on F7 but not UART?
    DDEFS += -DEFI_CONSOLE_TX_BRAIN_PIN=Gpio::D6 -DEFI_CONSOLE_RX_BRAIN_PIN=Gpio::D5
    # Persist TS page 5 (and page 4 second tables, incl. Lua script) in internal flash on F7.
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
    DDEFS += -DTS_PRIMARY_UxART_PORT=SD2 -DEFI_TS_PRIMARY_IS_SERIAL=TRUE -DSTM32_SERIAL_USE_USART2=TRUE -DSTM32_UART_USE_USART2=FALSE
else ifeq ($(PROJECT_CPU),ARCH_STM32F4)
	DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_ALPHAX_4CHAN
	DDEFS += $(PRIMARY_COMMUNICATION_PORT_USART2)
	# Same EFI_* feature set as fw-custom-paralela-master (F4 RAM/flash budget), minus
	# EFI_EXTERNAL_CAN_ETB which needs paralela's external CAN ETB hardware.
	DDEFS += -DEFI_HPFP=FALSE
	DDEFS += -DEFI_WIDEBAND_FIRMWARE_UPDATE=FALSE
	DDEFS += -DEFI_EMBED_INI_MSD=FALSE
	DDEFS += -DEFI_LOGIC_ANALYZER=FALSE
	DDEFS += -DEFI_ENGINE_SNIFFER=FALSE
	DDEFS += -DEFI_ENGINE_EMULATOR=FALSE
	DDEFS += -DEFI_TOOTH_LOGGER=FALSE
	DDEFS += -DEFI_VVT_COMPENSATION=TRUE
	DDEFS += -DEFI_VVT_ADVANCED_MODE=TRUE
	DDEFS += -DEFI_ADVANCED_FUEL_PUMP=TRUE
	DDEFS += -DEFI_EXHAUST_CUTOUT=TRUE
	DDEFS += -DEFI_DOWNSHIFT_BLIPPER=TRUE
	DDEFS += -DEFI_UPSHIFT_RPM_HOLD=TRUE
	DDEFS += -DEFI_ENGINE_STATE_MACHINE=TRUE
	DDEFS += -DEFI_GHOST_CAM=TRUE
	DDEFS += -DEFI_CLUTCH_DELAY_VALVE=TRUE
	DDEFS += -DEFI_OFF_IDLE_RPM_ADDER=TRUE
	DDEFS += -DEFI_LUA_LIMITER=TRUE
	DDEFS += -DEFI_CRANKING_NO_SPARK=TRUE
	DDEFS += -DEFI_ROLLING_LAUNCH=TRUE
	DDEFS += -DEFI_SPORT_PEDAL=TRUE
	DDEFS += -DEFI_AC_PRESSURE_FAN=TRUE
	DDEFS += -DEFI_BURST_KNOCK=TRUE
	DDEFS += -DEFI_DYNO_VIEW=FALSE
	DDEFS += -DEFI_ETHERNET=FALSE
	DDEFS += -DEFI_HELLA_OIL=FALSE
	DDEFS += -DEFI_LAUNCH_POWER_RAMP=TRUE
	DDEFS += -DEFI_MISFIRE_DETECTION=TRUE
	DDEFS += -DEFI_UART_GPS=FALSE
	DDEFS += -DEFI_WIFI=FALSE
	DDEFS += -DEFI_WOT_ENRICHMENT=TRUE
else
$(error Unsupported PROJECT_CPU [$(PROJECT_CPU)])
endif

DDEFS += -DHW_HELLEN_4CHAN=1

# INI no longer fits the embedded ramdisk image (Disk full)
DDEFS += -DEFI_EMBED_INI_MSD=FALSE
