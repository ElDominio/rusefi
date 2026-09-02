/**
 * @file	electronic_throttle.h
 *
 * @date Dec 7, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#pragma once

#include "closed_loop_controller.h"
#include "rusefi_types.h"
#include "engine_configuration.h"

void initElectronicThrottle();
void doInitElectronicThrottle(bool isStartupInit);

void setEtbIdlePosition(percent_t pos);
void setEtbWastegatePosition(percent_t pos);
void setEtbLuaAdjustment(percent_t adjustment);
void setEwgLuaAdjustment(percent_t pos);
void setHitachiEtbCalibration();

void pickEtbOrStepper();

void blinkEtbErrorCodes(bool blinkPhase);

// same plug as 18919 AM810 but have different calibrations
void setToyota89281_33010_pedal_position_sensor();

void setBoschVAGETB();

void setDefaultEtbBiasCurve();
void setDefaultEtbParameters();
void setBoschVNH2SP30Curve();

void onConfigurationChangeElectronicThrottleCallback(engine_configuration_s *previousConfiguration);
void unregisterEtbPins();
void setProteusHitachiEtbDefaults();

void etbAutocal(dc_function_e function, bool reportToTs = true);
void etbBenchTestStart(size_t throttleIndex);
EtbStatus etbGetState(size_t throttleIndex);

float getSanitizedPedal();

#if EFI_SPORT_PEDAL
bool isSportPedalActive();
#endif

enum class EtbState : uint8_t {
  Uninitialized, // 0
  Autotune, // 1
  NoMotor, // 2
  NotEbt, // 3
  LimpProhibited, // 4
  Paused, // 5
  NoOutput, // 6
  Active, // 7
  NoPedal, // 8
  FailFast, // 9
  InInit, // 10
  SuccessfulInit, // 11
};

class DcMotor;
struct pid_s;
class ValueProvider3D;
struct pid_state_s;

class IEtbController : public ClosedLoopController<percent_t, percent_t>  {
public:
	// Initialize the throttle.
	// returns true if the throttle was initialized, false otherwise.
	virtual bool init(dc_function_e function, DcMotor *motor, pid_s *pidParameters, const ValueProvider3D* pedalMap) = 0;
	virtual void reset(const char *reason) = 0;
	virtual void setIdlePosition(percent_t pos) = 0;
	virtual void setWastegatePosition(percent_t pos) = 0;
	virtual void update() = 0;
	virtual void autoCalibrateTps(bool reportToTs = true) { (void)reportToTs; }
	virtual void startBenchTest() {}
	virtual bool isEtbMode() const = 0;

	virtual const pid_state_s& getPidState() const = 0;
  virtual float getCurrentTarget() const = 0;
	virtual void setLuaAdjustment(percent_t adjustment) = 0;

	// Re-declares ClosedLoopController's getSetpoint() (private there) so external code holding
	// only an IEtbController* can read the final blended throttle target (post idle/antilag/eco
	// blend - the same value the local PID would chase) without duplicating that computation. See
	// external-etb/RUSEFI_SIDE_TODO.md #3.1/#5.2's "expose vs duplicate" decision - this is the
	// concrete "expose it" seam, used by the external CAN ETB's remote target component
	// (can_etb_remote.cpp's sendExternalEtbTarget()). EtbController's own override is already
	// public, but that was only reachable through the concrete type, not this interface.
	expected<percent_t> getSetpoint() override = 0;

	// True while autocal or bench-test owns the motor directly instead of the normal closed-loop
	// tick (EtbImpl::update() skips TBase::update() during this time - see electronic_throttle_impl.h).
	// Lets the external CAN ETB's periodic remote-target component (#3.1) stay off the wire while
	// bench-test/autocal (#3.2) is driving ETB_TARGET itself, instead of racing it - see #6's
	// coexistence question.
	virtual bool isAutocalOrBenchTestActive() const { return false; }

	// True when checkStatus() (electronic_throttle.cpp) found a fault/pause condition. Since a
	// throttle owned by an external CAN ETB controller never reaches its own local-motor-disable
	// path (update() returns before that - see #1/#3.1), the remote-target component checks this
	// instead, to stop sending ETB_TARGET rather than racing a "disable" with no wire
	// representation (can_bus.h: silence is the fail-safe signal, there is no disable bit).
	virtual bool isEtbFaulted() const { return false; }
};
