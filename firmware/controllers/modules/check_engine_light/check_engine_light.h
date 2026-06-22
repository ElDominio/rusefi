#pragma once

#include "check_engine_light_state_generated.h"
#include <rusefi/timer.h>

// Shared debounce primitive used by every Check Engine Triggering check: the raw trip condition
// must hold continuously for `timeoutSec` before update() reports tripped, and be continuously
// clear for the same duration before it reports untripped. All checks reuse the same
// celDebounceTimeSec page-5 field as `timeoutSec`, per the Check Engine Triggering design.
class DebouncedGate {
public:
	void reset() {
		m_timer.reset();
		m_wasConditionSet = false;
		m_tripped = false;
	}

	bool update(bool conditionNow, float timeoutSec) {
		if (conditionNow != m_wasConditionSet) {
			m_timer.reset();
			m_wasConditionSet = conditionNow;
		}
		if (m_timer.hasElapsedSec(timeoutSec)) {
			m_tripped = conditionNow;
		}
		return m_tripped;
	}

private:
	Timer m_timer;
	bool m_wasConditionSet = false;
	bool m_tripped = false;
};

// "Check Engine Triggering": TS-configurable threshold checks, each with its own enable bit and
// worth a flat 1 point, that feed a points-gated CEL independent of the standard addError() path.
// See check_engine_light.md for the full design.
class CheckEngineTriggering : public check_engine_light_state_s, public EngineModule {
public:
	using interface_t = CheckEngineTriggering;

	void initNoConfiguration() override;
	void onSlowCallback() override;

private:
	static constexpr int TPS_FLIP_HISTORY_SIZE = 10;

	DebouncedGate m_tpsCircuitLowGate;
	DebouncedGate m_tpsCircuitHighGate;
	DebouncedGate m_tpsIntermittentGate;

	// Ring buffer of recent TPS1 ok<->fault transition times, used to count flips within
	// celDebounceTimeSec for the Intermittent check.
	Timer m_tps1FlipTimers[TPS_FLIP_HISTORY_SIZE];
	int m_tps1FlipIndex = 0;
	bool m_wasTps1Faulted = false;
};

// Queried by MILController (malfunction_indicator.cpp) to flash the CEL pin directly, in place
// of the normal digit-blink-out, once points reach celBlinkPointsThreshold -- an already-active
// DTC (celPointsThreshold) escalating to a more critical, actively-flashing CEL.
bool isCelBlinkingActive();
