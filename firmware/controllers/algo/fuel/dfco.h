/**
 * @file dfco.h
 */

#pragma once
#include "engine_module.h"
#include <rusefi/timer.h>
#include "hysteresis.h"

// DFCO = deceleration fuel cut off, ie, save gas when your foot is off the pedal
class DfcoController : public EngineModule {
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
	bool getState() const;
	bool m_isDfco = false;

	mutable Hysteresis m_mapHysteresis;

	Timer m_timeSinceCut;
	Timer m_timeSinceNoCut;
};
