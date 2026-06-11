#include "pch.h"

#if EFI_ELECTRONIC_THROTTLE_BODY

#include "downshift_blipper.h"
#include "engine_state_machine.h"
#include "custom_page.h"

#if EFI_DOWNSHIFT_BLIPPER

// A non-zero DetectedGear must hold this long before it is trusted as the pre-shift gear.
// Protects against the gear detector reading 0/neutral the instant the clutch disengages.
static constexpr efitimems_t GEAR_STABLE_MS = 200;

// Blipper config lives in TS page 5 (page5_s) with PID gains as plain floats, so the Pid
// is bound to this file-static pid_s (single blipper instance) rather than to a page field.
static pid_s s_blipPidCfg{};

static SensorType mapLuaGauge(uint8_t index) {
	switch (index) {
		case 1:  return SensorType::LuaGauge2;
		case 2:  return SensorType::LuaGauge3;
		case 3:  return SensorType::LuaGauge4;
		case 4:  return SensorType::LuaGauge5;
		case 5:  return SensorType::LuaGauge6;
		case 6:  return SensorType::LuaGauge7;
		case 7:  return SensorType::LuaGauge8;
		default: return SensorType::LuaGauge1;
	}
}

void DownshiftBlipper::onConfigurationChange(engine_configuration_s const* /*previousConfig*/) {
	// Bind the PID to the file-static params; constant fields set once here.
	s_blipPidCfg.offset = 0;
	s_blipPidCfg.periodMs = 0;
	s_blipPidCfg.minValue = 0;
	s_blipPidCfg.maxValue = 100;
	m_pid.initPidClass(&s_blipPidCfg);
	m_pidInited = true;
}

bool DownshiftBlipper::isActive() const {
	auto s = static_cast<DownshiftBlipState>(downshiftBlipState);
	return s == DownshiftBlipState::RampOpen
	    || s == DownshiftBlipState::ActivePID
	    || s == DownshiftBlipState::RampClose;
}

void DownshiftBlipper::setState(DownshiftBlipState newState) {
	downshiftBlipState = static_cast<uint8_t>(newState);
	m_stateTimer.reset();
}

