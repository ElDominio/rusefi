/*
 * test_vvt_angle_window.cpp
 *
 * VVT angle window [tag:vvt_angle_window]: once engine phase is synchronized, cam sync teeth whose
 * VVT angle is outside vvtAngleWindow*Min/Max are ignored, with a recovery after
 * vvtAngleWindowRecoveryCycles engine cycles without an in-window tooth. See
 * vvtAngleWindowRejects() in trigger_central.cpp.
 */

#include "pch.h"
#include "custom_page.h"

// Same geometry as trigger.testCamInput: one crank tooth per revolution at 1200 RPM, so one
// revolution is 50 ms = 360 deg, i.e. 1 ms = 7.2 crank deg. A real cam tooth 10 ms into the
// revolution reads 71 deg on the VVT gauge.
static void setupSingleToothCam(EngineTestHelper& eth) {
	engineConfiguration->globalTriggerAngleOffset = -37;
	engineConfiguration->crankingTimingAngle = 3;
	engineConfiguration->triggerInputPins[1] = Gpio::Unassigned;

	setCrankOperationMode();
	engineConfiguration->vvtMode[0] = VVT_SINGLE_TOOTH;
	engineConfiguration->vvtOffsets[0] = 360;
	eth.setTriggerType(trigger_type_e::TT_HALF_MOON);
	engineConfiguration->camInputs[0] = Gpio::A10;

	for (int i = 0; i < 5; i++) {
		eth.fireRise(25);
		eth.fireFall(25);
	}
	ASSERT_EQ(1200, round(Sensor::getOrZero(SensorType::Rpm)));
}

static void enableIntakeWindow(float minAngle, float maxAngle) {
	auto page = getCustomPage();
	page->vvtAngleWindowEnabled = true;
	page->vvtAngleWindowIntakeMin = minAngle;
	page->vvtAngleWindowIntakeMax = maxAngle;
}

/**
 * One crank revolution: cam rise edges at the given ms offsets (negative = none, must be < 25 and
 * in ascending order), then the crank rise at 25 ms and the crank fall at 50 ms.
 */
static void spinRevolution(EngineTestHelper& eth, float firstCamMs, float secondCamMs = -1) {
	float nowMs = 0;
	for (float camMs : { firstCamMs, secondCamMs }) {
		if (camMs < 0) {
			continue;
		}
		eth.moveTimeForwardUs(MS2US(camMs - nowMs));
		nowMs = camMs;
		hwHandleVvtCamSignal(TriggerValue::RISE, getTimeNowNt(), 0);
	}
	eth.moveTimeForwardUs(MS2US(25 - nowMs));
	eth.firePrimaryTriggerRise();
	eth.moveTimeForwardUs(MS2US(25));
	eth.firePrimaryTriggerFall();
}

// real cam tooth every other revolution, 10 ms in
static void spinCycles(EngineTestHelper& eth, int cycles, float camMs = 10) {
	for (int i = 0; i < cycles; i++) {
		spinRevolution(eth, camMs);
		spinRevolution(eth, -1);
	}
}

static uint32_t rejectCount() {
	return engine->triggerCentral.vvtAngleWindow[0].rejectCounter;
}

TEST(vvtAngleWindow, disabledByDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	EXPECT_FALSE(getCustomPage()->vvtAngleWindowEnabled);
	EXPECT_EQ(3, getCustomPage()->vvtAngleWindowRecoveryCycles);
}

TEST(vvtAngleWindow, rejectsNoiseOutsideWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSingleToothCam(eth);
	enableIntakeWindow(40, 100);

	spinCycles(eth, 10);
	ASSERT_TRUE(engine->triggerCentral.triggerState.hasSynchronizedPhase());
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));
	EXPECT_EQ(0u, rejectCount());
	int syncCounter = engine->triggerCentral.triggerState.getSynchronizationCounter();

	// noise tooth exactly one revolution (360 deg) away from the real one: without the window
	// this would flip engine phase
	spinRevolution(eth, 10);
	spinRevolution(eth, 10);
	EXPECT_EQ(1u, rejectCount());

	// noise 12 ms = 86.4 deg after the real tooth, in the same revolution: reads -15.4 deg
	spinRevolution(eth, 10, 22);
	spinRevolution(eth, -1);
	EXPECT_EQ(2u, rejectCount());

	spinCycles(eth, 3);
	EXPECT_EQ(2u, rejectCount());
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));
	EXPECT_TRUE(engine->triggerCentral.triggerState.hasSynchronizedPhase());
	// no phase flip: exactly one sync point per revolution, 10 revolutions since syncCounter
	EXPECT_EQ(syncCounter + 10, engine->triggerCentral.triggerState.getSynchronizationCounter());
}

