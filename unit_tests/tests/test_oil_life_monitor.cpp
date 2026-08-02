#include "pch.h"
#include "custom_page.h"
#include "oil_life_monitor.h"
#include "extra_flash_pages.h"

static OilLifeMonitor& getOlm() {
	return engine->module<OilLifeMonitor>().unmock();
}

// Regression guard: EFI_OIL_LIFE_RECORD_ID must be registered in extra_flash_pages.cpp's
// getExtraPageFlashOffset(), or storage_flash.cpp's isIdSupported() returns false and the
// storage manager thread never runs load()/store() on an internal-flash-only board (no MFS/SD)
// -- m_loadPending then stays true forever and onSlowCallback() blocks accumulation completely.
TEST(OilLifeMonitor, FlashOffsetIsRegistered) {
	EXPECT_NE(getExtraPageFlashOffset(EFI_OIL_LIFE_RECORD_ID), 0u);
}

TEST(OilLifeMonitor, PercentFormula) {
	// 0 accumulated revs -> 100% life.
	EXPECT_FLOAT_EQ(OilLifeMonitor::computeOilLifePercent(0, 6), 100.0f);

	// Exactly at the budget (6 million revs @ scale=6) -> 0%.
	EXPECT_FLOAT_EQ(OilLifeMonitor::computeOilLifePercent(6'000'000, 6), 0.0f);

	// Halfway through the budget -> 50%.
	EXPECT_FLOAT_EQ(OilLifeMonitor::computeOilLifePercent(3'000'000, 6), 50.0f);

	// Over the budget clamps to 0, never goes negative.
	EXPECT_FLOAT_EQ(OilLifeMonitor::computeOilLifePercent(50'000'000, 6), 0.0f);

	// A corrupt/zero scale fails open to 100% rather than dividing by zero.
	EXPECT_FLOAT_EQ(OilLifeMonitor::computeOilLifePercent(1'000'000, 0), 100.0f);
}

TEST(OilLifeMonitor, ZoneBoundaries) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	auto* cfg = getCustomPage();
	// Distinct sentinel values per zone/source so a boundary mistake shows up immediately.
	cfg->oilLifeMultOilCold      = 1.0f;
	cfg->oilLifeMultOilOptimal   = 2.0f;
	cfg->oilLifeMultOilHighHeat  = 3.0f;
	cfg->oilLifeMultOilExtreme   = 4.0f;
	cfg->oilLifeMultCoolantCold     = 11.0f;
	cfg->oilLifeMultCoolantOptimal  = 12.0f;
	cfg->oilLifeMultCoolantHighHeat = 13.0f;
	cfg->oilLifeMultCoolantExtreme  = 14.0f;

	// Oil-temp source.
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(69.9f, false), 1.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(70.0f, false), 2.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(104.9f, false), 2.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(105.0f, false), 3.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(124.9f, false), 3.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(125.0f, false), 4.0f);

	// Coolant-fallback source: same boundaries, different (harsher) multiplier set.
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(69.9f, true), 11.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(70.0f, true), 12.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(104.9f, true), 12.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(105.0f, true), 13.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(124.9f, true), 13.0f);
	EXPECT_FLOAT_EQ(OilLifeMonitor::getZoneMultiplier(125.0f, true), 14.0f);
}

TEST(OilLifeMonitor, TempSourceFallback_DefaultOilPrimary) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	auto* cfg = getCustomPage();
	cfg->oilLifeMonitorEnabled = true;
	cfg->oilLifePrimarySource = oil_life_temp_source_e::OilTemp; // matches the default

	auto& olm = getOlm();
	olm.init();

	Sensor::setMockValue(SensorType::OilTemperature, 90);
	Sensor::setMockValue(SensorType::Clt, 90);
	engine->rpmCalculator.onNewEngineCycle();
	olm.onSlowCallback();
	EXPECT_EQ(olm.getTempSourceForOutput(), 0); // Oil

	Sensor::resetMockValue(SensorType::OilTemperature);
	engine->rpmCalculator.onNewEngineCycle();
	olm.onSlowCallback();
	EXPECT_EQ(olm.getTempSourceForOutput(), 1); // fell back to Coolant

	Sensor::setMockValue(SensorType::OilTemperature, 90);
	engine->rpmCalculator.onNewEngineCycle();
	olm.onSlowCallback();
	EXPECT_EQ(olm.getTempSourceForOutput(), 0); // Oil, back once valid again
}

