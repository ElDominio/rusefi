#pragma once

#include "tcu.h"

#if EFI_TCU
class Generic4TransmissionController: public SimpleTransmissionController {
public:
	void update(gear_e);
	void init();
	TransmissionControllerMode getMode() const {
		return TransmissionControllerMode::Generic4;
	}
#if EFI_UNIT_TEST
	void resetForUnitTest() override {
		TransmissionControllerBase::resetForUnitTest();
		m_pcDutyRamped = 0;
		m_pcRampTimer.reset();
	}
#endif // EFI_UNIT_TEST
private:
	void setPcState(gear_e desiredGear);
	// Slew-rate state for the line pressure solenoid duty (tcu_pcRampTimeMs): persists across
	// calls so setPcState() can move the output gradually toward its computed target instead of
	// stepping instantly.
	Timer m_pcRampTimer;
	float m_pcDutyRamped = 0;
};

Generic4TransmissionController* getGeneric4TransmissionController();
#endif // EFI_TCU
