#include "pch.h"

#include "upshift_rpm_hold.h"
#include "engine_state_machine.h"
#include "custom_page.h"

namespace {

UpshiftRpmHold& dut() {
	return engine->module<UpshiftRpmHold>().unmock();
}

void setUpshifting(bool v) {
	engine->module<EngineStateMachine>().unmock().engineSmIsUpshifting = v;
}

// Configure a 5-speed box where 3rd = 1.35 (pre-shift) and 4th = 1.00 (target), plus sane
// hold limits. An upshift LOWERS RPM: 2700 * (1.00 / 1.35) = 2000.
void setupHold() {
	engineConfiguration->totalGearsCount = 5;
	engineConfiguration->gearRatio[0] = 3.35f;
	engineConfiguration->gearRatio[1] = 2.20f;
	engineConfiguration->gearRatio[2] = 1.35f; // 3rd (pre-shift)
	engineConfiguration->gearRatio[3] = 1.00f; // 4th (target)
	engineConfiguration->gearRatio[4] = 0.80f;
	engineConfiguration->rpmHardLimit = 8000;

	auto cfg = getCustomPage();
	cfg->upshiftRpmHoldEnabled = true;
	cfg->upshiftRpmHoldMinVss = 10;
	cfg->upshiftRpmHoldMinRpm = 1500;
	cfg->upshiftRpmHoldMaxRpm = 6500;
	cfg->upshiftRpmHoldDriverTpsThreshold = 15;
	cfg->upshiftRpmHoldMaxTpsLimit = 40;
	cfg->upshiftRpmHoldMaxTimeMs = 250;
	cfg->upshiftRpmHoldLockoutTimeMs = 500;
	cfg->upshiftRpmHoldRampDownMs = 15;
	cfg->upshiftRpmHoldRampUpMs = 30;

	cfg->upshiftRpmHoldKp = 5;
	cfg->upshiftRpmHoldKi = 0;
	cfg->upshiftRpmHoldKd = 0;

	// Flat 30% feed-forward: the hold commands throttle to keep RPM from falling below target.
	for (size_t i = 0; i < 10; i++) {
		cfg->tpsRpmFeedForwardBins[i] = i * 2000.0f;
		cfg->tpsRpmFeedForwardValues[i] = 30;
	}
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

TEST(UpshiftRpmHold, latchesTargetFromRatios) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupHold();

	setSensors(/*rpm*/2700, /*vss*/80, /*tps*/0);
	latchGear(eth, 3);

	setUpshifting(true);
	dut().onFastCallback();

	EXPECT_TRUE(dut().isActive());
	EXPECT_EQ(3, dut().upshiftHoldPreShiftGear);
	EXPECT_EQ(4, dut().upshiftHoldTargetGear);
	// 2700 * (1.00 / 1.35) = 2000
	EXPECT_NEAR(2000, dut().upshiftHoldTargetRpm, 2);
}

TEST(UpshiftRpmHold, holdsThroughMaxTimeThenReleases) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupHold();

	setSensors(2700, 80, 0);
	latchGear(eth, 3);

	setUpshifting(true);
	dut().onFastCallback(); // RampDown

	// RPM still above target (2700*1.00/1.35 = 1999) -> hold commands feed-forward throttle.
	setSensors(2100, 80, 0);
	eth.moveTimeForwardMs(20);
	dut().onFastCallback();
	EXPECT_GT(dut().getThrottleRequest(), 0);
	EXPECT_LE(dut().getThrottleRequest(), 40); // clamped to maxTpsLimit

	// RPM falls to/below the target: holding the matched RPM is the point of the hold, so it
	// must NOT release just because RPM reached target. It keeps holding through more ticks.
	setSensors(1900, 80, 0);
	dut().onFastCallback();
	dut().onFastCallback();
	EXPECT_TRUE(dut().isActive());
	EXPECT_GT(dut().getThrottleRequest(), 0);

	// Safety timeout (250ms from hold start) elapses -> ramp back to pedal, then lockout.
	eth.moveTimeForwardMs(250);
	dut().onFastCallback();
	eth.moveTimeForwardMs(40);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
	EXPECT_FLOAT_EQ(0, dut().getThrottleRequest());
}

TEST(UpshiftRpmHold, lockoutBlocksReTrigger) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupHold();

	setSensors(2700, 80, 0);
	latchGear(eth, 3);

	setUpshifting(true);
	dut().onFastCallback();
	eth.moveTimeForwardMs(20);
	dut().onFastCallback();
	ASSERT_TRUE(dut().isActive());

	// Clutch re-engages -> ramp back to pedal, then lockout
	setUpshifting(false);
	dut().onFastCallback();
	eth.moveTimeForwardMs(40);
	dut().onFastCallback(); // now in lockout
	ASSERT_FALSE(dut().isActive());

	// A fresh upshift edge during lockout must NOT start a hold
	setSensors(2700, 80, 0);
	setUpshifting(false);
	dut().onFastCallback();
	setUpshifting(true);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());

	// After lockout expires, a new edge works again
	eth.moveTimeForwardMs(600);
	setUpshifting(false);
	dut().onFastCallback();
	setUpshifting(true);
	dut().onFastCallback();
	EXPECT_TRUE(dut().isActive());
}

TEST(UpshiftRpmHold, abortsFromTopGear) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupHold();

	setSensors(2700, 80, 0);
	latchGear(eth, 5); // cannot upshift above top gear

	setUpshifting(true);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}

TEST(UpshiftRpmHold, abortsWhenAboveMaxRpm) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupHold();

	// Shift RPM above the configured over-rev lockout (6500) -> hold must not start,
	// even though the computed target (2000-equivalent scaling) would be a valid downward shift.
	setSensors(6600, 80, 0);
	latchGear(eth, 3);

	setUpshifting(true);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}

TEST(UpshiftRpmHold, abortsWhenDriverOnThrottle) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupHold();

	setSensors(2700, 80, /*tps*/50); // driver back on the throttle
	latchGear(eth, 3);

	setUpshifting(true);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}

TEST(UpshiftRpmHold, releasesWhenClutchEngaged) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupHold();

	setSensors(2700, 80, 0);
	latchGear(eth, 3);

	setUpshifting(true);
	dut().onFastCallback();
	eth.moveTimeForwardMs(20);
	dut().onFastCallback();
	ASSERT_TRUE(dut().isActive());

	// Clutch re-engages before RPM falls to target
	setUpshifting(false);
	dut().onFastCallback();
	eth.moveTimeForwardMs(40);
	dut().onFastCallback();
	EXPECT_FALSE(dut().isActive());
}
