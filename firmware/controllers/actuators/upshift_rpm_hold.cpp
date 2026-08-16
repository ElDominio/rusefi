#include "pch.h"

#if EFI_ELECTRONIC_THROTTLE_BODY

#include "upshift_rpm_hold.h"
#include "engine_state_machine.h"
#include "custom_page.h"

#if EFI_UPSHIFT_RPM_HOLD

// A non-zero DetectedGear must hold this long before it is trusted as the pre-shift gear.
// Protects against the gear detector reading 0/neutral the instant the clutch disengages.
static constexpr efitimems_t GEAR_STABLE_MS = 200;

// Hold config lives in TS page 6 (page6_s) with PID gains as plain floats, so the Pid is
// bound to this file-static pid_s (single hold instance) rather than to a page field.
static pid_s s_holdPidCfg{};

void UpshiftRpmHold::onConfigurationChange(engine_configuration_s const* /*previousConfig*/) {
	// Bind the PID to the file-static params; constant fields set once here.
	s_holdPidCfg.offset = 0;
	s_holdPidCfg.periodMs = 0;
	s_holdPidCfg.minValue = 0;
	s_holdPidCfg.maxValue = 100;
	m_pid.initPidClass(&s_holdPidCfg);
	m_pidInited = true;
}

bool UpshiftRpmHold::isActive() const {
	auto s = static_cast<UpshiftHoldState>(upshiftHoldState);
	return s == UpshiftHoldState::RampDown
	    || s == UpshiftHoldState::ActivePID
	    || s == UpshiftHoldState::RampUp;
}

void UpshiftRpmHold::setState(UpshiftHoldState newState) {
	upshiftHoldState = static_cast<uint8_t>(newState);
	m_stateTimer.reset();
}

void UpshiftRpmHold::updateGearLatch(efitimems_t /*nowMs*/) {
	size_t gear = static_cast<size_t>(Sensor::getOrZero(SensorType::DetectedGear));

	if (gear != 0 && gear == m_gearCandidate) {
		if (m_gearStableTimer.hasElapsedMs(GEAR_STABLE_MS)) {
			m_lastStableGear = gear;
		}
	} else {
		// Gear changed (or dropped to neutral): restart the stability window.
		m_gearCandidate = gear;
		m_gearStableTimer.reset();
	}
}

float UpshiftRpmHold::computeTargetRpm() const {
	size_t preShiftGear = m_lastStableGear;
	uint8_t totalGears = engineConfiguration->totalGearsCount;

	// Sequential single-gear upshift only: must be in gear 1..N-1 to climb to N+1.
	if (preShiftGear < 1 || preShiftGear >= totalGears) {
		return 0;
	}

	size_t targetGear = preShiftGear + 1;

	float preRatio    = engineConfiguration->gearRatio[preShiftGear - 1];
	float targetRatio = engineConfiguration->gearRatio[targetGear - 1];
	if (preRatio <= 0) {
		return 0;
	}

	// Target purely from ratios and the locked-in RPM — never re-reads (noisy) VSS.
	float targetRpm = m_latchedRpm * (targetRatio / preRatio);

	// A valid upshift always lowers RPM; reject anything that wouldn't.
	if (targetRpm >= m_latchedRpm) {
		return 0;
	}

	return targetRpm;
}

bool UpshiftRpmHold::passesEntryGate(float rpm, float vss, float driverTps) const {
	auto cfg = getCustomPage();
	if (!cfg->upshiftRpmHoldEnabled) {
		return false;
	}
	// Flat shift takes priority: if the driver is holding the flat-shift button, don't enter RPM hold.
	if (engine->shiftTorqueReductionController.isCuttingTorque()) {
		return false;
	}
	if (vss < cfg->upshiftRpmHoldMinVss) {
		return false;
	}
	if (rpm < cfg->upshiftRpmHoldMinRpm) {
		return false;
	}
	// Over-rev safety: don't start a hold if the engine is already too high at the shift
	// (e.g. a missed/late shift near the limiter) -- unlike downshift's MaxRpm, which gates
	// the computed *target*, upshift's target is always lower than the latched RPM, so the
	// gate has to be on the shift RPM itself.
	if (rpm > cfg->upshiftRpmHoldMaxRpm) {
		return false;
	}
	if (driverTps > cfg->upshiftRpmHoldDriverTpsThreshold) {
		return false;
	}
	return true;
}

bool UpshiftRpmHold::shouldTerminate(float driverTps, bool upshifting) const {
	auto cfg = getCustomPage();
	// Clutch re-engaging
	if (!upshifting) {
		return true;
	}
	// Safety timeout
	if (m_holdTimer.hasElapsedMs(cfg->upshiftRpmHoldMaxTimeMs)) {
		return true;
	}
	// Driver took over the pedal
	if (driverTps > cfg->upshiftRpmHoldDriverTpsThreshold) {
		return true;
	}
	return false;
}

float UpshiftRpmHold::runPid(float rpm, float dt) {
	auto cfg = getCustomPage();
	// Feed-forward seed: throttle needed at neutral to sustain the target RPM. The PID then
	// only corrects the residual error. With an unconfigured (all-zero) curve ff is 0.
	float ff = interpolate2d((float)upshiftHoldTargetRpm,
		cfg->tpsRpmFeedForwardBins, cfg->tpsRpmFeedForwardValues);
	// Error normalized to per-100-RPM so gains live in the same range as the idle PID.
	float out = ff + m_pid.getOutput(upshiftHoldTargetRpm / 100.0f, rpm / 100.0f, dt);
	out = clampF(0, out, cfg->upshiftRpmHoldMaxTpsLimit);
	upshiftHoldPidOutput = out;
	return out;
}

