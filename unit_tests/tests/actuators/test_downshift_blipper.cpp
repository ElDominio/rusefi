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
	cfg->downshiftBlipRpmOffset = 100;

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

TEST(DownshiftBlipper, opensThrottleThenCutsOnRpmMatch) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupBlipper();

	setSensors(2700, 80, 0);
	latchGear(eth, 4);

	setDownshifting(true);
	dut().onFastCallback(); // RampOpen

	// Push past the ramp-open window into closed-loop PID
	eth.moveTimeForwardMs(20);
	dut().onFastCallback();
	EXPECT_GT(dut().getThrottleRequest(), 0);
	EXPECT_LE(dut().getThrottleRequest(), 40); // clamped to maxTpsLimit

	// RPM reaches target minus the early-cut offset -> terminate
	setSensors(3600, 80, 0); // >= 3645 - 100
	dut().onFastCallback();

	// Ramp closed, then lockout drops it inactive
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
	setSensors(3600, 80, 0); // immediate match -> close
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
