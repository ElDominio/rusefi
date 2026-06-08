// Deceleration Fuel Cut-off

#include "pch.h"

#include "dfco.h"
#include "closed_loop_fuel.h"

// Simple direct check of physical overrun conditions: TPS, RPM, VSS.
// Does NOT check coastingFuelCutEnabled, CLT, or MAP — those are fuel-cut-only guards.
// No hysteresis: the SM updates at 20Hz which is fast enough to avoid display flicker.
bool DfcoController::isOverrun() const {
	const auto tps = Sensor::get(SensorType::DriverThrottleIntent);
	if (!tps) {
		return false;
	}

	float rpm = Sensor::getOrZero(SensorType::Rpm);
	float vss = Sensor::getOrZero(SensorType::VehicleSpeed);

	return (tps.Value < engineConfiguration->coastingFuelCutTps) &&
	       (rpm > engineConfiguration->coastingFuelCutRpmHigh) &&
	       (vss >= engineConfiguration->coastingFuelCutVssHigh);
}

// Original fuel-cut state logic — unchanged. Checks all guards including enabled flag and CLT.
bool DfcoController::getState() const {
	if (!engineConfiguration->coastingFuelCutEnabled) {
		return false;
	}

	if (checkIfTuningVeNow()) {
	  return false;
	}

	const auto tps = Sensor::get(SensorType::DriverThrottleIntent);
	const auto clt = Sensor::get(SensorType::Clt);
	const auto map = Sensor::get(SensorType::Map);

	// If some sensor is broken, inhibit DFCO
	if (!tps || !clt) {
		return false;
	}

	// MAP sensor is optional, only inhibit if the sensor is present but broken
	bool hasMap = Sensor::hasSensor(SensorType::Map);
	if (hasMap && !map) {
		return false;
	}
	if (engine->engineState.lua.disableDecelerationFuelCutOff) {
	  // Lua might have reasons to disable
		return false;
	}

	float rpm = Sensor::getOrZero(SensorType::Rpm);
	float vss = Sensor::getOrZero(SensorType::VehicleSpeed);

	bool mapActivate = !hasMap || !m_mapHysteresis.test(map.value_or(0), engineConfiguration->coastingFuelCutMap + 1, engineConfiguration->coastingFuelCutMap - 1);
	bool tpsActivate = tps.Value < engineConfiguration->coastingFuelCutTps;
	bool cltActivate = clt.Value > engineConfiguration->coastingFuelCutClt;
	// True if throttle, MAP, and CLT are all acceptable for DFCO to occur
	bool dfcoAllowed = mapActivate && tpsActivate && cltActivate;

	bool rpmActivate = (rpm > engineConfiguration->coastingFuelCutRpmHigh);
	bool rpmDeactivate = (rpm < engineConfiguration->coastingFuelCutRpmLow);

	// greater than or equal so that it works if both config params are set to 0
	bool vssActivate = (vss >= engineConfiguration->coastingFuelCutVssHigh);
	bool vssDeactivate = (vss < engineConfiguration->coastingFuelCutVssLow);

	// RPM is high enough, VSS high enough, and DFCO allowed
	if (dfcoAllowed && rpmActivate && vssActivate) {
		return true;
	}

	// RPM too low, VSS too low, or DFCO not allowed
	if (!dfcoAllowed || rpmDeactivate || vssDeactivate) {
		return false;
	}

	// No conditions hit, no change to state (provides hysteresis)
	return m_isDfco;
}

void DfcoController::update() {
	bool overrun = isOverrun();
	float rpm = Sensor::getOrZero(SensorType::Rpm);

	if (engineConfiguration->popsAndBangsEnabled && overrun) {
		if (!m_wasOverrun) {
			m_overrunTimer.reset();
			m_popsAndBangsState = PopsAndBangsState::Inactive;
		}

		if (m_popsAndBangsState == PopsAndBangsState::Inactive) {
			if (rpm > engineConfiguration->popsAndBangsRpmHigh &&
			    rpm < engineConfiguration->popsAndBangsRpmMax &&
			    m_overrunTimer.hasElapsedSec(engineConfiguration->popsAndBangsDelay)) {
				const auto clt = Sensor::get(SensorType::Clt);
				bool cltOk = clt &&
					clt.Value >= engineConfiguration->popsAndBangsCltMin &&
					clt.Value <= engineConfiguration->popsAndBangsCltMax;
				if (cltOk && !isPopsAndBangsBlocked()) {
					m_popsAndBangsState = PopsAndBangsState::Active;
					m_popsAndBangsTimer.reset();
				} else {
					m_popsAndBangsState = PopsAndBangsState::Expired;
				}
			}
		} else if (m_popsAndBangsState == PopsAndBangsState::Active) {
			if (rpm < engineConfiguration->popsAndBangsRpmLow ||
			    rpm > engineConfiguration->popsAndBangsRpmMax) {
				m_popsAndBangsState = PopsAndBangsState::Expired;
			}

			float duration = engineConfiguration->popsAndBangsDuration;
			if (duration > 0 && m_popsAndBangsTimer.hasElapsedSec(duration)) {
				m_popsAndBangsState = PopsAndBangsState::Expired;
			}

			if (isPopsAndBangsBlocked()) {
				m_popsAndBangsState = PopsAndBangsState::Expired;
			}
		}
	} else {
		m_popsAndBangsState = PopsAndBangsState::Inactive;
	}

	m_wasOverrun = overrun;

	// Run normal DFCO state machine
	bool newState = getState();

	if (newState) {
		m_timeSinceCut.reset();
	} else {
		m_timeSinceNoCut.reset();
	}

	m_isDfco = newState;
}

