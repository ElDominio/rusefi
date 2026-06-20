#include "pch.h"
#include "custom_page.h"
#include "check_engine_light.h"
#include "malfunction_central.h"

#if EFI_CHECK_ENGINE_TRIGGERING

void CheckEngineTriggering::initNoConfiguration() {
	m_tpsHighTimer.reset();
	m_tpsLowTimer.reset();
	m_wasStuckHighGate = false;
	m_wasStuckLowGate = false;
}

void CheckEngineTriggering::onSlowCallback() {
	auto cfg = getCustomPage();

	if (cfg->tpsStuckCelEnable) {
		auto tps = Sensor::get(SensorType::Tps1);
		auto pedal = Sensor::get(SensorType::AcceleratorPedal);

		if (tps && pedal) {
			// "Stuck high": throttle reads open while the driver pedal is released.
			bool stuckHighGate = tps.Value >= cfg->tpsStuckHighThreshold && pedal.Value <= cfg->tpsStuckLowThreshold;
			// "Stuck low": throttle reads closed while the driver pedal is pressed.
			bool stuckLowGate = tps.Value <= cfg->tpsStuckLowThreshold && pedal.Value >= cfg->tpsStuckHighThreshold;

			// One timer, run in both directions: it resets whenever the gate flips, and the flag
			// only follows the gate once tpsStuckCelTimeoutSec has passed since the last flip --
			// debouncing trip and untrip with the same timer and the same timeout. Untripping is
			// additionally gated by tpsStuckCelAutoClear: if disabled, once tripped the flag is
			// never set back to false here (latches until power cycle).
			if (stuckHighGate != m_wasStuckHighGate) {
				m_tpsHighTimer.reset();
				m_wasStuckHighGate = stuckHighGate;
			}
			if (m_tpsHighTimer.hasElapsedSec(cfg->tpsStuckCelTimeoutSec)) {
				if (stuckHighGate) {
					isTpsStuckHigh = true;
				} else if (cfg->tpsStuckCelAutoClear) {
					isTpsStuckHigh = false;
				}
			}

			if (stuckLowGate != m_wasStuckLowGate) {
				m_tpsLowTimer.reset();
				m_wasStuckLowGate = stuckLowGate;
			}
			if (m_tpsLowTimer.hasElapsedSec(cfg->tpsStuckCelTimeoutSec)) {
				if (stuckLowGate) {
					isTpsStuckLow = true;
				} else if (cfg->tpsStuckCelAutoClear) {
					isTpsStuckLow = false;
				}
			}
		} else {
			// No Electronic Throttle Body / pedal sensor: nothing to compare against.
			isTpsStuckHigh = false;
			isTpsStuckLow = false;
			initNoConfiguration();
		}
	} else {
		isTpsStuckHigh = false;
		isTpsStuckLow = false;
		initNoConfiguration();
	}

	uint8_t points = 0;
	if (isTpsStuckHigh || isTpsStuckLow) {
		points += cfg->tpsStuckCelPoints;
	}
	celPointsTotal = points;

	bool latchCel = cfg->celPointsThreshold > 0 && points >= cfg->celPointsThreshold;
	isCelPreWarning = !latchCel && cfg->celBlinkPointsThreshold > 0 && points >= cfg->celBlinkPointsThreshold;

	if (latchCel && isTpsStuckHigh) {
		addError(ObdCode::OBD_Throttle_Actuator_Stuck_Open);
	} else {
		removeError(ObdCode::OBD_Throttle_Actuator_Stuck_Open);
	}

	if (latchCel && isTpsStuckLow) {
		addError(ObdCode::OBD_Throttle_Actuator_Stuck_Closed);
	} else {
		removeError(ObdCode::OBD_Throttle_Actuator_Stuck_Closed);
	}
}

bool isCelPreWarningActive() {
	return engine->module<CheckEngineTriggering>().unmock().isCelPreWarning;
}

#else // !EFI_CHECK_ENGINE_TRIGGERING

void CheckEngineTriggering::onSlowCallback() { }

bool isCelPreWarningActive() {
	return false;
}

#endif // EFI_CHECK_ENGINE_TRIGGERING
