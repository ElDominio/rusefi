#include "pch.h"

#include "vvt.h"
#include "custom_page.h"

using ::testing::StrictMock;
using ::testing::Return;

TEST(Vvt, TestSetPoint) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Set up a mock target map & pwm output
	StrictMock<MockVp3d> targetMap;
	StrictMock<MockPwm> pwm;

	EXPECT_CALL(targetMap, getValue(1500, 55))
		.WillRepeatedly(Return(20)); // one from onFastCallback() then getSetpoint()
	EXPECT_CALL(pwm, setSimplePwmDutyCycle(0.730005));

	// set up VVT config
	engineConfiguration->vvtActivationDelayMs = 5;
	engineConfiguration->vvtControlMinRpm = 500;

	// mock RPM
	engine->rpmCalculator.setRpmValue(1500);
	ASSERT_EQ(1500, Sensor::getOrZero(SensorType::Rpm));
	ASSERT_TRUE(engine->rpmCalculator.isRunning());
	advanceTimeUs(0.5e6);

	VvtController dut(0);
	dut.init(&targetMap, &pwm);

	// Mock necessary inputs
	engine->engineState.fuelingLoad = 55;

	// update m_engineRunningLongEnough / m_isRpmHighEnough flags
	dut.onFastCallback();
	// Test dut
	EXPECT_EQ(20, dut.getSetpoint().value_or(0));
}

struct FakeMap : public ValueProvider3D {
	float setpoint = 0;

	float getValue(float xColumn, float yRow) const override {
		return setpoint;
	}
};

TEST(VVT, SetpointHysteresisAdvancingCam) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	Sensor::setMockValue(SensorType::Clt, 70); // m_isCltWarmEnough
	// m_isRpmHighEnough
	engine->rpmCalculator.setRpmValue(1500);
	engineConfiguration->vvtControlMinRpm = 500;
	// m_engineRunningLongEnough
	advanceTimeUs(0.8e6);
	engineConfiguration->vvtActivationDelayMs = 5;
	engineConfiguration->invertVvtControlIntake = false;
	engineConfiguration->invertVvtControlExhaust = false;

	FakeMap targetMap;
	MockPwm pwm;

	VvtController dut(0);
	dut.init(&targetMap, &pwm);

	// update m_engineRunningLongEnough / m_isRpmHighEnough flags
	dut.onFastCallback();

	// 0 position returns unexpected
	targetMap.setpoint = 0;
	EXPECT_EQ(-1000, dut.getSetpoint().value_or(-1000));

	// Between hysteresis still return unexpected
	targetMap.setpoint = 2;
	EXPECT_EQ(-1000, dut.getSetpoint().value_or(-1000));

	// Above hysteresis returns real value
	targetMap.setpoint = 5;
	EXPECT_EQ(5, dut.getSetpoint().value_or(-1000));

	// Between hysteresis still returns valid
	targetMap.setpoint = 2;
	EXPECT_EQ(2, dut.getSetpoint().value_or(-1000));

	// Back under hysteresis retuns unexpected again
	targetMap.setpoint = 0.5;
	EXPECT_EQ(-1000, dut.getSetpoint().value_or(-1000));
}

