#include "pch.h"
#include "custom_page.h"
#include "engine_state_machine.h"
#include "dfco.h"
#include "exhaust_cutout.h"

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

// Helper: advance the crank revolution counter that drives the spark-cut "every X revs" window.
static void advanceRevs(EngineTestHelper& eth, int count) {
	for (int i = 0; i < count; i++) {
		eth.engine.rpmCalculator.onNewEngineCycle();
	}
}

static void setupSparkCut(EngineTestHelper& eth) {
	getCustomPage()->popsAndBangsSparkCutEnabled = true;
	getCustomPage()->popsAndBangsCutDurationAuto = false;
	getCustomPage()->popsAndBangsCutEveryRevs    = 3;
	getCustomPage()->popsAndBangsCutPercent      = 50;
	getCustomPage()->popsAndBangsCutDurationMs   = 100;
	tickPnb(eth); // activate P&B so the spark-cut overlay can run
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

// ---- Spark cut overlay ----

TEST(PopsAndBangs, SparkCutZeroWhenDisabled) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	tickPnb(eth);

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();
	ASSERT_TRUE(esm.engineSmIsPopsAndBangs);
	// Spark cut left disabled — ratio is always zero regardless of revolutions.
	advanceRevs(eth, 50);
	EXPECT_FLOAT_EQ(0.0f, esm.getPopsAndBangsSparkSkipRatio());
	EXPECT_FALSE(esm.engineSmPnbSparkCut);
}

TEST(PopsAndBangs, SparkCutZeroWhenPnbInactive) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	getCustomPage()->popsAndBangsSparkCutEnabled = true;
	getCustomPage()->popsAndBangsCutEveryRevs = 1;
	getCustomPage()->popsAndBangsCutPercent = 80;
	setTimeNowUs(1e6);
	tickPnb(eth, /*overrun=*/false); // P&B never activates

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();
	ASSERT_FALSE(esm.engineSmIsPopsAndBangs);
	advanceRevs(eth, 10);
	EXPECT_FLOAT_EQ(0.0f, esm.getPopsAndBangsSparkSkipRatio());
}

TEST(PopsAndBangs, SparkCutOpensWindowEveryXRevs) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	setupSparkCut(eth); // everyRevs=3, cutPercent=50, duration=100ms

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();

	// Not yet 3 revolutions since activation: still firing (ratio 0).
	EXPECT_FLOAT_EQ(0.0f, esm.getPopsAndBangsSparkSkipRatio());
	advanceRevs(eth, 2);
	EXPECT_FLOAT_EQ(0.0f, esm.getPopsAndBangsSparkSkipRatio());

	// Third revolution opens the cut window: ratio == cutPercent/100.
	advanceRevs(eth, 1);
	EXPECT_FLOAT_EQ(0.5f, esm.getPopsAndBangsSparkSkipRatio());
	EXPECT_TRUE(esm.engineSmPnbSparkCut);
}

TEST(PopsAndBangs, SparkCutWindowClosesAfterDuration) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	setupSparkCut(eth);

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();

	// Open the window.
	advanceRevs(eth, 3);
	EXPECT_FLOAT_EQ(0.5f, esm.getPopsAndBangsSparkSkipRatio());

	// Window stays open within the duration.
	advanceTimeUs(50e3); // 50ms < 100ms
	EXPECT_FLOAT_EQ(0.5f, esm.getPopsAndBangsSparkSkipRatio());

	// Past the duration the window closes (ratio 0) and rev-counting restarts.
	advanceTimeUs(60e3); // total 110ms > 100ms
	EXPECT_FLOAT_EQ(0.0f, esm.getPopsAndBangsSparkSkipRatio());
	EXPECT_FALSE(esm.engineSmPnbSparkCut);

	// Needs another full interval before the next window.
	advanceRevs(eth, 2);
	EXPECT_FLOAT_EQ(0.0f, esm.getPopsAndBangsSparkSkipRatio());
	advanceRevs(eth, 1);
	EXPECT_FLOAT_EQ(0.5f, esm.getPopsAndBangsSparkSkipRatio());
}

TEST(PopsAndBangs, SparkCutPercentClampedTo100) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	setupSparkCut(eth);
	getCustomPage()->popsAndBangsCutPercent = 150; // out of range, clamps to 100

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();
	advanceRevs(eth, 3);
	EXPECT_FLOAT_EQ(1.0f, esm.getPopsAndBangsSparkSkipRatio());
}

// ---- Exhaust cutout inhibit gate ----

TEST(PopsAndBangs, CutoutInhibitOffIgnoresCutoutState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	getCustomPage()->popsAndBangsCutoutInhibitMode = pops_and_bangs_cutout_inhibit_e::Off;
	eth.engine.module<ExhaustCutoutController>().unmock().isCutoutOpen = false;
	setTimeNowUs(1e6);
	tickPnb(eth);
	EXPECT_TRUE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, CutoutInhibitBlocksActivationWhenCutoutClosed) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	getCustomPage()->popsAndBangsCutoutInhibitMode = pops_and_bangs_cutout_inhibit_e::Inhibit;
	eth.engine.module<ExhaustCutoutController>().unmock().isCutoutOpen = false;
	setTimeNowUs(1e6);
	tickPnb(eth);
	EXPECT_FALSE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, CutoutInhibitAllowsActivationWhenCutoutOpen) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	getCustomPage()->popsAndBangsCutoutInhibitMode = pops_and_bangs_cutout_inhibit_e::Inhibit;
	eth.engine.module<ExhaustCutoutController>().unmock().isCutoutOpen = true;
	setTimeNowUs(1e6);
	tickPnb(eth);
	EXPECT_TRUE(eth.engine.module<EngineStateMachine>().unmock().engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, CutoutInhibitExpiresActiveWindowWhenCutoutCloses) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	getCustomPage()->popsAndBangsCutoutInhibitMode = pops_and_bangs_cutout_inhibit_e::Inhibit;
	auto& cutout = eth.engine.module<ExhaustCutoutController>().unmock();
	cutout.isCutoutOpen = true;
	setTimeNowUs(1e6);
	tickPnb(eth);

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();
	ASSERT_TRUE(esm.engineSmIsPopsAndBangs);

	cutout.isCutoutOpen = false;
	tickPnb(eth);
	EXPECT_FALSE(esm.engineSmIsPopsAndBangs);
}

TEST(PopsAndBangs, SparkCutAutoDurationInRange) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSensors();
	setupPopsAndBangs();
	setTimeNowUs(1e6);
	setupSparkCut(eth);
	getCustomPage()->popsAndBangsCutDurationAuto = true;

	auto& esm = eth.engine.module<EngineStateMachine>().unmock();

	// Open a window, then confirm it stays open at least the minimum (20ms) and closes
	// by the maximum (500ms) — i.e. the random duration lands inside the 20-500ms range.
	advanceRevs(eth, 3);
	EXPECT_FLOAT_EQ(0.5f, esm.getPopsAndBangsSparkSkipRatio());
	advanceTimeUs(19e3); // < 20ms minimum: must still be open
	EXPECT_FLOAT_EQ(0.5f, esm.getPopsAndBangsSparkSkipRatio());
	advanceTimeUs(500e3); // > 500ms maximum: must have closed
	EXPECT_FLOAT_EQ(0.0f, esm.getPopsAndBangsSparkSkipRatio());
}

#endif // FUEL_RPM_COUNT == 16
