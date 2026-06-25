#include "pch.h"

TEST(engine, testRpmAcceleration) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	eth.setTriggerType(trigger_type_e::TT_HALF_MOON);

	// first revolution
	eth.smartFireTriggerEvents2(/*count*/1, /*delay*/ 40);
    ASSERT_EQ(0u,  engine->triggerCentral.triggerState.totalTriggerErrorCounter);
	ASSERT_EQ(1500, Sensor::getOrZero(SensorType::Rpm));
	// no acceleration info YET, huh?
	ASSERT_EQ(0, engine->rpmCalculator.getRpmAcceleration());

	// second revolution same speed
	eth.smartFireTriggerEvents2(/*count*/1, /*delay*/ 40);
	ASSERT_EQ(0u,  engine->triggerCentral.triggerState.totalTriggerErrorCounter);
	ASSERT_EQ(1500, Sensor::getOrZero(SensorType::Rpm));
	// we got RPM we got acceleration
	ASSERT_EQ(9375, engine->rpmCalculator.getRpmAcceleration());

	// third revolution slow down
	eth.smartFireTriggerEvents2(/*count*/1, /*delay*/ 80);
	ASSERT_EQ(1000, Sensor::getOrZero(SensorType::Rpm));
	ASSERT_NEAR(-2083.3335, engine->rpmCalculator.getRpmAcceleration(), EPS3D);
    ASSERT_EQ(0u,  engine->triggerCentral.triggerState.totalTriggerErrorCounter);
}

// dRPM (rpmRate) must still be computed from the cycle-averaged RPM even when RPM_UPDATE_INSTANT
// publishes instant RPM as the reported value - otherwise getRpmAcceleration() is stuck at 0.
TEST(engine, testRpmAccelerationAlwaysInstantRpm) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	engineConfiguration->rpmUpdateMode = rpmUpdateMode_e::RPM_UPDATE_INSTANT;
	eth.setTriggerType(trigger_type_e::TT_HALF_MOON);

	// first revolution - no acceleration info yet
	eth.smartFireTriggerEvents2(/*count*/1, /*delay*/ 40);
	ASSERT_EQ(0u, engine->triggerCentral.triggerState.totalTriggerErrorCounter);
	ASSERT_EQ(0, engine->rpmCalculator.getRpmAcceleration());

	// second revolution same speed - we get acceleration despite running on instant RPM
	eth.smartFireTriggerEvents2(/*count*/1, /*delay*/ 40);
	ASSERT_EQ(0u, engine->triggerCentral.triggerState.totalTriggerErrorCounter);
	ASSERT_EQ(9375, engine->rpmCalculator.getRpmAcceleration());

	// third revolution slow down
	eth.smartFireTriggerEvents2(/*count*/1, /*delay*/ 80);
	ASSERT_NEAR(-2083.3335, engine->rpmCalculator.getRpmAcceleration(), EPS3D);
	ASSERT_EQ(0u, engine->triggerCentral.triggerState.totalTriggerErrorCounter);
}