TEST(VVT, SetpointHysteresisRetardingCam) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	Sensor::setMockValue(SensorType::Clt, 70); // m_isCltWarmEnough
	// m_isRpmHighEnough
	engine->rpmCalculator.setRpmValue(1500);
	engineConfiguration->vvtControlMinRpm = 500;
	// m_engineRunningLongEnough
	advanceTimeUs(0.8e6);
	engineConfiguration->vvtActivationDelayMs = 5;

	engineConfiguration->invertVvtControlIntake = true;
	engineConfiguration->invertVvtControlExhaust = false;

	FakeMap targetMap;
	MockPwm pwm;

	VvtController dut(0);
	dut.init(&targetMap, &pwm);

	// update m_engineRunningLongEnough / m_isRpmHighEnough flags
	dut.onFastCallback();

	// 0 position returns unexpected
	targetMap.setpoint = 0;
	EXPECT_EQ(-1000, dut.getSetpoint().value_or(-1000));

	// Between hysteresis still return unexpected
	targetMap.setpoint = -2;
	EXPECT_EQ(-1000, dut.getSetpoint().value_or(-1000));

	// Above hysteresis returns real value
	targetMap.setpoint = -5;
	EXPECT_EQ(-5, dut.getSetpoint().value_or(-1000));

	// Between hysteresis still returns valid
	targetMap.setpoint = -2;
	EXPECT_EQ(-2, dut.getSetpoint().value_or(-1000));

	// Back under hysteresis retuns unexpected again
	targetMap.setpoint = -0.5;
	EXPECT_EQ(-1000, dut.getSetpoint().value_or(-1000));
}

TEST(VVT, ObservePlant) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engine->triggerCentral.vvtPosition[0][0] = 23;

	VvtController dut(0);
	dut.init(nullptr, nullptr);

	EXPECT_EQ(23, dut.observePlant().value_or(0));
}

TEST(Vvt, openLoop) {
	VvtController dut(0);

	// No open loop for now
	EXPECT_EQ(dut.getOpenLoop(10), 0);
}

TEST(Vvt, ClosedLoopNotInverted) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(/* second cam on second bank*/3);
	int camIndex = 1;
	ASSERT_EQ(dut.getCamIndex(), camIndex);
	dut.init(nullptr, nullptr);

	engineConfiguration->auxPid[camIndex].pFactor = 1.5f;
	engineConfiguration->auxPid[camIndex].iFactor = 0;
	engineConfiguration->auxPid[camIndex].dFactor = 0;
	engineConfiguration->auxPid[camIndex].offset = 0;

	// Target of 30 with position 20 should yield positive duty, P=1.5 means 15% duty for 10% error
	EXPECT_EQ(dut.getClosedLoop(30, 20).value_or(0), 15);
}

TEST(Vvt, ClosedLoopInverted) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(/*first cam on second bank*/2);
	int camIndex = 0;
	ASSERT_EQ(dut.getCamIndex(), camIndex);
	dut.init(nullptr, nullptr);

	engineConfiguration->invertVvtControlIntake = true;
	engineConfiguration->auxPid[camIndex].pFactor = 1.5f;
	engineConfiguration->auxPid[camIndex].iFactor = 0;
	engineConfiguration->auxPid[camIndex].dFactor = 0;
	engineConfiguration->auxPid[camIndex].offset = 0;

	// Target of -30 with position -20 should yield positive duty, P=1.5 means 15% duty for 10% error
	EXPECT_EQ(dut.getClosedLoop(-30, -20).value_or(0), 15);
}

// VVT Advanced Mode: the feedforward baseline is always the distance-from-target duty curve
// (scaled by an optional oil-pressure multiplier), at every distance -- the fixed "Hold Duty"
// (pid_s.offset) is never used while Advanced Mode is enabled. Within vvtAdvancedPidPauseDeg of
// the target, the classic P+I+D trim is fully paused (not evaluated at all, so its iTerm cannot
// integrate in the background) and only the duty curve is used; outside that window (or with
// vvtAdvancedPidPauseEnabled off) the trim runs at full authority.

static void setupAdvancedIntakePid(int camIndex) {
	engineConfiguration->auxPid[camIndex].pFactor = 0;
	engineConfiguration->auxPid[camIndex].iFactor = 0;
	engineConfiguration->auxPid[camIndex].dFactor = 0;
	engineConfiguration->auxPid[camIndex].offset = 0;
	engineConfiguration->auxPid[camIndex].periodMs = 10;
	engineConfiguration->auxPid[camIndex].minValue = -1000;
	engineConfiguration->auxPid[camIndex].maxValue = 1000;
}

