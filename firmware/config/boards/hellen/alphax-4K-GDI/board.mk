

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp

# todo? hopefully we've used the right chips? include $(PROJECT_DIR)/hw_layer/ports/stm32/2mb_flash.mk

DDEFS += -DEFI_MC33816=TRUE
DDEFS += -DBOARD_TLE9104_COUNT=4

DDEFS += -DSTM32_ICU_USE_TIM1=TRUE -DSTM32_PWM_USE_TIM1=FALSE
DDEFS += -DEFI_SENT_SUPPORT=TRUE

# we have hellen board id BOARD_ID_GDI4CHAN_A

DDEFS += -DHW_HELLEN_4K_GDI=1
DDEFS += -DLUA_STM32_STANDBY=1

include $(BOARDS_DIR)/hellen/hellen-common-mega144.mk

# AlphaX custom features (TS page 5)

# [tag:alphax_cpu_feature_policy] AlphaX feature-set policy: every F4 board uses
# fw-custom-paralela-master's EFI_* feature set (EFI_EXTERNAL_CAN_ETB excluded: paralela-only
# hardware), every F7 board uses alphax-s550-pnp's (F7 efifeatures defaults, no SD config
# storage, no embedded INI). Board hardware defines stay above. Keep this block in sync with
# those two boards.
ifeq ($(PROJECT_CPU),ARCH_STM32F7)
	# F7 defaults (stm32f7ems/efifeatures.h) enable the AlphaX features.
	# Exception: no 2mb_flash.mk on this board (flash size unverified, see top), so SD is the
	# only backend for extra TS pages - keep EFI_STORAGE_SD at its default here.
	DDEFS += -DEFI_EMBED_INI_MSD=FALSE
endif
