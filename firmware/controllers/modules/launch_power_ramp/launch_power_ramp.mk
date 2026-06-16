MODULES_INC += $(PROJECT_DIR)/controllers/modules/launch_power_ramp
MODULES_CPPSRC += $(PROJECT_DIR)/controllers/modules/launch_power_ramp/launch_power_ramp.cpp
MODULES_INCLUDE += \#include "launch_power_ramp.h"\n
MODULES_LIST += LaunchPowerRamp,

# this define needs to be used in any file where the module is used (ie to not generate a compile error when we deactivate this module)
DDEFS += -DMODULE_LAUNCH_POWER_RAMP