// Symmetric -40..40 deg distance axis (9 points, zero at the center -- matches the real bins)
// with all-zero duty by default. Bin index 5 sits at +10 deg.
static void setupAdvancedIntakeDistanceCurve(page6_s* d) {
	for (size_t i = 0; i < efi::size(d->vvtAdvDistanceBinsIntake); i++) {
		d->vvtAdvDistanceBinsIntake[i] = -40.0f + i * 10.0f;
		d->vvtAdvDutyIntake[i] = 0;
	}
}

TEST(Vvt, AdvancedModeDisabledMatchesLegacyBehavior) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);

	getCustomPage()->vvtAdvancedModeEnabled = false;

	engineConfiguration->auxPid[camIndex].pFactor = 1.5f;
	engineConfiguration->auxPid[camIndex].iFactor = 0;
	engineConfiguration->auxPid[camIndex].dFactor = 0;
	engineConfiguration->auxPid[camIndex].offset = 0;

	// Same as ClosedLoopNotInverted: P=1.5 means 15% duty for 10% error.
	EXPECT_EQ(dut.getClosedLoop(30, 20).value_or(0), 15);
}

TEST(Vvt, AdvancedModeHoldDutyNeverUsed) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);
	engineConfiguration->auxPid[camIndex].offset = 999.0f; // Hold Duty -- must never leak in

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	setupAdvancedIntakeDistanceCurve(d);
	d->vvtAdvDutyIntake[4] = 5.0f;  // center point (distance = 0)
	d->vvtAdvDutyIntake[5] = 30.0f; // bin index 5 = +10 deg

	// distance = 0 -> feedforward is the curve's center value.
	EXPECT_NEAR(dut.getClosedLoop(20, 20).value_or(0), 5.0f, 0.01f);
	// distance = 10 -> same curve, different point.
	EXPECT_NEAR(dut.getClosedLoop(30, 20).value_or(0), 30.0f, 0.01f);
}

TEST(Vvt, AdvancedModePidPausedWithinWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);
	engineConfiguration->auxPid[camIndex].pFactor = 1.0f;
	engineConfiguration->auxPid[camIndex].offset = 999.0f; // Hold Duty -- must never leak in

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	d->vvtAdvancedPidPauseEnabled = true;
	d->vvtAdvancedPidPauseDeg = 1.0f;
	setupAdvancedIntakeDistanceCurve(d); // zero duty curve everywhere

	// distance = 0.5 deg, inside the 1.0 deg pause window -> PID is not evaluated at all, output
	// is the (zero) feedforward alone -- a naive "PID always on" calc would give 0.5 here (P=1.0).
	EXPECT_NEAR(dut.getClosedLoop(0.5, 0).value_or(0), 0.0f, 0.001f);
}

TEST(Vvt, AdvancedModePidActiveOutsidePauseWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);
	engineConfiguration->auxPid[camIndex].pFactor = 1.0f;
	engineConfiguration->auxPid[camIndex].offset = 999.0f; // Hold Duty -- must never leak in

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	d->vvtAdvancedPidPauseEnabled = true;
	d->vvtAdvancedPidPauseDeg = 1.0f;
	setupAdvancedIntakeDistanceCurve(d); // zero duty curve everywhere

	// distance = 10 deg, well outside the 1.0 deg pause window -> PID runs at full authority,
	// feedforward is still the (zero) duty curve.
	EXPECT_NEAR(dut.getClosedLoop(30, 20).value_or(0), 10.0f, 0.01f);
}

TEST(Vvt, AdvancedModePauseDisabledKeepsPidActiveNearTarget) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);
	engineConfiguration->auxPid[camIndex].pFactor = 1.0f;
	engineConfiguration->auxPid[camIndex].offset = 999.0f; // Hold Duty -- must never leak in

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	d->vvtAdvancedPidPauseEnabled = false; // pausing turned off entirely
	d->vvtAdvancedPidPauseDeg = 1.0f;      // would normally cover this distance -- ignored
	setupAdvancedIntakeDistanceCurve(d); // zero duty curve everywhere

	// distance = 0.5 deg, but pausing is disabled -> PID still runs at full authority.
	EXPECT_NEAR(dut.getClosedLoop(0.5, 0).value_or(0), 0.5f, 0.001f);
}

