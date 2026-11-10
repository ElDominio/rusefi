/**
 * @file	stepper.h
 *
 * @date Dec 24, 2014
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#pragma once

#include "global.h"
#include "efi_gpio.h"
#include "backup_ram.h"
#include "thread_controller.h"

class StepperHw {
public:
	virtual bool step(bool positive) = 0;
	// pause between steps
	void pause(int divisor = 1) const;
	// pause and enter the idle mode (less current consumption)
	virtual void sleep();
	// release the coils/driver completely, e.g. when ignition voltage is lost
	virtual void deenergize() { }

	// Coil phase bookkeeping for drivers whose commutation phase lives in the ECU (dual H-bridge /
	// push-pull). Invariant: phase == f(step position), established by ending every home-stop
	// park/resync on phase zero, so a position restored from backup RAM can restore the phase too.
	// Step/Direction drivers keep the phase in their own indexer, hence the no-op defaults.
	virtual bool isAtPhaseZero() const { return true; }
	virtual void restorePhase(int /*position*/) { }

protected:
	void setReactionTime(float ms);

private:
	float m_reactionTime = 5.0f;
};

class StepDirectionStepper final : public StepperHw {
public:
	void initialize(brain_pin_e stepPin, brain_pin_e directionPin, pin_output_mode_e directionPinMode, float reactionTime, brain_pin_e enablePin, pin_output_mode_e enablePinMode);

	bool step(bool positive) override;
	void deenergize() override;

	bool isDriverEnabled() const;

private:
	bool pulse();
	void setDirection(bool isIncrementing);

	bool m_currentDirection = false;

	OutputPin directionPin, stepPin, enablePin;
	pin_output_mode_e directionPinMode, stepPinMode, enablePinMode;
};

class DcMotor;

class DualHBridgeStepper final : public StepperHw {
public:
    void initialize(DcMotor* motorPhaseA, DcMotor* motorPhaseB, float reactionTime);

    bool step(bool positive) override;

	void sleep() override;

	bool isAtPhaseZero() const override;
	void restorePhase(int position) override;

protected:
	bool update(float dutyMult);

private:
    DcMotor* m_motorPhaseA = nullptr;
    DcMotor* m_motorPhaseB = nullptr;

    uint8_t m_phase = 0;
};

class StepperMotorBase {
public:
	virtual void initialize(StepperHw* hardware, int totalSteps);
	void doIteration();

	void setTargetPosition(float targetPositionSteps);
	float getTargetPosition() const;

	bool isBusy() const;

	int m_currentPosition = 0;
	int m_totalSteps = 0;

protected:
	void setInitialPosition(void);

	void saveStepperPos(int pos);
	int loadStepperPos();

	void changeCurrentPosition(bool positive);
	void postCurrentPosition(void);
	// steps past the counted closed position until the hardware phase is back at zero, see isAtPhaseZero()
	bool alignPhaseAtHomeStop();
#if EFI_STEPPER_IDLE_EXT
	bool handleHomeStopResync(int& targetPosition);
	int m_resyncStepsDone = 0;
	bool m_homeResyncDone = false;
#endif // EFI_STEPPER_IDLE_EXT

	StepperHw* m_hw = nullptr;

	float m_targetPosition = 0;
	bool initialPositionSet = false;
	bool m_isBusy = false;
};

#if !EFI_UNIT_TEST

class StepperMotor final : public StepperMotorBase, private ThreadController<UTILITY_THREAD_STACK_SIZE> {
public:
	using ThreadController::stackSize;

	StepperMotor() : ThreadController("stepper", PRIO_STEPPER) {}

	void initialize(StepperHw* hardware, int totalSteps);

	void ThreadTask() override {
		// Require hardware to be set
		if (!m_hw) {
			return;
		}

		while (true) {
			doIteration();
		}
	}
};
#else
// no dedicated thread in unit tests - tests pump doIteration() by hand
using StepperMotor = StepperMotorBase;
#endif
