# Combine the related files for a specific platform and MCU.

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp
# Set this if you want a default engine type other than normal
ifeq ($(VAR_DEF_ENGINE_TYPE),)
  VAR_DEF_ENGINE_TYPE = -DDEFAULT_ENGINE_TYPE=engine_type_e::HELLEN_154_HYUNDAI_COUPE_BK2
endif




# Add them all together
DDEFS += $(VAR_DEF_ENGINE_TYPE)
DDEFS += -DEFI_SOFTWARE_KNOCK=TRUE -DSTM32_ADC_USE_ADC3=TRUE


DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_HELLEN_154_HYUNDAI

include $(BOARDS_DIR)/hellen/hellen-common144.mk
DDEFS += -DHW_HELLEN_HYUNDAI=1

# F4 flash-space savers

# AlphaX custom features (TS page 5)

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
