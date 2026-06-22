/**
 * @file downshift_blipper.h
 *
 * Automated downshift rev-matcher. On a clutch-in, single-gear downshift the
 * Engine State Machine raises engineSmIsDownshifting; this module then briefly
 * commands the Electronic Throttle Body (overriding the driver pedal target) to
 * bring engine RPM up to the next-lower gear's ideal speed so the clutch
 * re-engages smoothly.
 *
 * See docs/../downshift_blipper_design.md for the full design.
 */

#pragma once

#include "engine_module.h"
#include "downshift_blipper_state_generated.h"
#include "efi_pid.h"
#include <rusefi/timer.h>

enum class DownshiftBlipState : uint8_t {
	Idle      = 0,
	RampOpen  = 1,
	ActivePID = 2,
	RampClose = 3,
	Lockout   = 4,
};

class DownshiftBlipper : public downshift_blipper_state_s, public EngineModule {
public:
	using interface_t = DownshiftBlipper;

	void onFastCallback() override;
	void onConfigurationChange(engine_configuration_s const* previousConfig) override;

	// Read by EtbController::getSetpointEtb(): when active, the blip request
	// fully replaces the pedal-derived target so one or two ETBs follow it.
	bool isActive() const;
	float getThrottleRequest() const { return downshiftBlipThrottleRequest; }

private:
	void setState(DownshiftBlipState newState);
	void updateGearLatch(efitimems_t nowMs);
	// Returns target engine RPM for the downshift, or 0 if the blip should be aborted.
	float computeTargetRpm() const;
	bool passesEntryGate(float rpm, float vss, float driverTps) const;
	bool shouldTerminate(float driverTps, bool downshifting) const;
	float runPid(float rpm, float dt);
	bool isLuaGateBlocking() const;

	Pid m_pid;
	bool m_pidInited = false;

	bool m_prevDownshift = false;

	// Gear latch: last non-zero DetectedGear that was stable for >= GEAR_STABLE_MS.
	size_t m_gearCandidate = 0;
	size_t m_lastStableGear = 0;
	Timer m_gearStableTimer;

	// Engine RPM captured at the downshift edge, while still drivetrain-locked.
	float m_latchedRpm = 0;

	Timer m_stateTimer;   // reset on every state transition (ramp / lockout timing)
	Timer m_blipTimer;    // reset at RampOpen entry (overall max-blip safety timeout)

	float m_rampStartTps   = 0;  // throttle at RampOpen entry
	float m_rampCloseStart = 0;  // throttle at RampClose entry
};
