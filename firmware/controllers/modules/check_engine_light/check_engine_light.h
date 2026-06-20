#pragma once

#include "check_engine_light_state_generated.h"
#include <rusefi/timer.h>

// "Check Engine Triggering": TS-configurable threshold checks, each with its own enable bit and
// a tuner-adjustable "points" weight, that feed a points-gated CEL independent of the standard
// addError() path. See check_engine_light.md for the full design.
class CheckEngineTriggering : public check_engine_light_state_s, public EngineModule {
public:
	using interface_t = CheckEngineTriggering;

	void initNoConfiguration() override;
	void onSlowCallback() override;

private:
	// One timer per check, run in both directions: it resets whenever the gate condition flips,
	// and the check only trips/untrips once tpsStuckCelTimeoutSec has passed since the last flip.
	// This debounces both edges with the same timeout and the same timer. Untripping additionally
	// requires tpsStuckCelAutoClear -- if disabled, a tripped flag latches until power cycle.
	// Future checks (oil pressure, CLT, AFR, voltage) should follow the same one-timer pattern.
	Timer m_tpsHighTimer;
	Timer m_tpsLowTimer;
	bool m_wasStuckHighGate = false;
	bool m_wasStuckLowGate = false;
};

// Queried by MILController (malfunction_indicator.cpp) to blink the CEL pin directly as a
// pre-warning once points reach celBlinkPointsThreshold but before celPointsThreshold is hit.
bool isCelPreWarningActive();
