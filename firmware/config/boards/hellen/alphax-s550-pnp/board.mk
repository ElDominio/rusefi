# Combine the related files for a specific platform and MCU.

# Target ECU board design
BOARDCPPSRC = $(BOARD_DIR)/board_configuration.cpp




# TLS115_PG (PD12)
DDEFS += -DDIAG_5VP_PIN=Gpio::D12

LED_CRITICAL_ERROR_BRAIN_PIN = -DLED_CRITICAL_ERROR_BRAIN_PIN=H176_MCU_MEGA_LED1_RED
include $(BOARDS_DIR)/hellen/hellen-common-mega176.mk

ifeq ($(PROJECT_CPU),ARCH_STM32F7)
	DDEFS += -DCH_DBG_ENABLE_ASSERTS=FALSE
	DDEFS += -DENABLE_PERF_TRACE=FALSE
else ifeq ($(PROJECT_CPU),ARCH_STM32F4)
    # This board has trigger scope hardware!
    DDEFS += -DTRIGGER_SCOPE
    # serial ports only on F4
	DDEFS += $(PRIMARY_COMMUNICATION_PORT_USART2)
else
$(error Unsupported PROJECT_CPU [$(PROJECT_CPU)])
endif


DDEFS += -DLUA_STM32_STANDBY=1



# watchdog suddenly i see it #8699
DDEFS += -DHAL_USE_WDG=FALSE
