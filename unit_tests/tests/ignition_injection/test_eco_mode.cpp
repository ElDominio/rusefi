#include "pch.h"
#include "custom_page.h"
#include "engine_state_machine.h"

#if FUEL_RPM_COUNT == 16

// Eco mode is an Engine State Machine overlay: it engages once the engine has been in the
// Cruising state continuously for ecoModeCruisingTime, drops instantly on leaving Cruising, and
// can be forced on or inhibited by a manual switch / Lua gauge. The output overrides (target AFR,
// timing adder, VVT target, throttle multiplier) are one-liners mirroring the existing Limp/P&B
// reads at their respective sites; these tests cover the new engagement/switch logic itself.

static void setupEco() {
	getCustomPage()->ecoModeEnabled        = true;
	getCustomPage()->ecoModeVvtOverride    = false;
	getCustomPage()->ecoModeCruisingTime   = 2; // 2 s of steady cruise before engaging
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

TEST(EcoMode, ForceOnSwitchEngagesRegardlessOfTimer) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupEco();
	getCustomPage()->ecoModeSwitchMode = eco_mode_switch_mode_e::ForceOn;
	Sensor::setMockValue(SensorType::LuaGauge1, 1.0f); // >= 0.5 threshold -> asserted

	setTimeNowUs(1e6);
	// Not cruising and no time accumulated, but the switch forces eco on.
	tickEco(eth, EngineStateMachineState::Idle);
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
