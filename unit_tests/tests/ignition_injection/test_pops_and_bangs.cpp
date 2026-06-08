#include "pch.h"
#include "dfco.h"

#if FUEL_RPM_COUNT == 16

static void setupOverrunConditions() {
	// isOverrun() base conditions: TPS < threshold, RPM > RpmHigh, VSS always qualifies
	engineConfiguration->coastingFuelCutTps = 5;
	engineConfiguration->coastingFuelCutRpmHigh = 1500;
	engineConfiguration->coastingFuelCutVssHigh = 0;

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0);
	Sensor::setMockValue(SensorType::Rpm, 3000);
	Sensor::setMockValue(SensorType::Clt, 80.0f);
}

static void setupPopsAndBangs() {
	engineConfiguration->popsAndBangsEnabled = true;
	engineConfiguration->popsAndBangsDelay = 0.0f;
	engineConfiguration->popsAndBangsDuration = 2.0f;
	engineConfiguration->popsAndBangsRpmHigh = 2500;
	engineConfiguration->popsAndBangsRpmLow = 1800;
	engineConfiguration->popsAndBangsRpmMax = 6000;
	engineConfiguration->popsAndBangsCltMin = 40;
	engineConfiguration->popsAndBangsCltMax = 105;
	engineConfiguration->popsAndBangsTimingOverride = -10.0f;
	engineConfiguration->popsAndBangsVeOverride = 30.0f;
	engineConfiguration->popsAndBangsDisableMode = POPS_AND_BANGS_DISABLE_MODE_NONE;
}

TEST(PopsAndBangs, DisabledByDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	EXPECT_FALSE(engineConfiguration->popsAndBangsEnabled);
	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_FALSE(dfco.isPopsAndBangsActive());
}

TEST(PopsAndBangs, ActivatesInOverrunWithRpmInWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupOverrunConditions();
	setupPopsAndBangs();

	setTimeNowUs(1e6);
	eth.engine.periodicFastCallback();

	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_TRUE(dfco.isPopsAndBangsActive());
}

TEST(PopsAndBangs, DoesNotActivateBelowRpmHigh) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupOverrunConditions();
	setupPopsAndBangs();
	// RPM is between coastingFuelCutRpmHigh (1500) and popsAndBangsRpmHigh (2500)
	// overrun = true, but P&B RPM window not met
	Sensor::setMockValue(SensorType::Rpm, 2000);

	setTimeNowUs(1e6);
	eth.engine.periodicFastCallback();

	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_FALSE(dfco.isPopsAndBangsActive());
}

TEST(PopsAndBangs, DoesNotActivateAboveRpmMax) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupOverrunConditions();
	setupPopsAndBangs();
	Sensor::setMockValue(SensorType::Rpm, 7000); // above popsAndBangsRpmMax (6000)

	setTimeNowUs(1e6);
	eth.engine.periodicFastCallback();

	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_FALSE(dfco.isPopsAndBangsActive());
}

TEST(PopsAndBangs, InhibitsDfcoDuringActive) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupOverrunConditions();
	setupPopsAndBangs();
	engineConfiguration->coastingFuelCutEnabled = true;
	engineConfiguration->coastingFuelCutClt = 40.0f;

	setTimeNowUs(1e6);
	eth.engine.periodicFastCallback();

	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_TRUE(dfco.isPopsAndBangsActive());
	EXPECT_FALSE(dfco.cutFuel());
}

TEST(PopsAndBangs, DeactivatesWhenRpmFallsBelowRpmLow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupOverrunConditions();
	setupPopsAndBangs();

	setTimeNowUs(1e6);
	eth.engine.periodicFastCallback();

	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_TRUE(dfco.isPopsAndBangsActive());

	// Drop to 1600: still in overrun (> coastingFuelCutRpmHigh 1500) but below popsAndBangsRpmLow (1800)
	Sensor::setMockValue(SensorType::Rpm, 1600);
	eth.engine.periodicFastCallback();

	EXPECT_FALSE(dfco.isPopsAndBangsActive());
}

TEST(PopsAndBangs, DeactivatesAfterTimeout) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupOverrunConditions();
	setupPopsAndBangs();

	setTimeNowUs(1e6);
	eth.engine.periodicFastCallback();

	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_TRUE(dfco.isPopsAndBangsActive());

	// Advance past duration (2.0 seconds)
	advanceTimeUs(2.1e6);
	eth.engine.periodicFastCallback();

	EXPECT_FALSE(dfco.isPopsAndBangsActive());
}

TEST(PopsAndBangs, DelayBeforeActivation) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupOverrunConditions();
	setupPopsAndBangs();
	engineConfiguration->popsAndBangsDelay = 0.5f;

	setTimeNowUs(1e6);
	eth.engine.periodicFastCallback();

	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_FALSE(dfco.isPopsAndBangsActive()); // delay not yet elapsed

	advanceTimeUs(0.6e6);
	eth.engine.periodicFastCallback();

	EXPECT_TRUE(dfco.isPopsAndBangsActive());
}

TEST(PopsAndBangs, ColdEnginePreventsActivation) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupOverrunConditions();
	setupPopsAndBangs();
	Sensor::setMockValue(SensorType::Clt, 20.0f); // below popsAndBangsCltMin (40)

	setTimeNowUs(1e6);
	eth.engine.periodicFastCallback();

	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_FALSE(dfco.isPopsAndBangsActive());
}

TEST(PopsAndBangs, InactiveWhenFeatureDisabled) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupOverrunConditions();
	setupPopsAndBangs();
	engineConfiguration->popsAndBangsEnabled = false;

	setTimeNowUs(1e6);
	eth.engine.periodicFastCallback();

	auto& dfco = engine->module<DfcoController>().unmock();
	EXPECT_FALSE(dfco.isPopsAndBangsActive());
}

#endif // FUEL_RPM_COUNT == 16
