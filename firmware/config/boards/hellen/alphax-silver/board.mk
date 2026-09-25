

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp

# This board uses an STM32F427 (F42x/43x line), which has an extra 64k SRAM3 bank
# on top of the base 128k SRAM1+SRAM2. IS_STM32F429=yes is the codebase's existing
# flag for that whole silicon family (see hw_layer/ports/stm32/stm32f4/hw_ports.mk
# and alphax-s197-v2/board.mk, another AlphaX board using this same flag) -- without
# it, this board is treated as plain F407 (128k RAM only) and loses the SRAM3 bank,
# which is needed headroom for the full AlphaX feature set below (was overflowing
# HEAP_RAM by ~10KB at link time without this).
ifeq ($(PROJECT_CPU),ARCH_STM32F4)
  IS_STM32F429 = yes
endif

# This board has three tle9104
DDEFS += -DBOARD_TLE9104_COUNT=3









ONBOARD_MEMS_TYPE=LIS2DH12

include $(BOARDS_DIR)/hellen/hellen-common100.mk






DDEFS += -DBOOT_BACKDOOR_ENTRY_TIMEOUT_MS=0

DDEFS += -DSTATIC_BOARD_ID=STATIC_BOARD_ID_ALPHAX_SILVER

DDEFS += $(PRIMARY_COMMUNICATION_PORT_USART2)

# INI has grown too large for the fixed-size embedded ramdisk (same "Disk full" issue every other
# AlphaX board - alphax-gold, alphax-s550-pnp, alphax-s197-v2, alphax-8chan - already opts out of)
DDEFS += -DEFI_EMBED_INI_MSD=FALSE

# AlphaX custom features (TS page 5)
DDEFS += -DEFI_VVT_COMPENSATION=TRUE
DDEFS += -DEFI_VVT_ADVANCED_MODE=TRUE
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
