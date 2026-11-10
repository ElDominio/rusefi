/**
 * @file test_idle_hardware.cpp
 *
 * Coverage for applyIACposition(): single solenoid vs double solenoid
 * (Subaru/BMW style "three wire") idle valve duty split, plus initIdleHardware()
 * stepper branches: step/dir driver, dual H-bridge and raw-output stepper - the
 * latter two claim DC motor pool slots ETB_COUNT+0/1, see
 * https://github.com/rusefi/rusefi/pull/9466 for the planned pool split.
 */

#include "pch.h"

#include "idle_hardware.h"
#include "dc_motors.h"
#include "stepper.h"

TEST(Actuators, IdleSingleSolenoid) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->isDoubleSolenoidIdle = false;
	// pretend a bench test is in progress so the valve is driven even while the engine is stopped
	engine->timeToStopIdleTest = getTimeNowUs() + MS2US(3000);

	applyIACposition(30);
	EXPECT_NEAR(0.30f, getIdleSolenoidOpenDutyForUnitTest(), EPS4D);

	applyIACposition(75);
	EXPECT_NEAR(0.75f, getIdleSolenoidOpenDutyForUnitTest(), EPS4D);
}

TEST(Actuators, IdleDoubleSolenoid) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->isDoubleSolenoidIdle = true;
	engine->timeToStopIdleTest = getTimeNowUs() + MS2US(3000);

	// duty is mapped into the 0.01..0.99 range: open = 0.01 + 0.98 * duty, close is complementary
	applyIACposition(30);
	EXPECT_NEAR(0.304f, getIdleSolenoidOpenDutyForUnitTest(), EPS4D);
	EXPECT_NEAR(0.696f, getIdleSolenoidCloseDutyForUnitTest(), EPS4D);

	// near the extremes both coils keep switching - neither goes to DC on/off
	applyIACposition(1);
	EXPECT_NEAR(0.0198f, getIdleSolenoidOpenDutyForUnitTest(), EPS4D);
	EXPECT_NEAR(0.9802f, getIdleSolenoidCloseDutyForUnitTest(), EPS4D);

	// at EXACTLY 0%/100% float rounding pushes the nominal 0.01/0.99 endpoints just past
	// ZERO_PWM_THRESHOLD/FULL_PWM_THRESHOLD, so the coils degrade to plain digital on/off
	applyIACposition(0);
	EXPECT_EQ(0, getIdleSolenoidOpenDutyForUnitTest());
	EXPECT_EQ(1, getIdleSolenoidCloseDutyForUnitTest());

	applyIACposition(100);
	EXPECT_EQ(1, getIdleSolenoidOpenDutyForUnitTest());
	EXPECT_EQ(0, getIdleSolenoidCloseDutyForUnitTest());
}

TEST(Actuators, IdleSingleSolenoidInit) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->useStepperIdle = false;
	engineConfiguration->isDoubleSolenoidIdle = false;
	engineConfiguration->idle.solenoidPin = Gpio::A7;
	engineConfiguration->idle.solenoidFrequency = 200;

	EXPECT_NO_FATAL_ERROR(initIdleHardware());

	engine->timeToStopIdleTest = getTimeNowUs() + MS2US(3000);
	applyIACposition(30);
	EXPECT_NEAR(0.30f, getIdleSolenoidOpenDutyForUnitTest(), EPS4D);
}

TEST(Actuators, IdleDoubleSolenoidRequiresSecondPin) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->useStepperIdle = false;
	engineConfiguration->isDoubleSolenoidIdle = true;
	engineConfiguration->idle.solenoidPin = Gpio::A7;
	engineConfiguration->idle.solenoidFrequency = 200;
	engineConfiguration->secondSolenoidPin = Gpio::Unassigned;

	EXPECT_FATAL_ERROR(initIdleHardware());
}

TEST(Actuators, StepDirStepperIdle) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	Sensor::setMockValue(SensorType::BatteryVoltage, 12);

	engineConfiguration->useStepperIdle = true;
	engineConfiguration->useHbridgesToDriveIdleStepper = false;
	engineConfiguration->useRawOutputToDriveIdleStepper = false;
	engineConfiguration->idleStepperTotalSteps = 200;
	engineConfiguration->idle.stepperStepPin = Gpio::C13;
	engineConfiguration->idle.stepperDirectionPin = Gpio::C14;

	EXPECT_NO_FATAL_ERROR(initIdleHardware());

	auto& iac = getIacMotorForUnitTest();

	// first iteration latches the initial (saved = 0) position, no parking needed
	iac.doIteration();
	EXPECT_EQ(0, iac.m_currentPosition);

	// 25% of 200 steps
	applyIACposition(25);
	EXPECT_FLOAT_EQ(50, iac.getTargetPosition());

	// the motor works its way toward the target one step per iteration
	iac.doIteration();
	iac.doIteration();
	iac.doIteration();
	EXPECT_EQ(3, iac.m_currentPosition);
	EXPECT_TRUE(isIdleMotorBusy());
}

