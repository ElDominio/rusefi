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

// Auto + Sport Mode: opens when the RPM/TPS/MAP auto-trigger is met OR Sport Mode is active;
// bypasses the Low/High behavior lookup entirely.
TEST(ExhaustCutout, autoSportModeOpensOnTriggerOrSportMode) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->exhaustCutoutEnabled = true;
	auto cfg = getCustomPage();
	cfg->exhaustCutoutActivationMode = EXHAUST_CUTOUT_AUTO_SPORT_MODE;
	cfg->exhaustCutoutOpenRpm = 3000;
	cfg->exhaustCutoutClosingDelayS = 0;

	// Neither trigger nor Sport Mode -> closed.
	Sensor::setMockValue(SensorType::Rpm, 2000);
	setSportMode(false);
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isCutoutOpen);

	// RPM above the threshold without Sport Mode -> opens.
	Sensor::setMockValue(SensorType::Rpm, 3500);
	dut().onSlowCallback();
	EXPECT_TRUE(dut().isTriggerRpm);
	EXPECT_TRUE(dut().isCutoutOpen);

	// RPM drops below the threshold -> closes.
	Sensor::setMockValue(SensorType::Rpm, 2000);
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isCutoutOpen);

	// Sport Mode alone (RPM below the threshold) -> opens.
	setSportMode(true);
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isTriggerRpm);
	EXPECT_TRUE(dut().isCutoutOpen);

	// Sport Mode turns off with no trigger -> closes.
	setSportMode(false);
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isCutoutOpen);
}

// Lua override forces the cutout open in Auto + Sport Mode even while Sport Mode is inactive
// and no RPM/TPS/MAP trigger is met.
TEST(ExhaustCutout, luaOverrideForcesOpenInAutoSportMode) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->exhaustCutoutEnabled = true;
	auto cfg = getCustomPage();
	cfg->exhaustCutoutActivationMode = EXHAUST_CUTOUT_AUTO_SPORT_MODE;
	cfg->exhaustCutoutOpenRpm = 3000;

	Sensor::setMockValue(SensorType::Rpm, 0);
	setSportMode(false);
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isCutoutOpen);

	dut().isLuaOverrideActive = true;
	dut().onSlowCallback();
	EXPECT_TRUE(dut().isCutoutOpen);

	dut().isLuaOverrideActive = false;
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isCutoutOpen);
}

// Lua override forces the cutout open under plain "Auto" behavior (Switch/Sport Mode activation),
// but must not override an explicit Always Closed behavior.
TEST(ExhaustCutout, luaOverrideAppliesOnlyToAutoBehavior) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->exhaustCutoutEnabled = true;
	auto cfg = getCustomPage();
	cfg->exhaustCutoutActivationMode = EXHAUST_CUTOUT_SWITCH;
	cfg->exhaustCutoutBehaviorLow = EXHAUST_CUTOUT_ALWAYS_CLOSED;
	cfg->exhaustCutoutBehaviorHigh = EXHAUST_CUTOUT_AUTO;
	cfg->exhaustCutoutOpenRpm = 3000;

	Sensor::setMockValue(SensorType::Rpm, 0);
	dut().isLuaOverrideActive = true;

	// Switch input LOW -> Always Closed behavior; override does not apply.
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isCutoutOpen);
}

// H-Bridge output mode: same control logic and status bits as Digital/PWM; OPENING/CLOSING
// report moving until the motor drive duration elapses.
TEST(ExhaustCutout, hBridgeModeStatus) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->exhaustCutoutEnabled = true;
	auto cfg = getCustomPage();
	cfg->exhaustCutoutOutputMode = EXHAUST_CUTOUT_OUTPUT_HBRIDGE;
	cfg->exhaustCutoutHBridgeDutyCycle = 100;
	cfg->exhaustCutoutMoveDurationS = 1;
	cfg->exhaustCutoutActivationMode = EXHAUST_CUTOUT_SPORT_MODE;
	cfg->exhaustCutoutBehaviorLow = EXHAUST_CUTOUT_ALWAYS_CLOSED;
	cfg->exhaustCutoutBehaviorHigh = EXHAUST_CUTOUT_ALWAYS_OPEN;
	dut().initPins();

	setSportMode(false);
	dut().onSlowCallback();
	EXPECT_FALSE(dut().isInputHigh);
	EXPECT_FALSE(dut().isCutoutOpen);

	setSportMode(true);
	dut().onSlowCallback();
	EXPECT_TRUE(dut().isInputHigh);
	EXPECT_TRUE(dut().targetOpen);
	EXPECT_TRUE(dut().isCutoutOpen);
	EXPECT_TRUE(dut().isCutoutMoving);

	advanceTimeUs(1'100'000);
	dut().onSlowCallback();
	EXPECT_TRUE(dut().isCutoutOpen);
	EXPECT_FALSE(dut().isCutoutMoving);
}
