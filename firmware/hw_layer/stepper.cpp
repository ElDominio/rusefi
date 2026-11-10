/**
 * @file	stepper.cpp
 *
 * http://rusefi.com/wiki/index.php?title=Hardware:Stepper_motor
 *
 * @date Dec 24, 2014
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#include "pch.h"

#include "stepper.h"
#include "custom_page.h"

#if !EFI_UNIT_TEST
RUSEFI_STACK_ROOT(StepperMotor, ThreadTask);
#endif

float StepperMotorBase::getTargetPosition() const {
	return m_targetPosition;
}

void StepperMotorBase::setTargetPosition(float targetPositionSteps) {
	// When the IAC position value change is insignificant (lower than this threshold), leave the poor valve alone
	// When we get a larger change, actually update the target stepper position
	float minMove = 1;
#if EFI_STEPPER_IDLE_EXT
	// [tag:stepper_idle_ext] configurable deadband against noise-driven hunting
	minMove = std::max(1.0f, (float)getCustomPage()->stepperMinMoveSteps);
	// a move all the way to the home (step 0) end always goes through (also arms the home resync)
	bool closing = targetPositionSteps <= 0 && m_targetPosition > 0;
#else
	bool closing = false;
#endif // EFI_STEPPER_IDLE_EXT
	if (closing || std::abs(m_targetPosition - targetPositionSteps) >= minMove) {
		m_targetPosition = targetPositionSteps;
	}
}

void StepperMotorBase::initialize(StepperHw *hardware, int totalSteps) {
	m_totalSteps = maxI(3, totalSteps);

	m_hw = hardware;
}

void StepperMotorBase::saveStepperPos(int pos) {
	// use backup-power RTC registers to store the data
#if EFI_PROD_CODE && EFI_BACKUP_SRAM
	backupRamSave(backup_ram_e::StepperPosition, pos + 1);
#else
  UNUSED(pos);
#endif
	postCurrentPosition();
}

int StepperMotorBase::loadStepperPos() {
#if EFI_PROD_CODE && EFI_BACKUP_SRAM
	return (int)backupRamLoad(backup_ram_e::StepperPosition) - 1;
#else
	return 0;
#endif
}

void StepperMotorBase::changeCurrentPosition(bool positive) {
	if (positive) {
		m_currentPosition++;
	} else {
		m_currentPosition--;
	}
	postCurrentPosition();
}

void StepperMotorBase::postCurrentPosition() {
//	if (engineConfiguration->debugMode == DBG_STEPPER_IDLE_CONTROL) {
//#if EFI_TUNER_STUDIO
//		engine->outputChannels.debugIntField5 = m_currentPosition;
//#endif /* EFI_TUNER_STUDIO */
//	}
}

void StepperMotorBase::setInitialPosition() {
	// try to get saved stepper position (-1 for no data)
	m_currentPosition = loadStepperPos();

#if HAL_USE_ADC
	// first wait until at least 1 slowADC sampling is complete
	waitForSlowAdc();
#endif

#if EFI_SHAFT_POSITION_INPUT
	bool isRunning = engine->rpmCalculator.isRunning();
#else
	bool isRunning = false;
#endif /* EFI_SHAFT_POSITION_INPUT */
	// now check if stepper motor re-initialization is requested - if the throttle pedal is pressed at startup
	auto tpsPos = Sensor::getOrZero(SensorType::DriverThrottleIntent);
	bool forceStepperParking = !isRunning && tpsPos > STEPPER_PARKING_TPS;
	if (engineConfiguration->stepperForceParkingEveryRestart)
		forceStepperParking = true;
	efiPrintf("Stepper: savedStepperPos=%d forceStepperParking=%d (tps=%.2f)", m_currentPosition, (forceStepperParking ? 1 : 0), tpsPos);

	if (m_currentPosition < 0 || forceStepperParking) {
		efiPrintf("Stepper: starting parking time=%lums", (unsigned long)getTimeNowMs());
		// reset saved value
		saveStepperPos(-1);

		/**
		 * let's park the motor in a known position to begin with
		 *
		 * I believe it's safer to retract the valve for parking - at least on a bench I've seen valves
		 * disassembling themselves while pushing too far out.
		 *
		 * Add extra steps to compensate step skipping by some old motors.
		 */
		int numParkingSteps = (int)efiRound((1.0f + (float)engineConfiguration->stepperParkingExtraSteps / PERCENT_MULT) * m_totalSteps, 1.0f);
		for (int i = 0; i < numParkingSteps; i++) {
			if (!m_hw->step(false)) {
				initialPositionSet = false;
				return;
			}
			changeCurrentPosition(false);
		}

		// keep pushing into the stop until the coil phase is back at zero, so that position 0 always
		// corresponds to phase 0 (see StepperHw::isAtPhaseZero)
		if (!alignPhaseAtHomeStop()) {
			initialPositionSet = false;
			return;
		}

		// set & save zero stepper position after the parking completion
		m_currentPosition = 0;
		saveStepperPos(m_currentPosition);
		// todo: is this a slow operation on the start-up path?
		efiPrintf("Stepper: parking finished time=%lums", (unsigned long)getTimeNowMs());
	} else {
		// Restored from backup RAM: re-establish the coil phase that matches this position,
		// otherwise the first step after boot energizes a pattern the rotor isn't at and the
		// valve can jump the wrong way / lose steps (dual H-bridge / push-pull only).
		m_hw->restorePhase(m_currentPosition);
		// The initial target position should correspond to the saved stepper position.
		// Idle thread starts later and sets a new target position.
		// Assigned directly: the move deadband must not keep a stale target here.
		m_targetPosition = m_currentPosition;
	}

	initialPositionSet = true;
}

