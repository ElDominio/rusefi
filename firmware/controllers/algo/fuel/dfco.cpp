// Deceleration Fuel Cut-off

#include "pch.h"

#include "dfco.h"
#include "closed_loop_fuel.h"
#include "engine_state_machine.h"

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

// Shared guards: must pass regardless of fuel-cut mode.
bool DfcoController::commonGuards() const {
	if (!engineConfiguration->coastingFuelCutEnabled) {
		return false;
	}
	if (checkIfTuningVeNow()) {
		return false;
	}
	if (engine->engineState.lua.disableDecelerationFuelCutOff) {
		return false;
	}

	const auto clt = Sensor::get(SensorType::Clt);
	if (!clt || clt.Value <= engineConfiguration->coastingFuelCutClt) {
		return false;
	}

	bool hasMap = Sensor::hasSensor(SensorType::Map);
	if (hasMap) {
		const auto map = Sensor::get(SensorType::Map);
		if (!map) {
			return false;
		}
		if (m_mapHysteresis.test(map.value_or(0), engineConfiguration->coastingFuelCutMap + 1, engineConfiguration->coastingFuelCutMap - 1)) {
			return false;
		}
	}

	return true;
}

// Returns true when the traditional overrun conditions (TPS/RPM/VSS/gear) are met.
bool DfcoController::overrunActive() const {
	const auto tps = Sensor::get(SensorType::DriverThrottleIntent);
	if (!tps) {
		return false;
	}

	if (engineConfiguration->coastingFuelCutRequiresGear && Sensor::getOrZero(SensorType::DetectedGear) <= 0) {
		return false;
	}

	float rpm = Sensor::getOrZero(SensorType::Rpm);
	float vss = Sensor::getOrZero(SensorType::VehicleSpeed);

	bool tpsActivate   = tps.Value < engineConfiguration->coastingFuelCutTps;
	bool rpmActivate   = (rpm > engineConfiguration->coastingFuelCutRpmHigh);
	bool rpmDeactivate = (rpm < engineConfiguration->coastingFuelCutRpmLow);
	bool vssActivate   = (vss >= engineConfiguration->coastingFuelCutVssHigh);
	bool vssDeactivate = (vss < engineConfiguration->coastingFuelCutVssLow);

	if (tpsActivate && rpmActivate && vssActivate) {
		return true;
	}
	if (rpmDeactivate || vssDeactivate || !tpsActivate) {
		return false;
	}

	return m_isDfco;
}

bool DfcoController::getState(const EngineStateMachine& sm) const {
	if (!commonGuards()) {
		return false;
	}

	auto mode = engineConfiguration->dfcoFuelCutMode;

	bool wantsOverrun = (mode == dfco_fuel_cut_mode_e::Overrun || mode == dfco_fuel_cut_mode_e::Both);
	bool wantsDecel   = (mode == dfco_fuel_cut_mode_e::Decel   || mode == dfco_fuel_cut_mode_e::Both);

	if (wantsOverrun && overrunActive()) {
		return true;
	}

	// Decel mode: cut fuel when the SM reports Decelerating (VSS + clutch-out confirmed),
	// or — when VSS is absent — fall back to a raw RPM rate check so people without VSS
	// can still get fuel cut on a fast RPM drop.
	if (wantsDecel) {
		if (sm.engineSmIsDecelerating) {
			return true;
		}
		if (!Sensor::hasSensor(SensorType::VehicleSpeed)) {
			float decelThr = (float)getCustomPage()->smDecelRateThreshold;
			if (decelThr > 0 && sm.getLastRpmRate() < -decelThr) {
				return true;
			}
		}
	}

	return false;
}

void DfcoController::update() {
	auto& sm = engine->module<EngineStateMachine>().unmock();
	bool newState = getState(sm);

	// If P&B is active, don't cut fuel — we want rich, unburnt mixture for pops.
	// Likewise, don't cut fuel while the SM has a shift in progress (Upshifting/Downshifting) —
	// an abrupt fuel cut mid-shift fights the rev-match / RPM-hold modules and feels harsh.
	if (sm.engineSmIsPopsAndBangs || sm.engineSmIsUpshifting || sm.engineSmIsDownshifting) {
		newState = false;
	}


	if (newState) {
		m_timeSinceCut.reset();
	} else {
		m_timeSinceNoCut.reset();
	}

	m_isDfco = newState;
	dfcoCutActive = cutFuel();
}

bool DfcoController::cutFuel() const {
	float cutDelay = engineConfiguration->dfcoDelay;
	bool hasBeenDelay = (cutDelay == 0) || m_timeSinceNoCut.hasElapsedSec(cutDelay);
	return m_isDfco && hasBeenDelay;
}

float DfcoController::getTimeSinceCut() const {
	return m_timeSinceCut.getElapsedSeconds();
}

float DfcoController::getTimingRetard() const {
	float cutTiming = clampF(0, engineConfiguration->dfcoRetardDeg, 30);

	if (m_isDfco) {
		return cutTiming;
	} else {
		float timeSinceCut = m_timeSinceCut.getElapsedSeconds();
		float rampInTime = engineConfiguration->dfcoRetardRampInTime;

		if (timeSinceCut > rampInTime) {
			return 0;
		} else {
			return interpolateClamped(0, cutTiming, 0.5, 0, timeSinceCut);
		}
	}
}