bool DfcoController::isPopsAndBangsActive() const {
	return m_popsAndBangsState == PopsAndBangsState::Active;
}

bool DfcoController::isPopsAndBangsBlocked() const {
	auto mode = engineConfiguration->popsAndBangsDisableMode;

	bool pinBlocked = false;
	bool gaugeBlocked = false;

#if !EFI_SIMULATOR
	if (mode == POPS_AND_BANGS_DISABLE_MODE_SWITCH_INPUT ||
	    mode == POPS_AND_BANGS_DISABLE_MODE_SWITCH_OR_LUA_GAUGE) {
		const switch_input_pin_e pin = engineConfiguration->popsAndBangsDisablePin;
		const pin_input_mode_e pinMode = engineConfiguration->popsAndBangsDisablePinMode;
		if (isBrainPinValid(pin)) {
			pinBlocked = efiReadPin(pin, pinMode);
		}
	}
#endif

	if (mode == POPS_AND_BANGS_DISABLE_MODE_LUA_GAUGE ||
	    mode == POPS_AND_BANGS_DISABLE_MODE_SWITCH_OR_LUA_GAUGE) {
		SensorType gaugeType = SensorType::LuaGauge1;
		switch (engineConfiguration->popsAndBangsLuaGauge) {
			case LUA_GAUGE_2: gaugeType = SensorType::LuaGauge2; break;
			case LUA_GAUGE_3: gaugeType = SensorType::LuaGauge3; break;
			case LUA_GAUGE_4: gaugeType = SensorType::LuaGauge4; break;
			case LUA_GAUGE_5: gaugeType = SensorType::LuaGauge5; break;
			case LUA_GAUGE_6: gaugeType = SensorType::LuaGauge6; break;
			case LUA_GAUGE_7: gaugeType = SensorType::LuaGauge7; break;
			case LUA_GAUGE_8: gaugeType = SensorType::LuaGauge8; break;
			default: break;
		}
		const auto gaugeResult = Sensor::get(gaugeType);
		if (gaugeResult.Valid) {
			const float value = gaugeResult.Value;
			const float threshold = engineConfiguration->popsAndBangsLuaGaugeValue;
			switch (engineConfiguration->popsAndBangsLuaGaugeMeaning) {
				case LUA_GAUGE_LOWER_BOUND:
					gaugeBlocked = (value >= threshold);
					break;
				case LUA_GAUGE_UPPER_BOUND:
					gaugeBlocked = (value <= threshold);
					break;
			}
		}
	}

	return pinBlocked || gaugeBlocked;
}

bool DfcoController::cutFuel() const {
	if (engineConfiguration->popsAndBangsEnabled &&
	    (m_popsAndBangsState == PopsAndBangsState::Active ||
	     m_popsAndBangsState == PopsAndBangsState::Inactive)) {
		return false;
	}

	float cutDelay = engineConfiguration->dfcoDelay;

	// 0 delay means cut immediately, aka timer has always expired
	bool hasBeenDelay = (cutDelay == 0) || m_timeSinceNoCut.hasElapsedSec(cutDelay);

	return m_isDfco && hasBeenDelay;
}

float DfcoController::getTimeSinceCut() const {
	return m_timeSinceCut.getElapsedSeconds();
}

float DfcoController::getTimingRetard() const {
	float cutTiming = clampF(0, engineConfiguration->dfcoRetardDeg, 30);

	if (m_isDfco) {
		// While cut, always retard timing
		return cutTiming;
	} else {
		float timeSinceCut = m_timeSinceCut.getElapsedSeconds();
		float rampInTime = engineConfiguration->dfcoRetardRampInTime;

		if (timeSinceCut > rampInTime) {
			// Normal operation, no retard
			return 0;
		} else {
			return interpolateClamped(0, cutTiming, 0.5, 0, timeSinceCut);
		}
	}
}