// full-step "two phase on" quadrature sequence of DualHBridgeStepper, see stepper_dual_hbridge.cpp
static void expectHbridgePhase(int phase) {
	static const float phaseA[] = { 1, 1, -1, -1 };
	static const float phaseB[] = { -1, 1, 1, -1 };

	// legacy invariant of the shared DC pool: the idle stepper is wired to the two
	// slots right after the ETBs
	DcMotor* motorA = getDcMotorForUnitTest(ETB_COUNT + 0);
	DcMotor* motorB = getDcMotorForUnitTest(ETB_COUNT + 1);

	EXPECT_FLOAT_EQ(phaseA[phase & 3], motorA->get()) << "phase A at step " << phase;
	EXPECT_FLOAT_EQ(phaseB[phase & 3], motorB->get()) << "phase B at step " << phase;
}

static void runHbridgeStepperIdleScenario() {
	Sensor::setMockValue(SensorType::BatteryVoltage, 12);

	engineConfiguration->useStepperIdle = true;
	engineConfiguration->idleStepperTotalSteps = 4;
	engineConfiguration->stepperNumMicroSteps = STEPPER_FULL;
	// force deterministic parking so the test does not depend on saved position
	engineConfiguration->stepperForceParkingEveryRestart = true;
	engineConfiguration->stepperParkingExtraSteps = 0;

	EXPECT_NO_FATAL_ERROR(initIdleHardware());

	auto& iac = getIacMotorForUnitTest();

	// motors untouched until the first iteration
	EXPECT_FLOAT_EQ(0, getDcMotorForUnitTest(ETB_COUNT + 0)->get());
	EXPECT_FLOAT_EQ(0, getDcMotorForUnitTest(ETB_COUNT + 1)->get());

	// first iteration parks the valve: idleStepperTotalSteps reverse steps back to phase 0
	iac.doIteration();
	EXPECT_EQ(0, iac.m_currentPosition);
	expectHbridgePhase(0);

	// 100% -> target is idleStepperTotalSteps
	applyIACposition(100);
	EXPECT_FLOAT_EQ(4, iac.getTargetPosition());

	// each iteration advances the quadrature sequence by exactly one step
	for (int step = 1; step <= 4; step++) {
		iac.doIteration();
		EXPECT_EQ(step, iac.m_currentPosition);
		expectHbridgePhase(step);
		EXPECT_TRUE(isIdleMotorBusy());
	}

	// once on target the motor goes to sleep and reports not busy
	iac.doIteration();
	EXPECT_EQ(4, iac.m_currentPosition);
	EXPECT_FALSE(isIdleMotorBusy());
}

TEST(Actuators, HbridgeStepperIdleDrivesDcPoolSlots) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->useHbridgesToDriveIdleStepper = true;

	runHbridgeStepperIdleScenario();
}

TEST(Actuators, RawOutputStepperIdleDrivesDcPoolSlots) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// four push-pull outputs directly driving the stepper coils
	engineConfiguration->useRawOutputToDriveIdleStepper = true;
	engineConfiguration->stepper_raw_output[0] = Gpio::D0;
	engineConfiguration->stepper_raw_output[1] = Gpio::D1;
	engineConfiguration->stepper_raw_output[2] = Gpio::D2;
	engineConfiguration->stepper_raw_output[3] = Gpio::D3;

	runHbridgeStepperIdleScenario();
}

TEST(Actuators, IdleSolenoidsOffWhileEngineStopped) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->isDoubleSolenoidIdle = true;

	// while a bench test is active the valves are driven despite the engine being stopped
	engine->timeToStopIdleTest = getTimeNowUs() + MS2US(3000);
	applyIACposition(50);
	EXPECT_NEAR(0.50f, getIdleSolenoidOpenDutyForUnitTest(), EPS4D);
	EXPECT_NEAR(0.50f, getIdleSolenoidCloseDutyForUnitTest(), EPS4D);

	// engine stopped and no bench test -> both solenoids off to be quieter and save power
	engine->timeToStopIdleTest = 0;
	applyIACposition(50);
	EXPECT_EQ(0, getIdleSolenoidOpenDutyForUnitTest());
	EXPECT_EQ(0, getIdleSolenoidCloseDutyForUnitTest());
}