TEST(OilLifeMonitor, TempSourceFallback_CoolantPrimary) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	auto* cfg = getCustomPage();
	cfg->oilLifeMonitorEnabled = true;
	cfg->oilLifePrimarySource = oil_life_temp_source_e::CoolantTemp;

	auto& olm = getOlm();
	olm.init();

	Sensor::setMockValue(SensorType::OilTemperature, 90);
	Sensor::setMockValue(SensorType::Clt, 90);
	engine->rpmCalculator.onNewEngineCycle();
	olm.onSlowCallback();
	EXPECT_EQ(olm.getTempSourceForOutput(), 1); // Coolant is primary

	Sensor::resetMockValue(SensorType::Clt);
	engine->rpmCalculator.onNewEngineCycle();
	olm.onSlowCallback();
	EXPECT_EQ(olm.getTempSourceForOutput(), 0); // fell back to Oil

	Sensor::setMockValue(SensorType::Clt, 90);
	engine->rpmCalculator.onNewEngineCycle();
	olm.onSlowCallback();
	EXPECT_EQ(olm.getTempSourceForOutput(), 1); // Coolant, back once valid again
}

TEST(OilLifeMonitor, AccumulatesWeightedRevs) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	auto* cfg = getCustomPage();
	cfg->oilLifeMonitorEnabled = true;
	cfg->oilLifeRevsScaleMillions = 1; // 1,000,000 weighted revs == 0%
	cfg->oilLifeMultOilOptimal = 2.0f; // easy-to-check multiplier

	auto& olm = getOlm();
	olm.init();

	Sensor::setMockValue(SensorType::OilTemperature, 90); // optimal zone

	constexpr int revs = 100'000;
	for (int i = 0; i < revs; i++) {
		engine->rpmCalculator.onNewEngineCycle();
	}
	olm.onSlowCallback();

	// 100,000 raw revs * 2.0x multiplier = 200,000 weighted revs against a 1,000,000 budget:
	// (1 - 200,000 / 1,000,000) * 100 = 80%.
	EXPECT_FLOAT_EQ(olm.getOilLifePercent(), 80.0f);
}

TEST(OilLifeMonitor, ThisDriveResetsOnIgnitionOn) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	auto* cfg = getCustomPage();
	cfg->oilLifeMonitorEnabled = true;
	cfg->oilLifeMultOilOptimal = 2.0f;

	auto& olm = getOlm();
	olm.init();

	Sensor::setMockValue(SensorType::OilTemperature, 90); // optimal zone

	constexpr int revs = 1'000;
	for (int i = 0; i < revs; i++) {
		engine->rpmCalculator.onNewEngineCycle();
	}
	olm.onSlowCallback();

	// 1,000 raw revs * 2.0x multiplier = 2,000 weighted revs.
	EXPECT_EQ(olm.getWeightedRevsThisDrive(), 2000u);

	// A new drive cycle (ignition on) resets the per-drive counter...
	olm.onIgnitionStateChanged(true);
	EXPECT_EQ(olm.getWeightedRevsThisDrive(), 0u);

	// ...but does not touch the lifetime accumulator.
	EXPECT_GT(olm.getOilLifePercent(), 0.0f);
	EXPECT_LT(olm.getOilLifePercent(), 100.0f);

	for (int i = 0; i < revs; i++) {
		engine->rpmCalculator.onNewEngineCycle();
	}
	olm.onSlowCallback();
	EXPECT_EQ(olm.getWeightedRevsThisDrive(), 2000u);
}

TEST(OilLifeMonitor, ShutdownSaveFlow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	auto& olm = getOlm();
	olm.init();

	EXPECT_FALSE(olm.needsDelayedShutoff());

	olm.onIgnitionStateChanged(false);
	EXPECT_TRUE(olm.needsDelayedShutoff());

	olm.store();
	EXPECT_FALSE(olm.needsDelayedShutoff());
}

TEST(OilLifeMonitor, Reset) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	auto* cfg = getCustomPage();
	cfg->oilLifeMonitorEnabled = true;
	cfg->oilLifeRevsScaleMillions = 1;

	auto& olm = getOlm();
	olm.init();

	Sensor::setMockValue(SensorType::OilTemperature, 90);
	for (int i = 0; i < 500'000; i++) {
		engine->rpmCalculator.onNewEngineCycle();
	}
	olm.onSlowCallback();
	EXPECT_LT(olm.getOilLifePercent(), 100.0f);

	olm.reset();
	EXPECT_FLOAT_EQ(olm.getOilLifePercent(), 100.0f);
}

TEST(OilLifeMonitor, SetOilLifePercent) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	auto* cfg = getCustomPage();
	cfg->oilLifeRevsScaleMillions = 1; // 1,000,000 weighted revs == 0%

	auto& olm = getOlm();
	olm.init();

	// Manual correction, e.g. after a settings loss -- matches the "set_oil_life" console command.
	olm.setOilLifePercent(75.0f);
	EXPECT_FLOAT_EQ(olm.getOilLifePercent(), 75.0f);

	// Out-of-range input clamps rather than wrapping/going negative.
	olm.setOilLifePercent(150.0f);
	EXPECT_FLOAT_EQ(olm.getOilLifePercent(), 100.0f);

	olm.setOilLifePercent(-10.0f);
	EXPECT_FLOAT_EQ(olm.getOilLifePercent(), 0.0f);
}
