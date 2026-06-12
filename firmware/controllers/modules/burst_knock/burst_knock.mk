MODULES_INC += $(PROJECT_DIR)/controllers/modules/burst_knock
MODULES_CPPSRC += $(PROJECT_DIR)/controllers/modules/burst_knock/burst_knock.cpp
MODULES_INCLUDE += \#include "burst_knock.h"\n
MODULES_LIST += BurstKnock,

# this define needs to be used in any file where the module is used (ie to not generate a compile error when we deactivate this module)
DDEFS += -DMODULE_BURST_KNOCK
