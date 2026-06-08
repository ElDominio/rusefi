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

	if (!tps || !clt) {
		return false;
	}

	bool hasMap = Sensor::hasSensor(SensorType::Map);
	if (hasMap && !map) {
		return false;
	}
	if (engine->engineState.lua.disableDecelerationFuelCutOff) {
		return false;
	}

	float rpm = Sensor::getOrZero(SensorType::Rpm);
	float vss = Sensor::getOrZero(SensorType::VehicleSpeed);

	bool mapActivate = !hasMap || !m_mapHysteresis.test(map.value_or(0), engineConfiguration->coastingFuelCutMap + 1, engineConfiguration->coastingFuelCutMap - 1);
	bool tpsActivate = tps.Value < engineConfiguration->coastingFuelCutTps;
	bool cltActivate = clt.Value > engineConfiguration->coastingFuelCutClt;
	bool dfcoAllowed = mapActivate && tpsActivate && cltActivate;

	bool rpmActivate   = (rpm > engineConfiguration->coastingFuelCutRpmHigh);
	bool rpmDeactivate = (rpm < engineConfiguration->coastingFuelCutRpmLow);
	bool vssActivate   = (vss >= engineConfiguration->coastingFuelCutVssHigh);
	bool vssDeactivate = (vss < engineConfiguration->coastingFuelCutVssLow);

	if (dfcoAllowed && rpmActivate && vssActivate) {
		return true;
	}
	if (!dfcoAllowed || rpmDeactivate || vssDeactivate) {
		return false;
	}

	return m_isDfco;
}

void DfcoController::update() {
	bool newState = getState();

	// If P&B is active, don't cut fuel — we want rich, unburnt mixture for pops
	if (engine->module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs) {
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
