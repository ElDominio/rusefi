MODULES_INC += $(PROJECT_DIR)/controllers/modules/rolling_launch
MODULES_CPPSRC += $(PROJECT_DIR)/controllers/modules/rolling_launch/rolling_launch.cpp
MODULES_INCLUDE += \#include "rolling_launch.h"\n
MODULES_LIST += RollingLaunchControl,

# this define needs to be used in any file where the module is used (ie to not generate a compile error when we deactivate this module)
DDEFS += -DMODULE_ROLLING_LAUNCH
