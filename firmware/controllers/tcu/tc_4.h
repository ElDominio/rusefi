#pragma once

#include "tcu.h"

#if EFI_TCU
class Generic4TransmissionController: public TransmissionControllerBase {
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
		m_pcSlipTrim = 0;
		m_pcSlipCleanTimer.reset();
	}
#endif // EFI_UNIT_TEST
private:
	void setPcState(gear_e desiredGear);
	void updateSlipTrim();
	// Applies the shift-solenoid on/off truth table (tcuSolenoidTable) for the given gear --
	// the actual mechanical actuation a "generic 4-speed" transmission needs regardless of
	// whether line pressure control (above) is populated at all. A board that only wires the
	// shift solenoid pins and leaves the EPC/TCC pins unconfigured gets shift-only behavior for
	// free, since those pins simply no-op when unconfigured.
	void updateShiftSolenoids(gear_e gear);
	// Slew-rate state for the line pressure solenoid duty (tcu_pcRampTimeMs): persists across
	// calls so setPcState() can move the output gradually toward its computed target instead of
	// stepping instantly.
	Timer m_pcRampTimer;
	float m_pcDutyRamped = 0;
	// Slip-based closed-loop trim state (tcu_pcSlip*) -- see updateSlipTrim(). Reset to 0 on
	// every commanded shift (Generic4TransmissionController::update()) and every key-on; not
	// persisted.
	float m_pcSlipTrim = 0;
	// How long slip has stayed at/below 0 continuously -- gates when the decay step starts.
	Timer m_pcSlipCleanTimer;
};

Generic4TransmissionController* getGeneric4TransmissionController();
#endif // EFI_TCU
