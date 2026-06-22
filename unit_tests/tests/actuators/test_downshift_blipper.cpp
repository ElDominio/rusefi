#include "pch.h"

#include "downshift_blipper.h"
#include "engine_state_machine.h"
#include "custom_page.h"

namespace {

DownshiftBlipper& dut() {
	return engine->module<DownshiftBlipper>().unmock();
}

void setDownshifting(bool v) {
	engine->module<EngineStateMachine>().unmock().engineSmIsDownshifting = v;
}

// Configure a 5-speed box where 4th = 1.00 and 3rd = 1.35, plus sane blipper limits.
// Gear ratios + rev limit stay on engine_configuration_s (page 1); blipper settings
// live on page 5 (getCustomPage()).
void setupBlipper() {
	engineConfiguration->totalGearsCount = 5;
	engineConfiguration->gearRatio[0] = 3.35f;
	engineConfiguration->gearRatio[1] = 2.20f;
	engineConfiguration->gearRatio[2] = 1.35f; // 3rd (target)
	engineConfiguration->gearRatio[3] = 1.00f; // 4th (pre-shift)
	engineConfiguration->gearRatio[4] = 0.80f;
	engineConfiguration->rpmHardLimit = 8000;

	// Distinct from 100 so tests can tell "open-loop ramp target" apart from a stray default.
	engineConfiguration->etbMaximumPosition = 90;

	auto cfg = getCustomPage();
	cfg->downshiftBlipperEnabled = true;
	cfg->downshiftBlipperRequireBrake = false;
	cfg->downshiftBlipperUseLuaGauge = false;
	cfg->downshiftBlipperMinVss = 10;
	cfg->downshiftBlipperMinRpm = 1500;
	cfg->downshiftBlipperMaxRpm = 7000;
	cfg->downshiftBlipperDriverTpsThreshold = 15;
	cfg->downshiftBlipperMaxTpsLimit = 40;
	cfg->downshiftBlipperMaxTimeMs = 250;
	cfg->downshiftBlipperLockoutTimeMs = 500;
	cfg->downshiftBlipRampOpenMs = 15;
	cfg->downshiftBlipRampCloseMs = 30;
	cfg->downshiftBlipOpenLoopWindowRpm = 100;

	// Strong proportional gain; error is normalized per-100-RPM so this drives to clamp quickly.
	cfg->downshiftBlipperKp = 5;
	cfg->downshiftBlipperKi = 0;
	cfg->downshiftBlipperKd = 0;
}

// Latch a stable pre-shift gear (held > 200 ms) before the clutch edge.
void latchGear(EngineTestHelper& eth, int gear) {
	Sensor::setMockValue(SensorType::DetectedGear, gear);
	dut().onFastCallback();      // establishes the candidate
	eth.moveTimeForwardMs(250);
	dut().onFastCallback();      // candidate now stable -> latched
}

void setSensors(float rpm, float vss, float driverTps) {
	Sensor::setMockValue(SensorType::Rpm, rpm);
	Sensor::setMockValue(SensorType::VehicleSpeed, vss);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, driverTps);
	Sensor::setMockValue(SensorType::Tps1, driverTps);
}

} // namespace

TEST(DownshiftBlipper, latchesTargetFromRatios) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	setSensors(/*rpm*/2700, /*vss*/80, /*tps*/0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback();

	EXPECT_TRUE(dut().isActive());
	EXPECT_EQ(4, dut().downshiftBlipPreShiftGear);
	EXPECT_EQ(3, dut().downshiftBlipTargetGear);
	// 2700 * (1.35 / 1.00) = 3645
	EXPECT_NEAR(3645, dut().downshiftBlipTargetRpm, 2);
}

TEST(DownshiftBlipper, opensLoopToEtbMaxBeforeRpmMatch) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback(); // RampOpen

	// RPM is nowhere near the target (3645 - 100 window), so we're still ramping open loop.
	// Past the ramp-open window, throttle should sit at etbMaximumPosition, NOT the blipper's
	// own (lower) maxTpsLimit -- the open-loop phase is not subject to that ceiling.
	eth.moveTimeForwardMs(20);
	dut().onFastCallback();
	EXPECT_TRUE(dut().isActive());
	EXPECT_NEAR(90, dut().getThrottleRequest(), 0.01); // etbMaximumPosition, > maxTpsLimit (40)
}

TEST(DownshiftBlipper, handsOffToPidHoldWithinOpenLoopWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback(); // RampOpen
	eth.moveTimeForwardMs(20);
	dut().onFastCallback(); // open loop, holding at etbMaximumPosition

	// RPM rises into the open-loop window (target 3645, window 100) -> hand off to PID hold,
	// which is clamped to the (lower) maxTpsLimit.
	setSensors(3600, 80, 0); // >= 3645 - 100
	dut().onFastCallback();
	EXPECT_TRUE(dut().isActive());
	EXPECT_GT(dut().getThrottleRequest(), 0);
	EXPECT_LE(dut().getThrottleRequest(), 40); // clamped to maxTpsLimit, not etbMaximumPosition

	// Holding the matched RPM is the point of the blip: it must NOT cut just because RPM is at
	// (or above) target. It keeps holding through further ticks while still downshifting.
	dut().onFastCallback();
	dut().onFastCallback();
	EXPECT_TRUE(dut().isActive());
	EXPECT_GT(dut().getThrottleRequest(), 0);
}

