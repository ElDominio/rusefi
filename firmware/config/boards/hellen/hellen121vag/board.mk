# Combine the related files for a specific platform and MCU.

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp
# Set this if you want a default engine type other than normal hellen121vag
ifeq ($(VAR_DEF_ENGINE_TYPE),)
  VAR_DEF_ENGINE_TYPE = -DDEFAULT_ENGINE_TYPE=engine_type_e::HELLEN_121_VAG_4_CYL
endif

# Add them all together
DDEFS += -DFIRMWARE_ID=\"hellen121vag\" $(VAR_DEF_ENGINE_TYPE)
DDEFS += -DEFI_SOFTWARE_KNOCK=TRUE -DSTM32_ADC_USE_ADC3=TRUE

# Use less Flash memory for this config
ifeq ($(EFI_LUA_LOOKUP),)
  EFI_LUA_LOOKUP = FALSE
endif
DDEFS += -DEFI_LUA_LOOKUP=$(EFI_LUA_LOOKUP)
DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_HELLEN_121VAG

# this board uses original MCU module before mega-module generation
LED_CRITICAL_ERROR_BRAIN_PIN = -DLED_CRITICAL_ERROR_BRAIN_PIN=H176_ORIGINAL_MCU_LED1_RED
include $(BOARDS_DIR)/hellen/hellen-common176.mk
# This is legacy MCU module
DDEFS += -DHELLEN_BOARD_MCU176=TRUE

DDEFS += -DHW_HELLEN_121_VAG=1


# we do not have much Lua RAM, let's drop some fancy functions
DDEFS += -DWITH_LUA_CONSUMPTION=FALSE
DDEFS += -DWITH_LUA_PID=FALSE
DDEFS += -DWITH_LUA_STOP_ENGINE=FALSE

# [tag:alphax_cpu_feature_policy] AlphaX feature-set policy: every F4 board uses
# fw-custom-paralela-master's EFI_* feature set (EFI_EXTERNAL_CAN_ETB excluded: paralela-only
# hardware), every F7 board uses alphax-s550-pnp's (F7 efifeatures defaults, no SD config
# storage, no embedded INI). Board hardware defines stay above. Keep this block in sync with
# those two boards.
ifeq ($(PROJECT_CPU),ARCH_STM32F4)
	DDEFS += -DEFI_HPFP=FALSE
	DDEFS += -DEFI_WIDEBAND_FIRMWARE_UPDATE=FALSE
	DDEFS += -DEFI_EMBED_INI_MSD=FALSE
	DDEFS += -DEFI_LOGIC_ANALYZER=FALSE
	DDEFS += -DEFI_ENGINE_SNIFFER=FALSE
	DDEFS += -DEFI_ENGINE_EMULATOR=FALSE
	DDEFS += -DEFI_TOOTH_LOGGER=FALSE
	DDEFS += -DEFI_VVT_COMPENSATION=TRUE
	DDEFS += -DEFI_EXHAUST_CUTOUT=TRUE
	DDEFS += -DEFI_DOWNSHIFT_BLIPPER=TRUE
	DDEFS += -DEFI_UPSHIFT_RPM_HOLD=TRUE
	DDEFS += -DEFI_GHOST_CAM=TRUE
	DDEFS += -DEFI_CLUTCH_DELAY_VALVE=TRUE
	DDEFS += -DEFI_OFF_IDLE_RPM_ADDER=TRUE
	DDEFS += -DEFI_LUA_LIMITER=TRUE
	DDEFS += -DEFI_CRANKING_NO_SPARK=TRUE
	DDEFS += -DEFI_SPORT_PEDAL=TRUE
	DDEFS += -DEFI_DYNO_VIEW=FALSE
	DDEFS += -DEFI_ETHERNET=FALSE
	DDEFS += -DEFI_HELLA_OIL=FALSE
	DDEFS += -DEFI_LAUNCH_POWER_RAMP=TRUE
	DDEFS += -DEFI_MISFIRE_DETECTION=TRUE
	DDEFS += -DEFI_UART_GPS=FALSE
	DDEFS += -DEFI_WIFI=FALSE
	DDEFS += -DKNOCK_SPECTROGRAM=FALSE
endif
