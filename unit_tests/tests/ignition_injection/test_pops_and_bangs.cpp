#include "pch.h"
#include "custom_page.h"
#include "engine_state_machine.h"
#include "dfco.h"

#if FUEL_RPM_COUNT == 16

static void setupSensors() {
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0);
	Sensor::setMockValue(SensorType::Rpm, 3000);
	Sensor::setMockValue(SensorType::Clt, 80.0f);
	Sensor::setMockValue(SensorType::VehicleSpeed, 0.0f);
}

static void setupPopsAndBangs() {
	engineConfiguration->popsAndBangsEnabled      = true;
	getCustomPage()->popsAndBangsDelay        = 0.0f;
	getCustomPage()->popsAndBangsDuration     = 2.0f;
	getCustomPage()->popsAndBangsRpmHigh      = 2500;
	getCustomPage()->popsAndBangsRpmLow       = 1800;
	getCustomPage()->popsAndBangsRpmMax       = 6000;
	getCustomPage()->popsAndBangsCltMin       = 40;
	getCustomPage()->popsAndBangsCltMax       = 105;
	getCustomPage()->popsAndBangsTimingOverride = -10.0f;
	getCustomPage()->popsAndBangsVeOverride   = 30.0f;
	getCustomPage()->popsAndBangsDisableMode  = POPS_AND_BANGS_DISABLE_MODE_NONE;
}

// Helper: run the ESM P&B state machine with overrun=true at the given time
static void tickPnb(EngineTestHelper& eth, bool overrun = true) {
	eth.engine.module<EngineStateMachine>().unmock().updatePopsAndBangs(overrun);
}

TEST(PopsAndBangs, DisabledByDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setTimeNowUs(1e6);
	tickPnb(eth, /*overrun=*/true);
	EXPECT_FALSE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, ActivatesInOverrunWithRpmInWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	tickPnb(eth);
	EXPECT_TRUE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, DoesNotActivateWhenNotInOverrun) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	tickPnb(eth, /*overrun=*/false);
	EXPECT_FALSE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, DoesNotActivateBelowRpmHigh) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	Sensor::setMockValue(SensorType::Rpm, 2000); // below popsAndBangsRpmHigh (2500)
	setTimeNowUs(1e6);
	tickPnb(eth);
	EXPECT_FALSE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, DoesNotActivateAboveRpmMax) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	Sensor::setMockValue(SensorType::Rpm, 7000); // above popsAndBangsRpmMax (6000)
	setTimeNowUs(1e6);
	tickPnb(eth);
	EXPECT_FALSE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, InhibitsDfcoDuringActive) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	engineConfiguration->coastingFuelCutEnabled  = true;
	engineConfiguration->coastingFuelCutTps      = 5;
	engineConfiguration->coastingFuelCutRpmHigh  = 1500;
	engineConfiguration->coastingFuelCutVssHigh  = 0;
	engineConfiguration->coastingFuelCutClt      = 40.0f;

	setTimeNowUs(1e6);
	tickPnb(eth);
	eth.engine.periodicFastCallback(); // runs DfcoController::update()

	EXPECT_TRUE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
	EXPECT_FALSE(eth.engine.module<DfcoController>().unmock().cutFuel());
}

TEST(PopsAndBangs, DeactivatesWhenRpmFallsBelowRpmLow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	tickPnb(eth);

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();
	EXPECT_TRUE(esm.engineSmIsPopsAndBangs);

	Sensor::setMockValue(SensorType::Rpm, 1600); // below popsAndBangsRpmLow (1800)
	tickPnb(eth);
	EXPECT_FALSE(esm.engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, DeactivatesAfterTimeout) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	tickPnb(eth);

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();
	EXPECT_TRUE(esm.engineSmIsPopsAndBangs);

	advanceTimeUs(2.1e6);
	tickPnb(eth);
	EXPECT_FALSE(esm.engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, DelayBeforeActivation) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	getCustomPage()->popsAndBangsDelay = 0.5f;

	setTimeNowUs(1e6);
	tickPnb(eth);

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();
	EXPECT_FALSE(esm.engineSmIsPopsAndBangs); // delay not yet elapsed

	advanceTimeUs(0.6e6);
	tickPnb(eth);
	EXPECT_TRUE(esm.engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, ColdEnginePreventsActivation) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	Sensor::setMockValue(SensorType::Clt, 20.0f); // below popsAndBangsCltMin (40)
	setTimeNowUs(1e6);
	tickPnb(eth);
	EXPECT_FALSE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, InactiveWhenFeatureDisabled) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	engineConfiguration->popsAndBangsEnabled = false;
	setTimeNowUs(1e6);
	tickPnb(eth);
	EXPECT_FALSE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

// VSS irrelevant: overrun is determined by the ESM (which ignores VSS), so P&B works at zero speed
TEST(PopsAndBangs, VssIsIrrelevant) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	engineConfiguration->coastingFuelCutVssHigh = 20; // DFCO needs speed — P&B doesn't
	Sensor::setMockValue(SensorType::VehicleSpeed, 0.0f);
	setTimeNowUs(1e6);
	tickPnb(eth, /*overrun=*/true); // ESM overrun doesn't check VSS
	EXPECT_TRUE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, DfcoLiveDataReflectsCutState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	tickPnb(eth);
	eth.engine.periodicFastCallback();

	auto& dfco = eth.engine.module<DfcoController>().unmock();
	EXPECT_FALSE(dfco.dfcoCutActive); // cut inhibited while P&B active
}

#endif // FUEL_RPM_COUNT == 16
