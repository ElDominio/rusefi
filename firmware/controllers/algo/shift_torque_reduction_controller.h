//
// Created by kifir on 9/27/24.
//

#pragma once

#include "shift_torque_reduction_state_generated.h"

class ShiftTorqueReductionController : public shift_torque_reduction_state_s {
public:
	void update();

	float getSparkSkipRatio();

	float getTorqueReductionIgnitionRetard();

	// True while a flat-shift torque cut is engaged. isFlatShiftConditionSatisfied is the
	// single gate that enables both the spark skip (getSparkSkipRatio) and the ignition
	// retard (getTorqueReductionIgnitionRetard), so it is exactly "currently cutting torque".
	// Consumed by the Engine State Machine to raise its Torque Reduction overlay.
	bool isCuttingTorque() const { return isFlatShiftConditionSatisfied; }

private:
	void updateTriggerPinState();
	void updateTriggerPinState(switch_input_pin_e pin, pin_input_mode_e mode, const bool invertPhysicalPin, bool invalidPinState);

	void updateTimeConditionSatisfied();
	void updateRpmConditionSatisfied();
	void updateAppConditionSatisfied();
	void updateSpeedConditionSatisfied();

	Timer m_pinTriggeredTimer;
};
