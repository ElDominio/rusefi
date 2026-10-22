/*
 * test_single_tooth_cam_filter.cpp
 *
 * Single Tooth cam filter [tag:single_tooth_cam_filter]: once engine phase is synchronized,
 * single tooth cam teeth further than singleToothCamFilter*Pct of the engine cycle from the last
 * accepted tooth are ignored. See singleToothCamFilterRejects() in trigger_central.cpp.
 */

#include "pch.h"
#include "custom_page.h"

// Same geometry as trigger.testCamInput: one crank tooth per revolution at 1200 RPM, so one
// revolution is 50 ms = 360 deg, i.e. 1 ms = 7.2 crank deg.
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
	return engine->triggerCentral.singleToothCamFilter[0].rejectCounter;
}

TEST(singleToothCamFilter, disabledByDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	EXPECT_EQ(0, getCustomPage()->singleToothCamFilterIntakePct);
	EXPECT_EQ(0, getCustomPage()->singleToothCamFilterExhaustPct);
}

TEST(singleToothCamFilter, rejectsNoiseOutsideWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSingleToothCam(eth);
	getCustomPage()->singleToothCamFilterIntakePct = 10; // +/-72 deg

	spinCycles(eth, 10);
	ASSERT_TRUE(engine->triggerCentral.triggerState.hasSynchronizedPhase());
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));
	EXPECT_EQ(0u, rejectCount());
	int syncCounter = engine->triggerCentral.triggerState.getSynchronizationCounter();

	// noise tooth exactly one revolution (360 deg) away from the real one: without the filter
	// this would flip engine phase
	spinRevolution(eth, 10);
	spinRevolution(eth, 10);
	EXPECT_EQ(1u, rejectCount());

	// noise 12 ms = 86.4 deg after the real tooth, in the same revolution
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

TEST(singleToothCamFilter, noiseFlipsPhaseWithoutFilter) {
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

TEST(singleToothCamFilter, followsCamMovementInsideWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSingleToothCam(eth);
	getCustomPage()->singleToothCamFilterIntakePct = 5; // +/-36 deg

	spinCycles(eth, 10);
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));

	// cam phaser moves 3 ms = 21.6 deg per cycle, inside the window each time
	spinCycles(eth, 1, 13);
	spinCycles(eth, 1, 16);
	EXPECT_EQ(0u, rejectCount());
	EXPECT_NEAR(71 - 2 * 21.6, engine->triggerCentral.getVVTPosition(0, 0), EPS2D);
}

TEST(singleToothCamFilter, reseedsAfterStaleReference) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSingleToothCam(eth);
	getCustomPage()->singleToothCamFilterIntakePct = 10; // +/-72 deg

	spinCycles(eth, 10);
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));

	// cam suddenly lands 12 ms = 86.4 deg later, outside the window: rejected until no tooth has
	// been accepted for 2.5 engine cycles (250 ms), then the reference re-seeds on it
	spinCycles(eth, 2, 22);
	EXPECT_EQ(2u, rejectCount());
	EXPECT_NEAR_M3(71, engine->triggerCentral.getVVTPosition(0, 0));

	spinCycles(eth, 3, 22);
	EXPECT_EQ(2u, rejectCount());
	EXPECT_NEAR(71 - 86.4, engine->triggerCentral.getVVTPosition(0, 0), EPS2D);
}