void StepperMotorBase::doIteration() {
	int targetPosition = efiRound(getTargetPosition(), 1);

	// stepper requires +12V
	if (!isIgnVoltage()) {
	  initialPositionSet = false;
		m_hw->deenergize();
		m_hw->pause();
		return;
	}

	if (!initialPositionSet) {
		setInitialPosition();
		return;
	}

#if EFI_STEPPER_IDLE_EXT
	if (getCustomPage()->stepperFlipDirection) {
		// [tag:stepper_idle_ext] flipped: totalSteps is the extended end with no safe stop, never
		// command past it whatever the idle controller asks for
		targetPosition = std::min(targetPosition, m_totalSteps);
	}

	if (handleHomeStopResync(targetPosition)) {
		// this iteration was spent on a resync step
		return;
	}
#endif // EFI_STEPPER_IDLE_EXT

	// read after the resync above, which may have re-referenced the counter
	int currentPosition = m_currentPosition;

	if (targetPosition == currentPosition) {
		m_hw->sleep();
		m_isBusy = false;
		return;
	}

	m_isBusy = true;

	bool isIncrementing = targetPosition > currentPosition;

	if (m_hw->step(isIncrementing)) {
		changeCurrentPosition(isIncrementing);
	}

	// save position to backup RTC register
#if EFI_PROD_CODE
	saveStepperPos(m_currentPosition);
#endif
}

bool StepperMotorBase::alignPhaseAtHomeStop() {
	// at most one full electrical cycle (4 full steps); bounded in case the hardware cannot reach
	// phase zero (e.g. stepping mode changed at runtime)
	for (int i = 0; i < 4 && !m_hw->isAtPhaseZero(); i++) {
		if (!m_hw->step(false)) {
			return false;
		}
	}
	return true;
}

#if EFI_STEPPER_IDLE_EXT
/**
 * [tag:stepper_idle_ext] Home-stop resync.
 *
 * A stepper idle valve has no position feedback: m_currentPosition is just a step counter, and
 * every step the motor fails to make (low voltage, binding, too short a reaction time, asymmetric
 * spring/vacuum load) silently shifts the real valve away from the counter. Parking on boot is the
 * only regular re-reference. When the idle controller commands 0% or below while running, the
 * valve is meant to sit on the home (parking) stop anyway, so we drive stepperHomeResyncSteps extra
 * steps into that stop (the counter does not move while doing so) and re-define it as position 0.
 * Home is the closed end normally, the fully-open end with stepperFlipDirection (100% duty).
 * Commands below 0 (overtravel, see clampIdleValvePosition) are held at 0: the valve cannot go
 * further than the stop, hammering it would only re-introduce windup.
 *
 * Returns true if this iteration was consumed by a resync step.
 */