void UpshiftRpmHold::onFastCallback() {
	if (!m_pidInited) {
		onConfigurationChange(nullptr);
	}

	auto cfg = getCustomPage();

	// Page-5 burns don't fire onConfigurationChange, so refresh the live gains each tick.
	s_holdPidCfg.pFactor = cfg->upshiftRpmHoldKp;
	s_holdPidCfg.iFactor = cfg->upshiftRpmHoldKi;
	s_holdPidCfg.dFactor = cfg->upshiftRpmHoldKd;

	efitimems_t nowMs = getTimeNowMs();
	updateGearLatch(nowMs);

	float rpm       = Sensor::getOrZero(SensorType::Rpm);
	float vss       = Sensor::getOrZero(SensorType::VehicleSpeed);
	float driverTps = Sensor::getOrZero(SensorType::DriverThrottleIntent);

	bool upshifting = engine->module<EngineStateMachine>().unmock().engineSmIsUpshifting;
	bool upshiftRose = upshifting && !m_prevUpshift;
	m_prevUpshift = upshifting;

	const float dt = FAST_CALLBACK_PERIOD_MS / 1000.0f;
	float phaseThrottle = 0;

	auto state = static_cast<UpshiftHoldState>(upshiftHoldState);

	switch (state) {
		case UpshiftHoldState::Idle: {
			if (upshiftRose && passesEntryGate(rpm, vss, driverTps)) {
				m_latchedRpm = rpm;
				float targetRpm = computeTargetRpm();
				if (targetRpm > 0) {
					upshiftHoldPreShiftGear = static_cast<uint8_t>(m_lastStableGear);
					upshiftHoldTargetGear   = static_cast<uint8_t>(m_lastStableGear + 1);
					upshiftHoldTargetRpm    = static_cast<uint16_t>(targetRpm);
					m_rampStartTps = Sensor::getOrZero(SensorType::Tps1);
					m_pid.reset();
					m_holdTimer.reset();
					setState(UpshiftHoldState::RampDown);
				}
			}
			break;
		}

		case UpshiftHoldState::RampDown: {
			float pidOut = runPid(rpm, dt);
			if (shouldTerminate(driverTps, upshifting)) {
				m_rampUpStart = upshiftHoldThrottleRequest;
				setState(UpshiftHoldState::RampUp);
				phaseThrottle = m_rampUpStart;
			} else {
				float rampMs = cfg->upshiftRpmHoldRampDownMs;
				float frac = (rampMs > 0) ? clampF(0, m_stateTimer.getElapsedSeconds() * 1000.0f / rampMs, 1) : 1;
				phaseThrottle = interpolateClamped(0, m_rampStartTps, 1, pidOut, frac);
				if (frac >= 1) {
					setState(UpshiftHoldState::ActivePID);
				}
			}
			break;
		}

		case UpshiftHoldState::ActivePID: {
			// Hold the target RPM until the clutch re-engages, the driver takes over, or the
			// safety timeout elapses. We deliberately do NOT exit just because RPM has fallen
			// to the target -- holding it there is the point of the hold.
			phaseThrottle = runPid(rpm, dt);
			if (shouldTerminate(driverTps, upshifting)) {
				m_rampUpStart = upshiftHoldThrottleRequest;
				setState(UpshiftHoldState::RampUp);
				phaseThrottle = m_rampUpStart;
			}
			break;
		}

		case UpshiftHoldState::RampUp: {
			float rampMs = cfg->upshiftRpmHoldRampUpMs;
			float frac = (rampMs > 0) ? clampF(0, m_stateTimer.getElapsedSeconds() * 1000.0f / rampMs, 1) : 1;
			// Hand back to the driver's pedal target.
			phaseThrottle = interpolateClamped(0, m_rampUpStart, 1, driverTps, frac);
			if (frac >= 1) {
				setState(UpshiftHoldState::Lockout);
				phaseThrottle = 0;
			}
			break;
		}

		case UpshiftHoldState::Lockout: {
			phaseThrottle = 0;
			if (m_stateTimer.hasElapsedMs(cfg->upshiftRpmHoldLockoutTimeMs)) {
				setState(UpshiftHoldState::Idle);
			}
			break;
		}
	}

	upshiftHoldThrottleRequest = clampF(0, phaseThrottle, cfg->upshiftRpmHoldMaxTpsLimit);
	upshiftHoldActive = isActive();
}

#else // !EFI_UPSHIFT_RPM_HOLD

// Upshift RPM Hold compiled out (board set -DEFI_UPSHIFT_RPM_HOLD=FALSE).
// Module stays in the tuple; it does nothing and never requests throttle.
void UpshiftRpmHold::onFastCallback() { }
void UpshiftRpmHold::onConfigurationChange(engine_configuration_s const* /*previousConfig*/) { }
bool UpshiftRpmHold::isActive() const { return false; }

#endif // EFI_UPSHIFT_RPM_HOLD

#endif // EFI_ELECTRONIC_THROTTLE_BODY
