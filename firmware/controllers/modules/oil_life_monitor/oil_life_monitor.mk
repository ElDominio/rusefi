MODULES_INC += $(PROJECT_DIR)/controllers/modules/oil_life_monitor
MODULES_CPPSRC += $(PROJECT_DIR)/controllers/modules/oil_life_monitor/oil_life_monitor.cpp
MODULES_INCLUDE += \#include "oil_life_monitor.h"\n
MODULES_LIST += OilLifeMonitor,

# this define needs to be used in any file where the module is used (ie to not generate a compile error when we deactivate this module)
DDEFS += -DMODULE_OIL_LIFE_MONITOR
