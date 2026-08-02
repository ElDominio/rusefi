#include "pch.h"

#include "gc_auto.h"
#include "engine_state_machine.h"

#if EFI_TCU
AutomaticGearController automaticGearController;

AutomaticGearController::AutomaticGearController() {
}

void AutomaticGearController::update() {
	auto tps = Sensor::get(SensorType::DriverThrottleIntent);
	auto vss = Sensor::get(SensorType::VehicleSpeed);

	if (getDesiredGear() == NEUTRAL) {
		setDesiredGear(GEAR_1);
	}

	// At idle, force a downshift to 1st so the transmission is ready to move off from a stop.
	// "Idle" is whatever the Engine State Machine says it is (engineSmIsIdle) -- same source
	// other idle-only features use (see MisfireController::onEnginePhase) -- rather than a
	// separate RPM/TPS check duplicated here. Configurable via "Shift to First if Idle" and
	// the VSS threshold below it in Transmission Settings; a VSS threshold of 0 means only
	// the state machine's idle condition applies (no additional speed gate).
	bool idleShiftToFirst = false;
	if (config->tcuIdleShiftToFirstEnabled) {
		bool idle = engine->module<EngineStateMachine>().unmock().engineSmIsIdle;
		bool vssOk = config->tcuIdleShiftToFirstMaxVss == 0
				|| (vss.Valid && vss.Value < config->tcuIdleShiftToFirstMaxVss);
		if (idle && vssOk && getDesiredGear() > GEAR_1) {
			setDesiredGear(GEAR_1);
			idleShiftToFirst = true;
		}
	}
	if (transmissionController != nullptr) {
		transmissionController->tcu_idleShiftToFirst = idleShiftToFirst;
	}

	if (tps.Valid && vss.Valid) {
		switch (getDesiredGear()) {
		case GEAR_1 :
			shift(vss.Value, tps.Value, &config->tcu_shiftSpeed12, GEAR_2);
			break;
		case GEAR_2 :
			shift(vss.Value, tps.Value, &config->tcu_shiftSpeed21, GEAR_1, true);
			shift(vss.Value, tps.Value, &config->tcu_shiftSpeed23, GEAR_3);
			break;
		case GEAR_3 :
			shift(vss.Value, tps.Value, &config->tcu_shiftSpeed32, GEAR_2, true);
			shift(vss.Value, tps.Value, &config->tcu_shiftSpeed34, GEAR_4);
			break;
		case GEAR_4 :
			shift(vss.Value, tps.Value, &config->tcu_shiftSpeed43, GEAR_3, true);
			break;
		default :
			break;
		}
	}
	
	GearControllerBase::update();
}

void AutomaticGearController::shift(float speed, float throttle, uint8_t (*curve)[TCU_TABLE_WIDTH], gear_e gear) {
	shift(speed, throttle, curve, gear, false);
}

void AutomaticGearController::shift(float speed, float throttle, uint8_t (*curve)[TCU_TABLE_WIDTH], gear_e gear, bool down) {
	int curveSpeed = interpolate2d(throttle, config->tcu_shiftTpsBins, *curve);

	// Publish how far current speed is from the next shift point, for the "Upshift Margin" /
	// "Downshift Margin" gauges. Gears with no applicable edge in one direction (e.g. gear 1
	// has no downshift) simply retain their last computed value for that gauge.
	if (transmissionController != nullptr) {
		if (down) {
			transmissionController->tcu_downshiftMargin = speed - curveSpeed;
		} else {
			transmissionController->tcu_upshiftMargin = curveSpeed - speed;
		}
	}

	if ((down && speed < curveSpeed) || (!down && speed > curveSpeed)) {
		setDesiredGear(gear);
	}
}

AutomaticGearController* getAutomaticGearController() {
	return &automaticGearController;
}
#endif // EFI_TCU
