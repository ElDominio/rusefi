MODULES_INC += $(PROJECT_DIR)/controllers/modules/wot_enrichment
MODULES_CPPSRC += $(PROJECT_DIR)/controllers/modules/wot_enrichment/wot_enrichment.cpp
MODULES_INCLUDE += \#include "wot_enrichment.h"\n
MODULES_LIST += WotEnrichment,

# this define needs to be used in any file where the module is used (ie to not generate a compile error when we deactivate this module)
DDEFS += -DMODULE_WOT_ENRICHMENT
