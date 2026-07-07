#include "pch.h"

#include "exhaust_cutout.h"
#include "engine_state_machine.h"
#include "custom_page.h"

namespace {

ExhaustCutoutController& dut() {
	return engine->module<ExhaustCutoutController>().unmock();
}

void setSportMode(bool v) {
	engine->module<EngineStateMachine>().unmock().engineSmIsSportMode = v;
}

} // namespace

// Sport Mode activation: cutout follows the shared Sport Mode flag, same as a hardware switch.
TEST(ExhaustCutout, sportModeActivation) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->exhaustCutoutEnabled = true;
	auto cfg = getCustomPage();
	cfg->exhaustCutoutActivationMode = EXHAUST_CUTOUT_SPORT_MODE;
	cfg->exhaustCutoutBehaviorLow = EXHAUST_CUTOUT_ALWAYS_CLOSED;
	cfg->exhaustCutoutBehaviorHigh = EXHAUST_CUTOUT_ALWAYS_OPEN;

	setSportMode(false);
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isCutoutOpen);

	setSportMode(true);
	dut().onSlowCallback();
	EXPECT_TRUE(dut().isCutoutOpen);
}

// Auto + Sport Mode: RPM/TPS/MAP auto-trigger only takes effect while Sport Mode is active;
// bypasses the Low/High behavior lookup entirely.
TEST(ExhaustCutout, autoSportModeRequiresSportModeAndTrigger) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->exhaustCutoutEnabled = true;
	auto cfg = getCustomPage();
	cfg->exhaustCutoutActivationMode = EXHAUST_CUTOUT_AUTO_SPORT_MODE;
	cfg->exhaustCutoutOpenRpm = 3000;

	// Sport Mode on, but RPM below the auto-trigger threshold -> stays closed.
	Sensor::setMockValue(SensorType::Rpm, 2000);
	setSportMode(true);
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isCutoutOpen);

	// RPM crosses the threshold while Sport Mode is active -> opens.
	Sensor::setMockValue(SensorType::Rpm, 3500);
	dut().onSlowCallback();
	EXPECT_TRUE(dut().isCutoutOpen);

	// Sport Mode turns off, RPM still above threshold -> closes regardless of the auto trigger.
	setSportMode(false);
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isCutoutOpen);
}
