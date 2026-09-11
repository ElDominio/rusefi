#pragma once

#include "global.h"
#include "io_pins.h"
#include "persistent_configuration.h"
#include "generated_lookup_engine_configuration.h"
#include "simple_tcu.h"
#include "tc_4l6x.h"

#if EFI_TCU
class GearControllerBase {
public:
	virtual void update();
	virtual gear_e getDesiredGear() const;
	virtual void init();
	virtual GearControllerMode getMode() const {
		return GearControllerMode::ButtonShift;
	}
	// update() checks this against NULL before dereferencing it, which only worked because every
	// production controller is a file-scope instance and therefore zero initialized
	TransmissionControllerBase *transmissionController = nullptr;
#if EFI_UNIT_TEST
	// Production controllers are file-scope singletons reused by every test, so without this
	// seam desiredGear leaks between tests depending on execution order.
	void resetForUnitTest() {
		desiredGear = NEUTRAL;
	}
#endif // EFI_UNIT_TEST
protected:
	virtual gear_e setDesiredGear(gear_e);
	void initTransmissionController();
	float* getRangeStateArray(int);
private:
	gear_e desiredGear = NEUTRAL;
	void postState();
};

void initGearController();
#endif // EFI_TCU
