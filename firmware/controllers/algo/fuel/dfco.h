/**
 * @file dfco.h
 */

#pragma once
#include "engine_module.h"
#include <rusefi/timer.h>
#include "hysteresis.h"
#include "dfco_state_generated.h"
#include "engine_state_machine.h"

// DFCO = deceleration fuel cut off, ie, save gas when your foot is off the pedal
class DfcoController : public dfco_state_s, public EngineModule {
public:
	void update();

	// true when overrun conditions (TPS/RPM/VSS) are met — used by the state machine
	// does not check coastingFuelCutEnabled, CLT, or MAP; no hysteresis complexity
	bool isOverrun() const;
	// true if fuel should be cut, false during normal running
	bool cutFuel() const;
	// Degrees of timing to retard due to DFCO, positive removes timing
	float getTimingRetard() const;

	float getTimeSinceCut() const;

private:
	bool commonGuards() const;
	bool overrunActive() const;
	bool getState(const EngineStateMachine& sm) const;

	bool m_isDfco = false;

	mutable Hysteresis m_mapHysteresis;

	Timer m_timeSinceCut;
	Timer m_timeSinceNoCut;
};
