#include "pch.h"
#include "custom_page.h"
#include "check_engine_light.h"
#include "malfunction_central.h"
#include "misfire_detection.h"

static constexpr float CelDebounceSeconds = 1.0f;

void CheckEngineLight::setDefaultConfiguration() {
	config->cel_battery_min_v = 6.0f;
	config->cel_battery_max_v = 18.0f;

	config->cel_map_min_v = 0.2f;
	config->cel_map_max_v = 4.8f;
	config->cel_iat_min_v = 0.2f;
	config->cel_iat_max_v = 4.8f;
	config->cel_tps_min_v = 0.2f;
	config->cel_tps_max_v = 4.8f;
}

void CheckEngineLight::initNoConfiguration() {
	m_tpsCircuitLowGate.reset();
	m_tpsCircuitHighGate.reset();
	m_tpsIntermittentGate.reset();
	for (auto& t : m_tps1FlipTimers) {
		t.reset();
	}
	m_tps1FlipIndex = 0;
	m_wasTps1Faulted = false;
}

void CheckEngineLight::updateRange(RangeState& state, bool available, float value, float minimum, float maximum,
		ObdCode lowCode, ObdCode highCode) {
	ObdCode candidate = ObdCode::None;

	if (available && minimum < maximum) {
		if (value < minimum) {
			candidate = lowCode;
		} else if (value > maximum) {
			candidate = highCode;
		}
	}

	if (candidate == state.activeCode) {
		state.pendingCode = state.activeCode;
		state.pendingTimer.init();

		if (candidate != ObdCode::None) {
			addError(candidate);
		}
		return;
	}

	if (candidate != state.pendingCode) {
		state.pendingCode = candidate;
		state.pendingTimer.reset();
		return;
	}

	if (!state.pendingTimer.hasElapsedSec(CelDebounceSeconds)) {
		return;
	}

	if (state.activeCode != ObdCode::None) {
		removeError(state.activeCode);
	}

	state.activeCode = candidate;
	if (state.activeCode != ObdCode::None) {
		addError(state.activeCode);
	}
}

void CheckEngineLight::clearCurrentFaults() {
	RangeState* states[] = { &m_battery, &m_map, &m_iat, &m_tps };

	for (auto state : states) {
		if (state->activeCode != ObdCode::None) {
			removeError(state->activeCode);
		}

		*state = {};
	}
}

static bool getRawVoltage(SensorType type, float& voltage) {
	const Sensor* sensor = Sensor::getSensorOfType(type);
	if (!sensor || !sensor->hasSensor() || !sensor->hasRaw()) {
		return false;
	}

	auto result = sensor->get();
	if (!result.Valid && result.Code != UnexpectedCode::Low && result.Code != UnexpectedCode::High) {
		return false;
	}

	voltage = sensor->getRaw();
	return true;
}

