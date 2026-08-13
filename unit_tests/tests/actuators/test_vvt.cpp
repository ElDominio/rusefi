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

TEST(Vvt, observePlant) {
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
// (pid_s.offset) is never used while Advanced Mode is enabled. The classic P+I+D trim is never
// disabled either: it fades in from zero authority at distance=0 to full ("normal") authority at
// vvtAdvancedPidFadeDeg, and stays at full authority beyond it. vvtAdvancedPidFadeDeg only
// controls this PID fade-in -- it does not change which feedforward source is used.

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
	d->vvtAdvancedPidFadeDeg = 1.0f;
	setupAdvancedIntakeDistanceCurve(d);
	d->vvtAdvDutyIntake[4] = 5.0f;  // center point (distance = 0)
	d->vvtAdvDutyIntake[5] = 30.0f; // bin index 5 = +10 deg

	// distance = 0 (within the fade distance) -> feedforward is still the curve's center value.
	EXPECT_NEAR(dut.getClosedLoop(20, 20).value_or(0), 5.0f, 0.01f);
	// distance = 10 (well beyond the fade distance) -> same curve, different point.
	EXPECT_NEAR(dut.getClosedLoop(30, 20).value_or(0), 30.0f, 0.01f);
}

TEST(Vvt, AdvancedModePidFadeHalvesAtHalfDegree) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);
	engineConfiguration->auxPid[camIndex].pFactor = 1.0f;
	engineConfiguration->auxPid[camIndex].offset = 999.0f; // Hold Duty -- must never leak in

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	d->vvtAdvancedPidFadeDeg = 1.0f;
	setupAdvancedIntakeDistanceCurve(d); // zero duty curve everywhere

	// distance = 0.5 deg -> pidScale = 0.5/1.0 = 0.5 -> half of the raw P term (0.5) on top of a
	// zero feedforward.
	EXPECT_NEAR(dut.getClosedLoop(0.5, 0).value_or(0), 0.25f, 0.001f);
}

TEST(Vvt, AdvancedModePidStaysAtFullAuthorityBeyondFadeDistance) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);
	engineConfiguration->auxPid[camIndex].pFactor = 1.0f;
	engineConfiguration->auxPid[camIndex].offset = 999.0f; // Hold Duty -- must never leak in

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	d->vvtAdvancedPidFadeDeg = 1.0f;
	setupAdvancedIntakeDistanceCurve(d); // zero duty curve everywhere

	// distance = 10 deg, far beyond the 1.0 deg fade distance -> pidScale clamps at 1.0 (full,
	// "normal" PID authority -- never disabled), feedforward is still the (zero) duty curve.
	EXPECT_NEAR(dut.getClosedLoop(30, 20).value_or(0), 10.0f, 0.01f);
}

TEST(Vvt, AdvancedModeOilPressureSensorAbsentIsNeutral) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VvtController dut(0);
	int camIndex = 0;
	dut.init(nullptr, nullptr);
	setupAdvancedIntakePid(camIndex);

	auto* d = getCustomPage();
	d->vvtAdvancedModeEnabled = true;
	d->vvtAdvancedPidFadeDeg = 1.0f;
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
	d->vvtAdvancedPidFadeDeg = 1.0f;
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
	d->vvtAdvancedPidFadeDeg = 1.0f;
	setupAdvancedIntakeDistanceCurve(d);
	dut.getClosedLoop(30, 20);
	EXPECT_NEAR(dut.vvtDistance, 10.0f, 0.5f); // vvtDistance is a scaled int16 (0.1 deg resolution)
}