/**
 * #9123: some valves need to rest at a position rather than de-energized while the engine is
 * stopped. Off-while-stopped stays the default; opting in keeps driving the valve to the position
 * the idle controller already computed, and only for a bounded time - holding a solenoid
 * energized with the engine off drains the battery and can overheat the coil.
 */
TEST(Actuators, IdleSolenoidHeldWhileEngineStopped) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->isDoubleSolenoidIdle = true;
	engine->timeToStopIdleTest = 0;

	// default: engine stopped means both solenoids off, unchanged from before
	engineConfiguration->keepIdleSolenoidWhenStopped = false;
	applyIACposition(50);
	EXPECT_EQ(0, getIdleSolenoidOpenDutyForUnitTest());
	EXPECT_EQ(0, getIdleSolenoidCloseDutyForUnitTest());

	// opted in, but this ECU has never seen the engine turn: nothing to hold after, stay off
	engineConfiguration->keepIdleSolenoidWhenStopped = true;
	applyIACposition(50);
	EXPECT_EQ(0, getIdleSolenoidOpenDutyForUnitTest());
	EXPECT_EQ(0, getIdleSolenoidCloseDutyForUnitTest());

	// the engine turned and stopped 5 seconds ago: still driven to the requested position
	engine->triggerCentral.m_lastEventTimer.reset();
	eth.moveTimeForwardSec(5);
	ASSERT_FALSE(engine->triggerCentral.engineMovedRecently());
	applyIACposition(50);
	EXPECT_NEAR(0.50f, getIdleSolenoidOpenDutyForUnitTest(), EPS4D);
	EXPECT_NEAR(0.50f, getIdleSolenoidCloseDutyForUnitTest(), EPS4D);

	// past the timeout: switched off despite the option, to protect the coil and the battery
	eth.moveTimeForwardSec(IDLE_SOLENOID_HOLD_TIMEOUT_SEC);
	applyIACposition(50);
	EXPECT_EQ(0, getIdleSolenoidOpenDutyForUnitTest());
	EXPECT_EQ(0, getIdleSolenoidCloseDutyForUnitTest());
}

TEST(Actuators, IdleSingleSolenoidHeldWhileEngineStopped) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->isDoubleSolenoidIdle = false;
	engine->timeToStopIdleTest = 0;

	engineConfiguration->keepIdleSolenoidWhenStopped = false;
	applyIACposition(70);
	EXPECT_EQ(0, getIdleSolenoidOpenDutyForUnitTest());

	engineConfiguration->keepIdleSolenoidWhenStopped = true;
	engine->triggerCentral.m_lastEventTimer.reset();
	eth.moveTimeForwardSec(5);
	applyIACposition(70);
	EXPECT_NEAR(0.70f, getIdleSolenoidOpenDutyForUnitTest(), EPS4D);

	eth.moveTimeForwardSec(IDLE_SOLENOID_HOLD_TIMEOUT_SEC);
	applyIACposition(70);
	EXPECT_EQ(0, getIdleSolenoidOpenDutyForUnitTest());
}

// ---------------------------------------------------------------------------------------------
// Stepper idle extensions [tag:stepper_idle_ext]
// ---------------------------------------------------------------------------------------------

static StepperMotorBase& initStepDirStepperForExtTest(int totalSteps) {
	Sensor::setMockValue(SensorType::BatteryVoltage, 12);

	engineConfiguration->useStepperIdle = true;
	engineConfiguration->useHbridgesToDriveIdleStepper = false;
	engineConfiguration->useRawOutputToDriveIdleStepper = false;
	engineConfiguration->idleStepperTotalSteps = totalSteps;
	engineConfiguration->idle.stepperStepPin = Gpio::C13;
	engineConfiguration->idle.stepperDirectionPin = Gpio::C14;
	engineConfiguration->stepperEnablePin = Gpio::C15;
	// start from the saved (= 0 in unit tests) position, no parking steps
	engineConfiguration->stepperForceParkingEveryRestart = false;

	EXPECT_NO_FATAL_ERROR(initIdleHardware());

	auto& iac = getIacMotorForUnitTest();
	// first iteration latches the initial (saved = 0) position
	iac.doIteration();
	EXPECT_EQ(0, iac.m_currentPosition);
	return iac;
}