bool StepperMotorBase::handleHomeStopResync(int& targetPosition) {
	int resyncSteps = getCustomPage()->stepperHomeResyncSteps;
	if (resyncSteps == 0 || targetPosition > 0) {
		m_homeResyncDone = false;
		m_resyncStepsDone = 0;
		return false;
	}

	targetPosition = 0;
	if (m_homeResyncDone || m_currentPosition > 0) {
		// still on the way down to 0 (normal stepping), or already resynced at this home command
		return false;
	}

	bool needMoreSteps = m_resyncStepsDone < resyncSteps
		// then finish on phase zero to keep the position <-> phase invariant, bounded to one cycle
		|| (!m_hw->isAtPhaseZero() && m_resyncStepsDone < resyncSteps + 4);
	if (needMoreSteps) {
		m_isBusy = true;
		m_hw->step(false);
		m_resyncStepsDone++;
		return true;
	}

	m_currentPosition = 0;
	saveStepperPos(m_currentPosition);
	m_homeResyncDone = true;
	return false;
}
#endif // EFI_STEPPER_IDLE_EXT

bool StepperMotorBase::isBusy() const {
	return m_isBusy;
}

void StepDirectionStepper::setDirection(bool isIncrementing) {
	if (isIncrementing != m_currentDirection) {
		// compensate stepper motor inertia
		pause();
		m_currentDirection = isIncrementing;
	}

	directionPin.setValue(isIncrementing);
}

bool StepDirectionStepper::pulse() {
	enablePin.setValue(false); // enable stepper

	stepPin.setValue(true);
	pause();

	stepPin.setValue(false);
	pause();

#if EFI_STEPPER_IDLE_EXT
	// [tag:stepper_idle_ext] keep the coils energized between steps (holding torque); released by
	// deenergize() when ignition voltage goes away
	if (getCustomPage()->stepperKeepDriverEnabled) {
		return true;
	}
#endif // EFI_STEPPER_IDLE_EXT
	enablePin.setValue(true); // disable stepper

	return true;
}

void StepDirectionStepper::deenergize() {
	enablePin.setValue(true); // disable stepper
}

bool StepDirectionStepper::isDriverEnabled() const {
	// enable is active-low in logic terms: setValue(false) enables the driver
	return !enablePin.getLogicValue();
}

void StepperHw::sleep() {
	pause();
}

void StepperHw::pause(int divisor) const {
#if EFI_UNIT_TEST
	// no threads to sleep in unit tests - stepping happens synchronously
	UNUSED(divisor);
#else
	// currently we can't sleep less than 1ms (see #3214)
	chThdSleepMicroseconds(maxI(MS2US(1), (int)(MS2US(m_reactionTime)) / divisor));
#endif
}

void StepperHw::setReactionTime(float ms) {
	m_reactionTime = std::max(1.0f, ms);
}

bool StepDirectionStepper::step(bool positive) {
	setDirection(positive);
	return pulse();
}

#if !EFI_UNIT_TEST
void StepperMotor::initialize(StepperHw *hardware, int totalSteps) {
	StepperMotorBase::initialize(hardware, totalSteps);

	start();
}
#endif // EFI_UNIT_TEST

void StepDirectionStepper::initialize(brain_pin_e p_stepPin, brain_pin_e p_directionPin, pin_output_mode_e p_directionPinMode, float reactionTime, brain_pin_e p_enablePin, pin_output_mode_e p_enablePinMode) {
	if (!isBrainPinValid(p_stepPin) || !isBrainPinValid(p_directionPin)) {
		return;
	}

	setReactionTime(reactionTime);

	directionPinMode = p_directionPinMode;
	directionPin.initPin("Stepper DIR", p_directionPin, directionPinMode);

	stepPinMode = OM_DEFAULT;	// todo: do we need configurable stepPinMode?
	stepPin.initPin("Stepper step", p_stepPin, stepPinMode);

	enablePinMode = p_enablePinMode;
	enablePin.initPin("Stepper EN", p_enablePin, enablePinMode);

	// All pins must be 0 for correct hardware startup (e.g. stepper auto-disabling circuit etc.).
	enablePin.setValue(true); // disable stepper
	stepPin.setValue(false);
	directionPin.setValue(false);
	m_currentDirection = false;
}
