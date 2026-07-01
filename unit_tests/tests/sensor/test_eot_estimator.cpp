#include "pch.h"
#include "eot_estimator.h"

TEST(EotEstimatorTest, NoFlowNeverHeats) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	engineConfiguration->eotFromIatCht = true;
	engineConfiguration->eotEstK0 = 10;
	engineConfiguration->eotEstK1 = 0.02f;
	engineConfiguration->eotEstK2 = 0.05f;
	engineConfiguration->eotEstK3 = -0.02f;
	engineConfiguration->eotEstTauHeat = 5;
	engineConfiguration->eotEstTauCool = 500;

	initEotEstimator();

	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 90);
	Sensor::setMockValue(SensorType::Iat, 20);
	Sensor::setMockValue(SensorType::OilPressure, 0);
	Sensor::setMockValue(SensorType::Rpm, 0);

	updateEotEstimator();
	eth.moveTimeForwardSec(5);
	updateEotEstimator();

	float afterSettling = Sensor::getOrZero(SensorType::OilTemperature);

	// Simulate a CHT sensor heat-soak bump right after shutdown: still no oil flow, but
	// the head reads hotter. The estimate must not be dragged back up toward CHT.
	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 110);
	eth.moveTimeForwardSec(5);
	updateEotEstimator();

	float afterHeatSoak = Sensor::getOrZero(SensorType::OilTemperature);

	EXPECT_LE(afterHeatSoak, afterSettling);
}

TEST(EotEstimatorTest, FlowingHeatsTowardCht) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	engineConfiguration->eotFromIatCht = true;
	engineConfiguration->eotEstK0 = 10;
	engineConfiguration->eotEstK1 = 0.02f;
	engineConfiguration->eotEstK2 = 0.05f;
	engineConfiguration->eotEstK3 = -0.02f;
	engineConfiguration->eotEstTauHeat = 5;
	engineConfiguration->eotEstTauCool = 20;

	initEotEstimator();

	Sensor::setMockValue(SensorType::CylinderHeadTemperature, 90);
	Sensor::setMockValue(SensorType::Iat, 20);

	// deltaTActual starts at 0 (estimate pinned at CHT), so first build up a real gap with
	// no oil flow before testing that flow pulls the estimate back toward CHT.
	Sensor::setMockValue(SensorType::OilPressure, 0);
	Sensor::setMockValue(SensorType::Rpm, 0);
	for (int i = 0; i < 100; i++) {
		eth.moveTimeForwardSec(1);
		updateEotEstimator();
	}

	float first = Sensor::getOrZero(SensorType::OilTemperature);

	// Oil starts flowing: higher pressure shrinks the target delta, so the estimate should
	// climb back toward CHT using the (short) heat tau.
	Sensor::setMockValue(SensorType::OilPressure, 300);
	Sensor::setMockValue(SensorType::Rpm, 3000);
	for (int i = 0; i < 30; i++) {
		eth.moveTimeForwardSec(1);
		updateEotEstimator();
	}

	float later = Sensor::getOrZero(SensorType::OilTemperature);

	EXPECT_GT(later, first);
}

TEST(EotEstimatorTest, FallbackWhenInvalid) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	engineConfiguration->eotFromIatCht = true;
	engineConfiguration->eotEstFallbackEot = 77;

	initEotEstimator();

	Sensor::setInvalidMockValue(SensorType::OilPressure);

	updateEotEstimator();

	EXPECT_NEAR(77, Sensor::getOrZero(SensorType::OilTemperature), 0.01);
}