static void iterate(StepperMotorBase& iac, int count) {
	for (int i = 0; i < count; i++) {
		iac.doIteration();
	}
}

TEST(Actuators, StepperHomeStopResync) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->stepperHomeResyncSteps = 5;
	auto& iac = initStepDirStepperForExtTest(100);

	applyIACposition(10);
	iterate(iac, 10);
	EXPECT_EQ(10, iac.m_currentPosition);

	// commanded past the home (closed) stop: travel down to 0 normally...
	applyIACposition(-20);
	iterate(iac, 10);
	EXPECT_EQ(0, iac.m_currentPosition);

	// ...then 5 extra steps into the stop while the counter stays at 0
	for (int i = 0; i < 5; i++) {
		iac.doIteration();
		EXPECT_EQ(0, iac.m_currentPosition);
		EXPECT_TRUE(isIdleMotorBusy()) << "resync step " << i;
	}

	// resync done: held at 0 even though the target is -20, no more hammering
	iterate(iac, 10);
	EXPECT_EQ(0, iac.m_currentPosition);
	EXPECT_FALSE(isIdleMotorBusy());

	// opening again re-arms the resync for the next home command
	applyIACposition(3);
	iterate(iac, 3);
	EXPECT_EQ(3, iac.m_currentPosition);
}

TEST(Actuators, StepperHomeStopResyncDisabledKeepsLegacyNegativeTravel) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->stepperHomeResyncSteps = 0;
	auto& iac = initStepDirStepperForExtTest(100);

	applyIACposition(-5);
	iterate(iac, 10);
	EXPECT_EQ(-5, iac.m_currentPosition);
}

TEST(Actuators, StepperHeldOffIdleIgnoresNewTarget) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	auto& iac = initStepDirStepperForExtTest(200);

	applyIACposition(25);
	EXPECT_FLOAT_EQ(50, iac.getTargetPosition());

	auto& idle = engine->module<IdleController>().unmock();
	idle.isStepperHeld = true;
	applyIACposition(80);
	EXPECT_FLOAT_EQ(50, iac.getTargetPosition());

	idle.isStepperHeld = false;
	applyIACposition(80);
	EXPECT_FLOAT_EQ(160, iac.getTargetPosition());
}

TEST(Actuators, StepDirDriverEnableHold) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// default: driver stays enabled after a step
	EXPECT_TRUE(getCustomPage()->stepperKeepDriverEnabled);
	auto& iac = initStepDirStepperForExtTest(200);
	EXPECT_FALSE(isIdleStepDirDriverEnabledForUnitTest());
	applyIACposition(10);
	iac.doIteration();
	EXPECT_EQ(1, iac.m_currentPosition);
	EXPECT_TRUE(isIdleStepDirDriverEnabledForUnitTest());

	// ignition voltage lost: driver released
	Sensor::setMockValue(SensorType::BatteryVoltage, 0);
	iac.doIteration();
	EXPECT_FALSE(isIdleStepDirDriverEnabledForUnitTest());
}

TEST(Actuators, StepDirDriverLegacyReleaseAfterEachStep) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->stepperKeepDriverEnabled = false;
	auto& iac = initStepDirStepperForExtTest(200);

	applyIACposition(10);
	iac.doIteration();
	EXPECT_EQ(1, iac.m_currentPosition);
	EXPECT_FALSE(isIdleStepDirDriverEnabledForUnitTest());
}

// Parking must end on coil phase 0, so position 0 == phase 0 and a restored position can restore
// the phase. With 5 total steps the 5 parking steps end on phase 3; 3 more align it.
TEST(Actuators, HbridgeStepperParkingEndsOnPhaseZero) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	Sensor::setMockValue(SensorType::BatteryVoltage, 12);

	engineConfiguration->useStepperIdle = true;
	engineConfiguration->useHbridgesToDriveIdleStepper = true;
	engineConfiguration->idleStepperTotalSteps = 5;
	engineConfiguration->stepperNumMicroSteps = STEPPER_FULL;
	engineConfiguration->stepperForceParkingEveryRestart = true;
	engineConfiguration->stepperParkingExtraSteps = 0;

	EXPECT_NO_FATAL_ERROR(initIdleHardware());
	auto& iac = getIacMotorForUnitTest();

	iac.doIteration();
	EXPECT_EQ(0, iac.m_currentPosition);
	expectHbridgePhase(0);

	applyIACposition(100);
	for (int step = 1; step <= 5; step++) {
		iac.doIteration();
		EXPECT_EQ(step, iac.m_currentPosition);
		expectHbridgePhase(step);
	}
}

