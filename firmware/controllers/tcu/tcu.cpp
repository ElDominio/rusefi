/**
 * @file        tcu.cpp
 * @brief       Base classes for gear selection and transmission control
 *
 * @date Aug 31, 2020
 * @author David Holdeman, (c) 2020
 */

#include "pch.h"

#include "tcu.h"

#if EFI_TCU
void TransmissionControllerBase::init() {
}

void TransmissionControllerBase::update(gear_e /*gear*/) {
	postState();
}

gear_e TransmissionControllerBase::setCurrentGear(gear_e gear) {
    currentGear = gear;
    return getCurrentGear();
}

gear_e TransmissionControllerBase::getCurrentGear() const {
    return currentGear;
}

void TransmissionControllerBase::postState() {
#if EFI_TUNER_STUDIO
	auto iss = Sensor::get(SensorType::InputShaftSpeed);
	auto rpm = Sensor::get(SensorType::Rpm);
	if (iss.Valid && rpm.Valid) {
		tcRatio = rpm.Value / iss.Value;
	}

	// "Current Gear" is the gear we're actually (believed to be) in, derived from the VSS/RPM
	// ratio via GearDetector (same source as the "Detected Gear" gauge) -- not a mirror of
	// what was last commanded to the solenoids (that's "Desired Gear", tcuDesiredGear).
	// Requires "Forward gear count" and the per-gear ratios in the Speed Sensor dialog to be
	// configured; until then SensorType::DetectedGear stays invalid and this just retains its
	// last value.
	auto detectedGear = Sensor::get(SensorType::DetectedGear);
	if (detectedGear.Valid) {
		tcuCurrentGear = static_cast<int8_t>(detectedGear.Value);
	}
#endif
}

// call to mark the start of the shift
void TransmissionControllerBase::measureShiftTime(gear_e gear) {
	m_shiftTime = true;
	m_shiftTimer.reset();
	m_shiftTimeGear = gear;
}

float TransmissionControllerBase::isShiftCompleted() {
	auto detected = Sensor::get(SensorType::DetectedGear);
	auto iss = Sensor::get(SensorType::InputShaftSpeed);
	// If gear detection is set up and the gear we are trying to shift into has been detected
	if (detected.Valid && m_shiftTime && (int)m_shiftTimeGear == detected.Value) {
		m_shiftTime = false;
		return m_shiftTimer.getElapsedSeconds();
		// If ISS isn't configured, we want to use a fixed value.
	} else if (!iss.Valid && m_shiftTime && m_shiftTimer.hasElapsedMs(config->tcu_shiftTime)) {
		m_shiftTime = false;
		// convert ms to seconds for gauge
		return config->tcu_shiftTime * 0.001;
	} else {
		// a return value of 0 means the shift is not completed yet
		return 0;
	}
}

void TransmissionControllerBase::updateTccLockup(gear_e gear) {
	// never lock up mid-shift, below the configured minimum gear (also excludes Reverse/Neutral),
	// or with the brake pedal pressed
	if (isShifting || static_cast<int>(gear) < config->tcu_tccMinGear || getBrakePedalState()) {
		torqueConverterDuty = 0;
		enginePins.tcuTccOnoffSolenoid.setValue(0);
		return;
	}

	auto tps = Sensor::get(SensorType::DriverThrottleIntent);
	auto vss = Sensor::get(SensorType::VehicleSpeed);
	if (!tps.Valid || !vss.Valid) {
		torqueConverterDuty = 0;
		enginePins.tcuTccOnoffSolenoid.setValue(0);
		return;
	}

	if (config->tcu_tccMinClt != 0) {
		auto clt = Sensor::get(SensorType::Clt);
		if (!clt.Valid || clt.Value < config->tcu_tccMinClt) {
			torqueConverterDuty = 0;
			enginePins.tcuTccOnoffSolenoid.setValue(0);
			return;
		}
	}

	int lockSpeed = interpolate2d(tps.Value, config->tcu_tccTpsBins, config->tcu_tccLockSpeed);
	int unlockSpeed = interpolate2d(tps.Value, config->tcu_tccTpsBins, config->tcu_tccUnlockSpeed);
	if (vss.Value > lockSpeed) {
		// torqueConverterDuty is only used for a gauge -- this is an on/off solenoid, not PWM
		torqueConverterDuty = 100;
		enginePins.tcuTccOnoffSolenoid.setValue(1);
	} else if (vss.Value < unlockSpeed) {
		torqueConverterDuty = 0;
		enginePins.tcuTccOnoffSolenoid.setValue(0);
	}
	// else: inside the lock/unlock hysteresis band, hold the current solenoid state
}
#endif // EFI_TCU