TEST(vvtAngleWindow, noiseFlipsPhaseWithoutWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSingleToothCam(eth);

	spinCycles(eth, 10);
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));
	int syncCounter = engine->triggerCentral.triggerState.getSynchronizationCounter();

	// same 360 deg noise tooth as above, but nothing filters it
	spinRevolution(eth, 10);
	spinRevolution(eth, 10);
	EXPECT_EQ(0u, rejectCount());
	EXPECT_NE(syncCounter + 2, engine->triggerCentral.triggerState.getSynchronizationCounter());
}

TEST(vvtAngleWindow, followsCamMovementInsideWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSingleToothCam(eth);
	enableIntakeWindow(20, 100);

	spinCycles(eth, 10);
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));

	// cam phaser moves 3 ms = 21.6 deg per cycle, still inside the window
	spinCycles(eth, 1, 13);
	spinCycles(eth, 1, 16);
	EXPECT_EQ(0u, rejectCount());
	EXPECT_NEAR(71 - 2 * 21.6, engine->triggerCentral.getVVTPosition(0, 0), EPS2D);

	// another 21.6 deg would read 6.2 deg: below min
	spinCycles(eth, 1, 19);
	EXPECT_EQ(1u, rejectCount());
	EXPECT_NEAR(71 - 2 * 21.6, engine->triggerCentral.getVVTPosition(0, 0), EPS2D);
}

TEST(vvtAngleWindow, recoversAfterConfiguredCycles) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSingleToothCam(eth);
	enableIntakeWindow(40, 100);
	getCustomPage()->vvtAngleWindowRecoveryCycles = 3;

	spinCycles(eth, 10);
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));

	// cam suddenly reads -15.4 deg, outside the window: rejected for 3 engine cycles...
	spinCycles(eth, 3, 22);
	EXPECT_EQ(3u, rejectCount());
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));

	// ...then accepted
	spinCycles(eth, 1, 22);
	EXPECT_EQ(3u, rejectCount());
	EXPECT_NEAR(71 - 86.4, engine->triggerCentral.getVVTPosition(0, 0), EPS2D);
}

TEST(vvtAngleWindow, noiseBetweenGoodTeethDoesNotTriggerRecovery) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSingleToothCam(eth);
	enableIntakeWindow(40, 100);
	getCustomPage()->vvtAngleWindowRecoveryCycles = 1;

	spinCycles(eth, 10);
	int syncCounter = engine->triggerCentral.triggerState.getSynchronizationCounter();

	// three noise pulses per cycle around one good tooth, for many cycles
	for (int i = 0; i < 10; i++) {
		spinRevolution(eth, 10, 22);
		spinRevolution(eth, 5, 20);
	}
	EXPECT_EQ(30u, rejectCount());
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));
	EXPECT_EQ(syncCounter + 20, engine->triggerCentral.triggerState.getSynchronizationCounter());
}

TEST(vvtAngleWindow, wrongPhaseRecovers) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSingleToothCam(eth);

	spinCycles(eth, 10);
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));

	int syncCounter = engine->triggerCentral.triggerState.getSynchronizationCounter();
	// noise tooth 360 deg away flips engine phase while the window is still disabled, so from now
	// on the real tooth (still in its original revolution) reads ~360 deg off
	spinRevolution(eth, 10);
	spinRevolution(eth, 10);
	ASSERT_NE(syncCounter + 2, engine->triggerCentral.triggerState.getSynchronizationCounter());

	enableIntakeWindow(40, 100);
	getCustomPage()->vvtAngleWindowRecoveryCycles = 3;

	// real teeth now read ~360 deg off and are rejected...
	spinCycles(eth, 3);
	EXPECT_EQ(3u, rejectCount());

	// ...until recovery lets one through, which resyncs phase. The last accepted tooth (the noise)
	// is exactly half a cycle before the real ones, so the 4th real tooth sits right on the
	// 3.5-cycle threshold and may or may not be rejected - by the 5th it is certainly accepted.
	spinCycles(eth, 2);
	uint32_t rejectsUntilRecovery = rejectCount();
	EXPECT_LE(rejectsUntilRecovery, 4u);
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));

	// and the window holds from there
	spinCycles(eth, 5);
	EXPECT_EQ(rejectsUntilRecovery, rejectCount());
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));
	EXPECT_TRUE(engine->triggerCentral.triggerState.hasSynchronizedPhase());
}