TEST(Actuators, HbridgeStepperRestorePhase) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	DualHBridgeStepper hb;

	engineConfiguration->stepperNumMicroSteps = STEPPER_FULL;
	hb.restorePhase(8);
	EXPECT_TRUE(hb.isAtPhaseZero());
	hb.restorePhase(5);
	EXPECT_FALSE(hb.isAtPhaseZero());
	hb.restorePhase(-4);
	EXPECT_TRUE(hb.isAtPhaseZero());
	hb.restorePhase(-3);
	EXPECT_FALSE(hb.isAtPhaseZero());

	// micro-stepping: one step = 8 entries of the 32-entry table, so 4 steps per cycle as well
	engineConfiguration->stepperNumMicroSteps = STEPPER_FOURTH;
	hb.restorePhase(4);
	EXPECT_TRUE(hb.isAtPhaseZero());
	hb.restorePhase(2);
	EXPECT_FALSE(hb.isAtPhaseZero());
}

TEST(Actuators, StepperMinMoveDeadband) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->stepperMinMoveSteps = 4;
	auto& iac = initStepDirStepperForExtTest(400);

	applyIACposition(10);
	EXPECT_FLOAT_EQ(40, iac.getTargetPosition());

	// +/-0.5% of 400 steps = 2 steps of jitter: ignored
	applyIACposition(10.5f);
	EXPECT_FLOAT_EQ(40, iac.getTargetPosition());
	applyIACposition(9.5f);
	EXPECT_FLOAT_EQ(40, iac.getTargetPosition());

	// a real change of 4+ steps goes through
	applyIACposition(11);
	EXPECT_FLOAT_EQ(44, iac.getTargetPosition());

	// fully closing always goes through, even when within the deadband
	applyIACposition(0.5f);
	EXPECT_FLOAT_EQ(2, iac.getTargetPosition());
	applyIACposition(0);
	EXPECT_FLOAT_EQ(0, iac.getTargetPosition());
}

TEST(Actuators, StepperMinMoveDisabledKeepsOneStepThreshold) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->stepperMinMoveSteps = 0;
	auto& iac = initStepDirStepperForExtTest(400);

	applyIACposition(10);
	applyIACposition(10.5f);
	EXPECT_FLOAT_EQ(42, iac.getTargetPosition());
}

// Flip stepper direction: step 0 (parking/home end) is the maximum-air end, 100% duty still
// means maximum air. For pintle valves whose only safe stop is fully retracted.
TEST(Actuators, StepperFlipDirectionMapsDutyToSteps) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	auto& iac = initStepDirStepperForExtTest(400);

	// flip off: legacy mapping
	getCustomPage()->stepperFlipDirection = false;
	applyIACposition(25);
	EXPECT_FLOAT_EQ(100, iac.getTargetPosition());

	getCustomPage()->stepperFlipDirection = true;
	applyIACposition(25);
	EXPECT_FLOAT_EQ(300, iac.getTargetPosition());
	applyIACposition(100);
	EXPECT_FLOAT_EQ(0, iac.getTargetPosition());
	applyIACposition(0);
	EXPECT_FLOAT_EQ(400, iac.getTargetPosition());
}

TEST(Actuators, StepperFlipDirectionHomeResyncAtFullAir) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->stepperFlipDirection = true;
	getCustomPage()->stepperHomeResyncSteps = 3;
	auto& iac = initStepDirStepperForExtTest(100);

	applyIACposition(90);
	iterate(iac, 10);
	EXPECT_EQ(10, iac.m_currentPosition);

	// 110% (overtravel) -> step -10: travel to the home stop, resync there, then hold at 0
	applyIACposition(110);
	iterate(iac, 10);
	EXPECT_EQ(0, iac.m_currentPosition);
	iterate(iac, 3);
	EXPECT_TRUE(isIdleMotorBusy());
	iterate(iac, 5);
	EXPECT_EQ(0, iac.m_currentPosition);
	EXPECT_FALSE(isIdleMotorBusy());
}

TEST(Actuators, StepperFlipDirectionNeverPastExtendedEnd) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->stepperFlipDirection = true;
	auto& iac = initStepDirStepperForExtTest(10);

	// -50% would be step 15: the stepper layer stops at totalSteps regardless
	applyIACposition(-50);
	EXPECT_FLOAT_EQ(15, iac.getTargetPosition());
	iterate(iac, 30);
	EXPECT_EQ(10, iac.m_currentPosition);
	EXPECT_FALSE(isIdleMotorBusy());
}
