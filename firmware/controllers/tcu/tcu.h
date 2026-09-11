/**
 * @file        tcu.h
 * @brief       Base classes for gear selection and transmission control
 *
 * @date Aug 31, 2020
 * @author David Holdeman, (c) 2020
 */
#pragma once

#include "global.h"
#include "io_pins.h"
#include "persistent_configuration.h"
#include "generated_lookup_engine_configuration.h"
#include "tcu_controller_generated.h"
#include <rusefi/timer.h>

#if EFI_TCU
class TransmissionControllerBase: public tcu_controller_s {
private:
	Timer m_shiftTimer;
	bool m_shiftTime = false;
	gear_e m_shiftTimeGear;
	Timer m_gearMatchTimer;
	bool m_gearMatching = false;
	Timer m_lockupEngageTimer;
public:
	virtual void update(gear_e);
	virtual void init();
	virtual gear_e getCurrentGear() const;
	virtual TransmissionControllerMode getMode() const {
		return TransmissionControllerMode::None;
	}
#if EFI_UNIT_TEST
	// Production controllers are file-scope singletons reused by every test, so without this
	// seam gear/shift state leaks between tests depending on execution order. Virtual so a
	// subclass with its own persistent state (e.g. Generic4TransmissionController's line
	// pressure ramp) can extend it -- gear_controller.cpp calls this through a base pointer.
	virtual void resetForUnitTest() {
		currentGear = NEUTRAL;
		shiftingFrom = NEUTRAL;
		isShifting = false;
		m_shiftTime = false;
		m_gearMatching = false;
		tcu_idleShiftToFirst = false;
		tcu_tccLockupOn = false;
		torqueConverterDuty = 0;
		pressureControlDuty = 0;
	}
#endif // EFI_UNIT_TEST
protected:
	gear_e currentGear = NEUTRAL;
	gear_e shiftingFrom = NEUTRAL;
	virtual gear_e setCurrentGear(gear_e);
	void postState();
	void measureShiftTime(gear_e);
	float isShiftCompleted();
	// Simple hysteresis-based torque converter clutch lock-up: TPS-dependent lock/unlock
	// vehicle speed curve, inhibited while shifting, below the minimum gear/CLT, or with
	// the brake pedal pressed. Drives enginePins.tcuTccOnoffSolenoid and tcu_tccLockupOn.
	void updateTccLockup(gear_e gear);
	// Extra (signed) line pressure duty to apply for tcu_pcLockupAdderTime after TCC lock-up
	// engages (helps control converter clutch apply shock). Returns 0 outside that window, or
	// while not locked up.
	int8_t getPcLockupAdderDuty() const;
private:
	void setLockupState(bool locked);
};
#endif // EFI_TCU