void CheckEngineLight::updateCheckEngineTriggering() {
	auto cfg = getCustomPage();
	float debounceSec = cfg->celDebounceTimeSec;

	// TPS Circuit Low/High (P0122/P0123): reuses the same TPS1 signal-out-of-range detection as
	// sensor_checker.cpp's warning(), just debounced through celDebounceTimeSec on both ends.
	bool tps1Low = false;
	bool tps1High = false;
	bool tps1Faulted = false;
	if (Sensor::hasSensor(SensorType::Tps1)) {
		auto tps = Sensor::get(SensorType::Tps1);
		if (!tps) {
			tps1Faulted = true;
			tps1Low = tps.Code == UnexpectedCode::Low;
			tps1High = tps.Code == UnexpectedCode::High;
		}
	}

	if (cfg->tpsCircuitCelEnable) {
		isTpsCircuitLow = m_tpsCircuitLowGate.update(tps1Low, debounceSec);
		isTpsCircuitHigh = m_tpsCircuitHighGate.update(tps1High, debounceSec);
	} else {
		m_tpsCircuitLowGate.reset();
		m_tpsCircuitHighGate.reset();
		isTpsCircuitLow = false;
		isTpsCircuitHigh = false;
	}

	// TPS Circuit Intermittent (P0124): count TPS1 ok<->fault transitions in a ring buffer, then
	// trip once enough of them land within the last celDebounceTimeSec (same shared window).
	if (tps1Faulted != m_wasTps1Faulted) {
		m_wasTps1Faulted = tps1Faulted;
		m_tps1FlipTimers[m_tps1FlipIndex].reset();
		m_tps1FlipIndex = (m_tps1FlipIndex + 1) % TPS_FLIP_HISTORY_SIZE;
	}

	uint8_t flipsInWindow = 0;
	for (auto& t : m_tps1FlipTimers) {
		if (!t.hasElapsedSec(debounceSec)) {
			flipsInWindow++;
		}
	}

	if (cfg->tpsIntermittentCelEnable) {
		isTpsIntermittent = m_tpsIntermittentGate.update(flipsInWindow >= cfg->tpsIntermittentFlipCount, debounceSec);
	} else {
		m_tpsIntermittentGate.reset();
		isTpsIntermittent = false;
	}

	// Points contributed by every check on this module, so a combination of otherwise-minor
	// faults can escalate the CEL to flashing even if no single one crosses its own threshold.
	// Misfire Detection keeps its own addError() latch (misfire_detection.cpp raises
	// OBD_Random_Misfire directly once latched) but also contributes a fixed point here. The
	// voltage-range checks above (battery/MAP/IAT/TPS) raise/clear their DTCs immediately and
	// unconditionally via updateRange() -- that behavior is unchanged -- but each also
	// contributes a point here while its DTC is active.
	uint8_t points = 0;
	if (isTpsCircuitLow || isTpsCircuitHigh) {
		points += 1;
	}
	if (isTpsIntermittent) {
		points += 1;
	}
	if (engine->module<MisfireController>().unmock().misfireLatched) {
		points += 1;
	}
	if (m_battery.activeCode != ObdCode::None) {
		points += 1;
	}
	if (m_map.activeCode != ObdCode::None) {
		points += 1;
	}
	if (m_iat.activeCode != ObdCode::None) {
		points += 1;
	}
	if (m_tps.activeCode != ObdCode::None) {
		points += 1;
	}
	celPointsTotal = points;

	// celPointsThreshold (lower) raises the DTC and lights the CEL solid -- the standard,
	// less-urgent fault path. celBlinkPointsThreshold (higher) escalates an already-active DTC
	// to a flashing CEL, matching OBD-II convention where a flashing MIL signals a more critical,
	// actively-damaging condition than a steady one. The DTC stays raised while blinking.
	bool dtcActive = cfg->celPointsThreshold > 0 && points >= cfg->celPointsThreshold;
	isCelBlinking = dtcActive && cfg->celBlinkPointsThreshold > 0 && points >= cfg->celBlinkPointsThreshold;

	// Range/Performance, not the Low/High codes -- those belong to CheckEngineLight's own raw-
	// voltage range check on the Tps1Primary ADC channel above, which must stay free to raise/
	// clear independently of this points-gated check.
	if (dtcActive && (isTpsCircuitLow || isTpsCircuitHigh)) {
		addError(ObdCode::OBD_TPS1_Range_Performance);
	} else {
		removeError(ObdCode::OBD_TPS1_Range_Performance);
	}

	if (dtcActive && isTpsIntermittent) {
		addError(ObdCode::OBD_TPS1_Intermittent);
	} else {
		removeError(ObdCode::OBD_TPS1_Intermittent);
	}
}

void CheckEngineLight::onSlowCallback() {
	if (!engine->module<SensorChecker>()->analogSensorsShouldWork()) {
		clearCurrentFaults();
	} else {
		auto battery = Sensor::get(SensorType::BatteryVoltage);
		updateRange(m_battery, battery.Valid, battery.Value,
			config->cel_battery_min_v, config->cel_battery_max_v,
			ObdCode::OBD_System_Voltage_Low, ObdCode::OBD_System_Voltage_Malfunction);

		float voltage = 0;
		bool available = getRawVoltage(SensorType::MapSlow, voltage);
		updateRange(m_map, available, voltage, config->cel_map_min_v, config->cel_map_max_v,
			ObdCode::OBD_Map_Low, ObdCode::OBD_Map_High);

		available = getRawVoltage(SensorType::Iat, voltage);
		updateRange(m_iat, available, voltage, config->cel_iat_min_v, config->cel_iat_max_v,
			ObdCode::OBD_Iat_Low, ObdCode::OBD_Iat_High);

		available = getRawVoltage(SensorType::Tps1Primary, voltage);
		updateRange(m_tps, available, voltage, config->cel_tps_min_v, config->cel_tps_max_v,
			ObdCode::OBD_TPS1_Primary_Low, ObdCode::OBD_TPS1_Primary_High);
	}

	updateCheckEngineTriggering();
}

bool isCelBlinkingActive() {
	return engine->module<CheckEngineLight>().unmock().isCelBlinking;
}