void DownshiftBlipper::updateGearLatch(efitimems_t /*nowMs*/) {
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

float DownshiftBlipper::computeTargetRpm() const {
	auto cfg = getCustomPage();
	size_t preShiftGear = m_lastStableGear;
	uint8_t totalGears = engineConfiguration->totalGearsCount;

	// Sequential single-gear downshift only: must be in gear 2..N to drop to N-1.
	if (preShiftGear <= 1 || preShiftGear > totalGears) {
		return 0;
	}

	size_t targetGear = preShiftGear - 1;

	float preRatio    = engineConfiguration->gearRatio[preShiftGear - 1];
	float targetRatio = engineConfiguration->gearRatio[targetGear - 1];
	if (preRatio <= 0) {
		return 0;
	}

	// Target purely from ratios and the locked-in RPM — never re-reads (noisy) VSS.
	float targetRpm = m_latchedRpm * (targetRatio / preRatio);

	// Over-rev guards: never target above the configured max or the rev limiter.
	if (targetRpm >= cfg->downshiftBlipperMaxRpm) {
		return 0;
	}
	uint16_t revLimit = engineConfiguration->rpmHardLimit;
	if (revLimit != 0 && targetRpm >= revLimit) {
		return 0;
	}

	// A valid downshift always raises RPM; reject anything that wouldn't.
	if (targetRpm <= m_latchedRpm) {
		return 0;
	}

	return targetRpm;
}

bool DownshiftBlipper::passesEntryGate(float rpm, float vss, float driverTps) const {
	auto cfg = getCustomPage();
	if (!cfg->downshiftBlipperEnabled) {
		return false;
	}
	if (vss < cfg->downshiftBlipperMinVss) {
		return false;
	}
	if (rpm < cfg->downshiftBlipperMinRpm) {
		return false;
	}
	if (driverTps > cfg->downshiftBlipperDriverTpsThreshold) {
		return false;
	}
	if (cfg->downshiftBlipperRequireBrake && engine->engineState.brakePedalState == 0) {
		return false;
	}
	return true;
}

bool DownshiftBlipper::shouldTerminate(float rpm, float driverTps, bool downshifting) const {
	auto cfg = getCustomPage();
	// RPM matched (within the early-cut offset)
	if (rpm >= (downshiftBlipTargetRpm - cfg->downshiftBlipRpmOffset)) {
		return true;
	}
	// Clutch re-engaging
	if (!downshifting) {
		return true;
	}
	// Safety timeout
	if (m_blipTimer.hasElapsedMs(cfg->downshiftBlipperMaxTimeMs)) {
		return true;
	}
	// Driver took over the pedal
	if (driverTps > cfg->downshiftBlipperDriverTpsThreshold) {
		return true;
	}
	return false;
}

float DownshiftBlipper::runPid(float rpm, float dt) {
	// Error normalized to per-100-RPM so gains live in the same range as the idle PID.
	float out = m_pid.getOutput(downshiftBlipTargetRpm / 100.0f, rpm / 100.0f, dt);
	out = clampF(0, out, getCustomPage()->downshiftBlipperMaxTpsLimit);
	downshiftBlipPidOutput = out;
	return out;
}

float DownshiftBlipper::getLuaMultiplier() const {
	auto cfg = getCustomPage();
	if (!cfg->downshiftBlipperUseLuaGauge) {
		return 1.0f;
	}
	SensorType gauge = mapLuaGauge(cfg->downshiftBlipperLuaGauge);
	float value = Sensor::getOrZero(gauge);
	float mult = interpolate2d(value,
		cfg->downshiftBlipperLuaMultBins,
		cfg->downshiftBlipperLuaMultValues);
	return clampF(0, mult, 1.0f);
}

void DownshiftBlipper::onFastCallback() {
	if (!m_pidInited) {
		onConfigurationChange(nullptr);
	}

	auto cfg = getCustomPage();

	// Page-5 burns don't fire onConfigurationChange, so refresh the live gains each tick.
	s_blipPidCfg.pFactor = cfg->downshiftBlipperKp;
	s_blipPidCfg.iFactor = cfg->downshiftBlipperKi;
	s_blipPidCfg.dFactor = cfg->downshiftBlipperKd;

	efitimems_t nowMs = getTimeNowMs();
	updateGearLatch(nowMs);

	float rpm       = Sensor::getOrZero(SensorType::Rpm);
	float vss       = Sensor::getOrZero(SensorType::VehicleSpeed);
	float driverTps = Sensor::getOrZero(SensorType::DriverThrottleIntent);

	bool downshifting = engine->module<EngineStateMachine>().unmock().engineSmIsDownshifting;
	bool downshiftRose = downshifting && !m_prevDownshift;
	m_prevDownshift = downshifting;

	const float dt = FAST_CALLBACK_PERIOD_MS / 1000.0f;
	float phaseThrottle = 0;

	auto state = static_cast<DownshiftBlipState>(downshiftBlipState);

	switch (state) {
		case DownshiftBlipState::Idle: {
			if (downshiftRose && passesEntryGate(rpm, vss, driverTps)) {
				m_latchedRpm = rpm;
				float targetRpm = computeTargetRpm();
				if (targetRpm > 0) {
					downshiftBlipPreShiftGear = static_cast<uint8_t>(m_lastStableGear);
					downshiftBlipTargetGear   = static_cast<uint8_t>(m_lastStableGear - 1);
					downshiftBlipTargetRpm    = static_cast<uint16_t>(targetRpm);
					m_rampStartTps = Sensor::getOrZero(SensorType::Tps1);
					m_pid.reset();
					m_blipTimer.reset();
					setState(DownshiftBlipState::RampOpen);
				}
			}
			break;
		}

		case DownshiftBlipState::RampOpen: {
			float pidOut = runPid(rpm, dt);
			if (shouldTerminate(rpm, driverTps, downshifting)) {
				m_rampCloseStart = downshiftBlipThrottleRequest;
				setState(DownshiftBlipState::RampClose);
				phaseThrottle = m_rampCloseStart;
			} else {
				float rampMs = cfg->downshiftBlipRampOpenMs;
				float frac = (rampMs > 0) ? clampF(0, m_stateTimer.getElapsedSeconds() * 1000.0f / rampMs, 1) : 1;
				phaseThrottle = interpolateClamped(0, m_rampStartTps, 1, pidOut, frac);
				if (frac >= 1) {
					setState(DownshiftBlipState::ActivePID);
				}
			}
			break;
		}

		case DownshiftBlipState::ActivePID: {
			phaseThrottle = runPid(rpm, dt);
			if (shouldTerminate(rpm, driverTps, downshifting)) {
				m_rampCloseStart = downshiftBlipThrottleRequest;
				setState(DownshiftBlipState::RampClose);
				phaseThrottle = m_rampCloseStart;
			}
			break;
		}

		case DownshiftBlipState::RampClose: {
			float rampMs = cfg->downshiftBlipRampCloseMs;
			float frac = (rampMs > 0) ? clampF(0, m_stateTimer.getElapsedSeconds() * 1000.0f / rampMs, 1) : 1;
			// Hand back to the driver's pedal target.
			phaseThrottle = interpolateClamped(0, m_rampCloseStart, 1, driverTps, frac);
			if (frac >= 1) {
				setState(DownshiftBlipState::Lockout);
				phaseThrottle = 0;
			}
			break;
		}

		case DownshiftBlipState::Lockout: {
			phaseThrottle = 0;
			if (m_stateTimer.hasElapsedMs(cfg->downshiftBlipperLockoutTimeMs)) {
				setState(DownshiftBlipState::Idle);
			}
			break;
		}
	}

	float luaMult = getLuaMultiplier();
	downshiftBlipLuaMult = luaMult;

	downshiftBlipThrottleRequest = clampF(0, phaseThrottle * luaMult,
		cfg->downshiftBlipperMaxTpsLimit);
	downshiftBlipActive = isActive();
}

#else // !EFI_DOWNSHIFT_BLIPPER

// Downshift Blipper compiled out (board set -DEFI_DOWNSHIFT_BLIPPER=FALSE).
// Module stays in the tuple; it does nothing and never requests throttle.
void DownshiftBlipper::onFastCallback() { }
void DownshiftBlipper::onConfigurationChange(engine_configuration_s const* /*previousConfig*/) { }
bool DownshiftBlipper::isActive() const { return false; }

#endif // EFI_DOWNSHIFT_BLIPPER

#endif // EFI_ELECTRONIC_THROTTLE_BODY
