MODULES_INC += $(PROJECT_DIR)/controllers/modules/cdv_controller
MODULES_CPPSRC += $(PROJECT_DIR)/controllers/modules/cdv_controller/cdv_controller.cpp
MODULES_INCLUDE += \#include "cdv_controller.h"\n
MODULES_LIST += CdvController,

# this define needs to be used in any file where the module is used (ie to not generate a compile error when we deactivate this module)
DDEFS += -DMODULE_CDV_CONTROLLER
