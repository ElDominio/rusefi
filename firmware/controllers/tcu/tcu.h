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
public:
	virtual void update(gear_e);
	virtual void init();
	virtual gear_e getCurrentGear() const;
	virtual TransmissionControllerMode getMode() const {
		return TransmissionControllerMode::None;
	}
protected:
	gear_e currentGear = NEUTRAL;
	gear_e shiftingFrom = NEUTRAL;
	virtual gear_e setCurrentGear(gear_e);
	void postState();
	void measureShiftTime(gear_e);
	float isShiftCompleted();
	// Simple hysteresis-based torque converter clutch lock-up: TPS-dependent lock/unlock
	// vehicle speed curve, inhibited while shifting, below the minimum gear/CLT, or with
	// the brake pedal pressed. Drives enginePins.tcuTccOnoffSolenoid.
	void updateTccLockup(gear_e gear);
};
#endif // EFI_TCU