TEST(Vvt, AdvancedModePauseFreezesIntegrator) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);
	engineConfiguration->auxPid[camIndex].iFactor = 1.0f;

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	d->vvtAdvancedPidPauseEnabled = true;
	d->vvtAdvancedPidPauseDeg = 1.0f;
	setupAdvancedIntakeDistanceCurve(d); // zero duty curve everywhere

	// While paused, the PID is never evaluated, so iTerm cannot quietly accumulate: many cycles
	// inside the window all read back as the flat (zero) feedforward.
	for (int i = 0; i < 50; i++) {
		EXPECT_NEAR(dut.getClosedLoop(0.5, 0).value_or(0), 0.0f, 0.001f);
	}

	// Leaving the window now integrates for a single fresh cycle (dTime * iFactor * error), not
	// 50 cycles' worth of background windup -- if iTerm had kept integrating while paused, this
	// would be far larger than one cycle's contribution.
	float dTime = MS2SEC(engineConfiguration->auxPid[camIndex].periodMs);
	float expectedFreshITerm = engineConfiguration->auxPid[camIndex].iFactor * dTime * 1.5f; // error = 1.5 at distance=1.5
	EXPECT_NEAR(dut.getClosedLoop(1.5, 0).value_or(0), expectedFreshITerm, 0.0005f);
}

TEST(Vvt, AdvancedModeOilPressureSensorAbsentIsNeutral) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	setupAdvancedIntakeDistanceCurve(d);
	d->vvtAdvDutyIntake[5] = 10.0f; // bin index 5 = +10 deg
	// Oil pressure multiplier curve is non-neutral, but no sensor is mocked -> must be ignored.
	for (size_t i = 0; i < efi::size(d->vvtAdvOilPressureBinsIntake); i++) {
		d->vvtAdvOilPressureBinsIntake[i] = i * 100.0f;
		d->vvtAdvOilPressureMultIntake[i] = 5.0f;
	}
	Sensor::resetMockValue(SensorType::OilPressure);

	EXPECT_NEAR(dut.getClosedLoop(30, 20).value_or(0), 10.0f, 0.01f);
}

TEST(Vvt, AdvancedModeOilPressureMultiplierApplied) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	setupAdvancedIntakeDistanceCurve(d);
	d->vvtAdvDutyIntake[5] = 10.0f; // bin index 5 = +10 deg, flat duty
	for (size_t i = 0; i < efi::size(d->vvtAdvOilPressureBinsIntake); i++) {
		d->vvtAdvOilPressureBinsIntake[i] = i * 100.0f; // 0..500 kPa (6 points)
		d->vvtAdvOilPressureMultIntake[i] = 1.0f;
	}
	d->vvtAdvOilPressureMultIntake[3] = 2.0f; // 300 kPa -> 2x multiplier

	Sensor::setMockValue(SensorType::OilPressure, 300);

	EXPECT_NEAR(dut.getClosedLoop(30, 20).value_or(0), 20.0f, 0.01f);
}

TEST(Vvt, DistanceLiveDataOnlyTrackedInAdvancedMode) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);

	auto* d = getCustomPage();

	// Basic mode: distance tracker is not written.
	d->vvtAdvancedModeEnabled = false;
	dut.vvtDistance = 0;
	dut.getClosedLoop(30, 20);
	EXPECT_EQ(dut.vvtDistance, 0);

	// Advanced mode: distance tracker reflects target - observation.
	d->vvtAdvancedModeEnabled = true;
	setupAdvancedIntakeDistanceCurve(d);
	dut.getClosedLoop(30, 20);
	EXPECT_NEAR(dut.vvtDistance, 10.0f, 0.5f); // vvtDistance is a scaled int16 (0.1 deg resolution)
}
