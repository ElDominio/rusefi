#include "pch.h"
#include "custom_page.h"
#include "check_engine_light.h"
#include "malfunction_central.h"

#if EFI_CHECK_ENGINE_TRIGGERING

void CheckEngineTriggering::initNoConfiguration() {
	m_tpsCircuitLowGate.reset();
	m_tpsCircuitHighGate.reset();
	m_tpsIntermittentGate.reset();
	for (auto& t : m_tps1FlipTimers) {
		t.reset();
	}
	m_tps1FlipIndex = 0;
	m_wasTps1Faulted = false;
}

void CheckEngineTriggering::onSlowCallback() {
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

	uint8_t points = 0;
	if (isTpsCircuitLow || isTpsCircuitHigh) {
		points += 1;
	}
	if (isTpsIntermittent) {
		points += 1;
	}
	celPointsTotal = points;

	// celPointsThreshold (lower) raises the DTC and lights the CEL solid -- the standard,
	// less-urgent fault path. celBlinkPointsThreshold (higher) escalates an already-active DTC
	// to a flashing CEL, matching OBD-II convention where a flashing MIL signals a more critical,
	// actively-damaging condition than a steady one. The DTC stays raised while blinking.
	bool dtcActive = cfg->celPointsThreshold > 0 && points >= cfg->celPointsThreshold;
	isCelBlinking = dtcActive && cfg->celBlinkPointsThreshold > 0 && points >= cfg->celBlinkPointsThreshold;

	if (dtcActive && isTpsCircuitLow) {
		addError(ObdCode::OBD_TPS1_Primary_Low);
	} else {
		removeError(ObdCode::OBD_TPS1_Primary_Low);
	}

	if (dtcActive && isTpsCircuitHigh) {
		addError(ObdCode::OBD_TPS1_Primary_High);
	} else {
		removeError(ObdCode::OBD_TPS1_Primary_High);
	}

	if (dtcActive && isTpsIntermittent) {
		addError(ObdCode::OBD_TPS1_Intermittent);
	} else {
		removeError(ObdCode::OBD_TPS1_Intermittent);
	}
}

bool isCelBlinkingActive() {
	return engine->module<CheckEngineTriggering>().unmock().isCelBlinking;
}

#else // !EFI_CHECK_ENGINE_TRIGGERING

void CheckEngineTriggering::onSlowCallback() { }

bool isCelBlinkingActive() {
	return false;
}

#endif // EFI_CHECK_ENGINE_TRIGGERING