TEST(DownshiftBlipper, holdsThroughMaxTimeThenCuts) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback(); // RampOpen
	setSensors(3600, 80, 0); // immediately within the open-loop window -> PID hold
	dut().onFastCallback();
	ASSERT_TRUE(dut().isActive());

	// Keep holding at the matched RPM right up to (just under) the safety timeout.
	eth.moveTimeForwardMs(200);
	dut().onFastCallback();
	EXPECT_TRUE(dut().isActive());

	// Safety timeout (250ms from blip start) elapses -> ramp closed, then lockout.
	eth.moveTimeForwardMs(100);
	dut().onFastCallback();
	eth.moveTimeForwardMs(40);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
	EXPECT_FLOAT_EQ(0, dut().getThrottleRequest());
}

TEST(DownshiftBlipper, lockoutBlocksReTrigger) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback();
	eth.moveTimeForwardMs(20);
	dut().onFastCallback();
	ASSERT_TRUE(dut().isActive());

	// Clutch re-engages -> ramp closed, then lockout
	setDownshifting(false);
	dut().onFastCallback();
	eth.moveTimeForwardMs(40);
	dut().onFastCallback(); // now in lockout
	ASSERT_FALSE(dut().isActive());

	// A fresh downshift edge during lockout must NOT start a blip
	setSensors(2700, 80, 0);
	setDownshifting(false);
	dut().onFastCallback();
	setDownshifting(true);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());

	// After lockout expires, a new edge works again
	eth.moveTimeForwardMs(600);
	setDownshifting(false);
	dut().onFastCallback();
	setDownshifting(true);
	dut().onFastCallback();
	EXPECT_TRUE(dut().isActive());
}

TEST(DownshiftBlipper, abortsFromFirstGear) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	setSensors(2700, 80, 0);
	latchGear(eth, 1); // cannot downshift below 1st

	setDownshifting(true);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}

TEST(DownshiftBlipper, abortsWhenTargetOverRev) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();
	getCustomPage()->downshiftBlipperMaxRpm = 3000; // target 3645 exceeds this

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}

TEST(DownshiftBlipper, abortsWhenDriverOnThrottle) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	setSensors(2700, 80, /*tps*/50); // driver already blipping
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}

// Lua gauge gate is an inhibit, not a throttle multiplier: while it trips, a blip cannot start.
TEST(DownshiftBlipper, luaGateBlocksEntry) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	auto cfg = getCustomPage();
	cfg->downshiftBlipperUseLuaGauge = true;
	cfg->downshiftBlipperLuaGauge = 0; // LuaGauge1
	cfg->downshiftBlipperLuaGaugeMeaning = LUA_GAUGE_LOWER_BOUND; // trips when gauge >= threshold
	cfg->downshiftBlipperLuaGaugeThreshold = 50;
	Sensor::setMockValue(SensorType::LuaGauge1, 60); // >= 50 -> gate is blocking

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}

// If the gauge crosses the threshold mid-blip, the gate must cut the blip short, same as the
// driver retaking the pedal or the clutch re-engaging.
TEST(DownshiftBlipper, luaGateCutsActiveBlip) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	auto cfg = getCustomPage();
	cfg->downshiftBlipperUseLuaGauge = true;
	cfg->downshiftBlipperLuaGauge = 0; // LuaGauge1
	cfg->downshiftBlipperLuaGaugeMeaning = LUA_GAUGE_LOWER_BOUND;
	cfg->downshiftBlipperLuaGaugeThreshold = 50;
	Sensor::setMockValue(SensorType::LuaGauge1, 0); // below threshold -> not blocking yet

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback();
	eth.moveTimeForwardMs(20);
	dut().onFastCallback();
	ASSERT_TRUE(dut().isActive());

	// Gauge crosses the threshold mid-blip -> gate trips -> ramp closed, then lockout.
	Sensor::setMockValue(SensorType::LuaGauge1, 60);
	dut().onFastCallback();
	eth.moveTimeForwardMs(40);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}

// The TPS-to-RPM feed-forward curve is added on top of the PID output. With the PID gains
// zeroed, the commanded throttle should equal the looked-up feed-forward value at the target.
TEST(DownshiftBlipper, feedForwardSeedsThrottle) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	auto cfg = getCustomPage();
	cfg->downshiftBlipperKp = 0;
	cfg->downshiftBlipperKi = 0;
	cfg->downshiftBlipperKd = 0;
	// Flat 25% feed-forward across the whole RPM range.
	for (size_t i = 0; i < 10; i++) {
		cfg->tpsRpmFeedForwardBins[i] = i * 2000.0f;
		cfg->tpsRpmFeedForwardValues[i] = 25;
	}

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback();  // RampOpen, latches target 3645

	// Push RPM into the open-loop window so it hands off into the PID hold this tick.
	setSensors(3600, 80, 0);
	dut().onFastCallback();  // ActivePID: PID contributes 0, output is pure feed-forward

	EXPECT_FLOAT_EQ(25, dut().downshiftBlipPidOutput);
	EXPECT_NEAR(25, dut().getThrottleRequest(), 0.01);
}

TEST(DownshiftBlipper, cutsWhenClutchReleased) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback();
	eth.moveTimeForwardMs(20);
	dut().onFastCallback();
	ASSERT_TRUE(dut().isActive());

	// Clutch re-engages before RPM match
	setDownshifting(false);
	dut().onFastCallback();
	eth.moveTimeForwardMs(40);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}
