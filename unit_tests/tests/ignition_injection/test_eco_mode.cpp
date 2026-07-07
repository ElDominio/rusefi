#include "pch.h"
#include "custom_page.h"
#include "engine_state_machine.h"

#if FUEL_RPM_COUNT == 16

// Eco mode is an Engine State Machine overlay: it engages once the engine has been in the
// Cruising state continuously for ecoModeCruisingTime with MAP at or below ecoModeMapLimit, drops
// instantly on leaving Cruising or exceeding the MAP limit, and can be inhibited by a manual
// switch / Lua gauge. The output overrides (target AFR, timing adder, VVT target, throttle
// multiplier) are one-liners mirroring the existing Limp/P&B reads at their respective sites;
// these tests cover the new engagement/switch/MAP-gate logic itself.

static void setupEco() {
	getCustomPage()->ecoModeEnabled        = true;
	getCustomPage()->ecoModeVvtOverride    = false;
	getCustomPage()->ecoModeCruisingTime   = 2; // 2 s of steady cruise before engaging
	getCustomPage()->ecoModeMapLimit       = 0; // disabled unless a test overrides it
	getCustomPage()->ecoTargetAfr          = 15.5f;
	getCustomPage()->ecoTimingAdder        = 3.0f;
	getCustomPage()->ecoThrottleMult       = 0.9f;
	getCustomPage()->ecoModeSwitchMode     = eco_mode_switch_mode_e::Off;
	getCustomPage()->ecoModeLuaGauge       = LUA_GAUGE_1;
	getCustomPage()->ecoModeLuaGaugeMeaning = LUA_GAUGE_LOWER_BOUND;
	getCustomPage()->ecoModeLuaGaugeValue  = 0.5f;
}

static void tickEco(EngineTestHelper& eth, EngineStateMachineState state) {
	eth.engine.module<EngineStateMachine>().unmock().updateEcoMode(state);
}

static bool ecoActive(EngineTestHelper& eth) {
	return eth.engine.module<EngineStateMachine>().unmock().engineSmIsEcoMode;
}

TEST(EcoMode, DisabledByDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	getCustomPage()->ecoModeEnabled = false;
	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);    // arm the cruise timer
	advanceTimeUs(5e6);
	tickEco(eth, EngineStateMachineState::Cruising); // well past the cruise time
	EXPECT_FALSE(ecoActive(eth));
}

TEST(EcoMode, EngagesAfterSustainedCruising) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);    // arm: timer reset at t=1s
	EXPECT_FALSE(ecoActive(eth));

	advanceTimeUs(1.5e6);                            // t=2.5s, 1.5s of cruise < 2s
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_FALSE(ecoActive(eth));

	advanceTimeUs(1.0e6);                            // t=3.5s, 2.5s of cruise >= 2s
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_TRUE(ecoActive(eth));
}

TEST(EcoMode, InstantDropOnLeavingCruising) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);
	advanceTimeUs(3e6);
	tickEco(eth, EngineStateMachineState::Cruising);
	ASSERT_TRUE(ecoActive(eth));

	// A tip-in into WOT drops eco immediately and resets the cruise timer...
	tickEco(eth, EngineStateMachineState::WOT);
	EXPECT_FALSE(ecoActive(eth));

	// ...so returning to Cruising must re-accumulate the full cruise time, not re-engage at once.
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_FALSE(ecoActive(eth));
}

TEST(EcoMode, LimpModeOutVotesEco) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	auto& esm = eth.engine.module<EngineStateMachine>().unmock();
	esm.engineSmIsLimp = true; // protective state wins

	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);
	advanceTimeUs(5e6);
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_FALSE(ecoActive(eth));
}

// Sport Mode and Eco Mode are mutually exclusive: the driver's explicit request for aggressive
// behavior out-votes the automatic economy overlay.
TEST(EcoMode, SportModeOutVotesEco) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	auto& esm = eth.engine.module<EngineStateMachine>().unmock();
	esm.engineSmIsSportMode = true;

	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);
	advanceTimeUs(5e6);
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_FALSE(ecoActive(eth));
}

TEST(EcoMode, MapAboveLimitBlocksEngagement) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	getCustomPage()->ecoModeMapLimit = 60; // kPa
	Sensor::setMockValue(SensorType::Map, 70.0f); // above the limit

	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);    // arm: timer reset at t=1s
	advanceTimeUs(3e6);                              // well past the cruise time
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_FALSE(ecoActive(eth));
}

TEST(EcoMode, MapAtOrBelowLimitAllowsEngagement) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	getCustomPage()->ecoModeMapLimit = 60; // kPa
	Sensor::setMockValue(SensorType::Map, 60.0f); // at the limit -> allowed

	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);
	advanceTimeUs(3e6);
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_TRUE(ecoActive(eth));
}

TEST(EcoMode, MapRisingAboveLimitDropsAlreadyActiveEco) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	getCustomPage()->ecoModeMapLimit = 60; // kPa
	Sensor::setMockValue(SensorType::Map, 40.0f);

	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);
	advanceTimeUs(3e6);
	tickEco(eth, EngineStateMachineState::Cruising);
	ASSERT_TRUE(ecoActive(eth));

	// Load creeps up past the limit while still nominally Cruising -> instant drop.
	Sensor::setMockValue(SensorType::Map, 80.0f);
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_FALSE(ecoActive(eth));

	// ...and the cruise timer was reset, so it must re-accumulate before re-engaging even once
	// MAP drops back down.
	Sensor::setMockValue(SensorType::Map, 40.0f);
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_FALSE(ecoActive(eth));
}

TEST(EcoMode, ZeroMapLimitDisablesTheGate) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	getCustomPage()->ecoModeMapLimit = 0; // disabled
	Sensor::setMockValue(SensorType::Map, 250.0f); // would block if the gate were active

	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);
	advanceTimeUs(3e6);
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_TRUE(ecoActive(eth));
}

TEST(EcoMode, InhibitSwitchBlocksEngagement) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	getCustomPage()->ecoModeCruisingTime = 0; // timer would otherwise allow immediate engage
	getCustomPage()->ecoModeSwitchMode = eco_mode_switch_mode_e::Inhibit;
	Sensor::setMockValue(SensorType::LuaGauge1, 1.0f); // asserted -> inhibits

	setTimeNowUs(1e6);
	tickEco(eth, EngineStateMachineState::Idle);
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_FALSE(ecoActive(eth));

	// Releasing the inhibit switch lets the (already-elapsed) timer engage eco.
	Sensor::setMockValue(SensorType::LuaGauge1, 0.0f); // < 0.5 -> not asserted
	tickEco(eth, EngineStateMachineState::Cruising);
	EXPECT_TRUE(ecoActive(eth));
}

#endif // FUEL_RPM_COUNT == 16
