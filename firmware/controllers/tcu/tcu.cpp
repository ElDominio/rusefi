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
	m_gearMatching = false;
}

float TransmissionControllerBase::isShiftCompleted() {
	auto detected = Sensor::get(SensorType::DetectedGear);
	auto iss = Sensor::get(SensorType::InputShaftSpeed);
	// If gear detection is set up and the gear we are trying to shift into has been detected,
	// require the match to hold continuously for tcu_shiftGearConfirmTime before declaring the
	// shift done. GearDetector re-derives DetectedGear from a raw instantaneous RPM ratio every
	// tick with no debounce of its own, and an RPM flare/sag through the target gear's ratio band
	// during clutch handoff can pass that check for a single tick before the clutch has actually
	// locked up -- without this debounce that shows up as a one-tick false "shift complete".
	if (detected.Valid && m_shiftTime) {
		if ((int)m_shiftTimeGear == detected.Value) {
			if (!m_gearMatching) {
				m_gearMatching = true;
				m_gearMatchTimer.reset();
			} else if (m_gearMatchTimer.hasElapsedMs(config->tcu_shiftGearConfirmTime)) {
				m_shiftTime = false;
				m_gearMatching = false;
				return m_shiftTimer.getElapsedSeconds();
			}
			return 0;
		} else {
			m_gearMatching = false;
		}
	}

	// If ISS isn't configured, we want to use a fixed value.
	if (!iss.Valid && m_shiftTime && m_shiftTimer.hasElapsedMs(config->tcu_shiftTime)) {
		m_shiftTime = false;
		// convert ms to seconds for gauge
		return config->tcu_shiftTime * 0.001;
	}

	// a return value of 0 means the shift is not completed yet
	return 0;
}

// torqueConverterDuty/tcu_tccLockupOn are only used for gauges -- this is an on/off solenoid, not PWM
void TransmissionControllerBase::setLockupState(bool locked) {
	if (locked && !tcu_tccLockupOn) {
		// rising edge: lock-up is engaging now, start the line-pressure duty adder window
		m_lockupEngageTimer.reset();
	}
	tcu_tccLockupOn = locked;
	torqueConverterDuty = locked ? 100 : 0;
	enginePins.tcuTccOnoffSolenoid.setValue(locked ? 1 : 0);
}

void TransmissionControllerBase::updateTccLockup(gear_e gear) {
	// never lock up mid-shift, below the configured minimum gear (also excludes Reverse/Neutral),
	// or with the brake pedal pressed
	if (isShifting || static_cast<int>(gear) < config->tcu_tccMinGear || getBrakePedalState()) {
		setLockupState(false);
		return;
	}

	auto tps = Sensor::get(SensorType::DriverThrottleIntent);
	auto vss = Sensor::get(SensorType::VehicleSpeed);
	if (!tps.Valid || !vss.Valid) {
		setLockupState(false);
		return;
	}

	if (config->tcu_tccMinClt != 0) {
		auto clt = Sensor::get(SensorType::Clt);
		if (!clt.Valid || clt.Value < config->tcu_tccMinClt) {
			setLockupState(false);
			return;
		}
	}

	int lockSpeed = interpolate2d(tps.Value, config->tcu_tccTpsBins, config->tcu_tccLockSpeed);
	int unlockSpeed = interpolate2d(tps.Value, config->tcu_tccTpsBins, config->tcu_tccUnlockSpeed);
	if (vss.Value > lockSpeed) {
		setLockupState(true);
	} else if (vss.Value < unlockSpeed) {
		setLockupState(false);
	}
	// else: inside the lock/unlock hysteresis band, hold the current solenoid state
}

int8_t TransmissionControllerBase::getPcLockupAdderDuty() const {
	if (!tcu_tccLockupOn || m_lockupEngageTimer.hasElapsedMs(config->tcu_pcLockupAdderTime)) {
		return 0;
	}
	return config->tcu_pcLockupAdderDuty;
}
#endif // EFI_TCU
