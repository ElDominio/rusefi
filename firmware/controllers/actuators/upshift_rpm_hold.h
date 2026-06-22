/**
 * @file upshift_rpm_hold.h
 *
 * Automated upshift RPM hold — the inverse of the Downshift Blipper. On a clutch-in,
 * single-gear upshift the Engine State Machine raises engineSmIsUpshifting; this module
 * then briefly commands the Electronic Throttle Body (overriding the driver pedal target)
 * to HOLD engine RPM at the next-higher gear's ideal speed instead of letting it free-fall
 * through the shift, so the clutch re-engages smoothly.
 *
 * Shares the TPS-to-RPM Feed Forward curve (page 5) with the Downshift Blipper: the curve
 * seeds the PID with the throttle needed to sustain the target RPM in neutral, so the PID
 * only corrects the residual error.
 */

#pragma once

#include "engine_module.h"
#include "upshift_rpm_hold_state_generated.h"
#include "efi_pid.h"
#include <rusefi/timer.h>

enum class UpshiftHoldState : uint8_t {
	Idle      = 0,
	RampDown  = 1,
	ActivePID = 2,
	RampUp    = 3,
	Lockout   = 4,
};

class UpshiftRpmHold : public upshift_rpm_hold_state_s, public EngineModule {
public:
	using interface_t = UpshiftRpmHold;

	void onFastCallback() override;
	void onConfigurationChange(engine_configuration_s const* previousConfig) override;

	// Read by EtbController::getSetpointEtb(): when active, the hold request fully
	// replaces the pedal-derived target so one or two ETBs follow it.
	bool isActive() const;
	float getThrottleRequest() const { return upshiftHoldThrottleRequest; }

private:
	void setState(UpshiftHoldState newState);
	void updateGearLatch(efitimems_t nowMs);
	// Returns target engine RPM for the upshift, or 0 if the hold should be aborted.
	float computeTargetRpm() const;
	bool passesEntryGate(float rpm, float vss, float driverTps) const;
	bool shouldTerminate(float driverTps, bool upshifting) const;
	float runPid(float rpm, float dt);

	Pid m_pid;
	bool m_pidInited = false;

	bool m_prevUpshift = false;

	// Gear latch: last non-zero DetectedGear that was stable for >= GEAR_STABLE_MS.
	size_t m_gearCandidate = 0;
	size_t m_lastStableGear = 0;
	Timer m_gearStableTimer;

	// Engine RPM captured at the upshift edge, while still drivetrain-locked.
	float m_latchedRpm = 0;

	Timer m_stateTimer;   // reset on every state transition (ramp / lockout timing)
	Timer m_holdTimer;    // reset at RampDown entry (overall max-hold safety timeout)

	float m_rampStartTps = 0;  // throttle at RampDown entry
	float m_rampUpStart  = 0;  // throttle at RampUp entry
};
