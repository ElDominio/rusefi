MODULES_INC += $(PROJECT_DIR)/controllers/modules/check_engine_light
MODULES_CPPSRC += $(PROJECT_DIR)/controllers/modules/check_engine_light/check_engine_light.cpp
MODULES_INCLUDE += \#include "check_engine_light.h"\n
MODULES_LIST += CheckEngineTriggering,

# this define needs to be used in any file where the module is used (ie to not generate a compile error when we deactivate this module)
DDEFS += -DMODULE_CHECK_ENGINE_TRIGGERING
