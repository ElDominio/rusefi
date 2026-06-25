/*
 * @file rolling_launch.cpp
 *
 * See rolling_launch.h for the feature overview.
 */

#include "pch.h"
#include "custom_page.h"
#include "rolling_launch.h"

#if EFI_ROLLING_LAUNCH

void RollingLaunchControl::onSlowCallback() {
	auto cfg = getCustomPage();

	if (!cfg->rollingLaunchEnabled) {
		isRollingLaunchArmed = false;
		isRollingLaunchActive = false;
		rollingLaunchSwitchState = false;
		isRollingLaunchRampActive = false;
		rollingLaunchSparkSkipRatio = 0;
		rollingLaunchTimingRetardOut = 0;
		rollingLaunchRampElapsed = 0;
		capturedRpm = 0;
		rollingLaunchTargetRpm = 0;
		m_prevArmed = false;
		return;
	}

	const float rpm = Sensor::getOrZero(SensorType::Rpm);
	const float vss = Sensor::getOrZero(SensorType::VehicleSpeed);

	// Activation is the hardware switch OR the Lua trigger (setRollingLaunchTrigger).
#if EFI_PROD_CODE
	if (isBrainPinValid(cfg->rollingLaunchActivatePin)) {
		rollingLaunchSwitchState = efiReadPin(cfg->rollingLaunchActivatePin, cfg->rollingLaunchActivatePinMode);
	}
#endif // EFI_PROD_CODE
	isRollingLaunchActive = rollingLaunchSwitchState || luaRollingLaunchState;

	// Arm only while moving (min VSS) and above the minimum engagement RPM. The VSS gate is what
	// makes this "rolling" rather than a standstill 2-step.
	const bool armed = isRollingLaunchActive
		&& rpm >= cfg->rollingLaunchMinArmRpm
		&& vss >= cfg->rollingLaunchMinVss;

	// Rising edge: snapshot the operating point and set the hold target, clamped to the ceiling.
	if (armed && !m_prevArmed) {
		capturedRpm = rpm;
		float target = rpm + cfg->rollingLaunchRpmWindow;
		if (target > cfg->rollingLaunchMaxRpm) {
			target = cfg->rollingLaunchMaxRpm;
		}
		rollingLaunchTargetRpm = target;
	}

	if (armed) {
		isRollingLaunchRampActive = false;
		rollingLaunchRampElapsed = 0;

		// Soft spark cut ramps in across [capturedRpm, targetRpm] so the ceiling is approached,
		// not slammed. A non-positive window degenerates to a hard cut at the captured RPM.
		if (rollingLaunchTargetRpm > capturedRpm) {
			rollingLaunchSparkSkipRatio = interpolateClamped(capturedRpm, 0.0f, rollingLaunchTargetRpm, 1.0f, rpm);
		} else {
			rollingLaunchSparkSkipRatio = (rpm >= capturedRpm) ? 1.0f : 0.0f;
		}

		rollingLaunchTimingRetardOut = cfg->rollingLaunchTimingRetard;
		m_rampStartRetard = rollingLaunchTimingRetardOut;
	} else {
		rollingLaunchSparkSkipRatio = 0;

		// Falling edge: start the flat timing ramp-out so the pulled timing returns to zero
		// over rollingLaunchRampOutTime seconds, easing boost back in.
		if (m_prevArmed) {
			m_rampTimer.reset();
			isRollingLaunchRampActive = true;
		}

		if (isRollingLaunchRampActive) {
			const float elapsed = m_rampTimer.getElapsedSeconds();
			rollingLaunchRampElapsed = elapsed;
			if (elapsed >= cfg->rollingLaunchRampOutTime) {
				isRollingLaunchRampActive = false;
				rollingLaunchTimingRetardOut = 0;
			} else {
				rollingLaunchTimingRetardOut = interpolateClamped(0.0f, m_rampStartRetard, cfg->rollingLaunchRampOutTime, 0.0f, elapsed);
			}
		} else {
			rollingLaunchTimingRetardOut = 0;
		}
	}

	isRollingLaunchArmed = armed;
	m_prevArmed = armed;
}

float RollingLaunchControl::getSparkSkipRatio() const {
	return rollingLaunchSparkSkipRatio;
}

float RollingLaunchControl::getFuelCoefficient() const {
	if (!isRollingLaunchArmed) {
		return 1.0f;
	}
	return 1.0f + getCustomPage()->rollingLaunchFuelAdderPercent / 100.0f;
}

float RollingLaunchControl::getTimingRetard() const {
	return rollingLaunchTimingRetardOut;
}

#else // !EFI_ROLLING_LAUNCH

// Rolling Launch Control compiled out (board set -DEFI_ROLLING_LAUNCH=FALSE).
void RollingLaunchControl::onSlowCallback() { }
float RollingLaunchControl::getSparkSkipRatio() const { return 0; }
float RollingLaunchControl::getFuelCoefficient() const { return 1; }
float RollingLaunchControl::getTimingRetard() const { return 0; }

#endif // EFI_ROLLING_LAUNCH
