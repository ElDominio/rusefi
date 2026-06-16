#include "pch.h"
#include "custom_page.h"
#include "launch_power_ramp.h"

// Launch Power Ramp: after launch control releases at WOT, ignition timing is pulled per the
// time-vs-retard curve and ramps back to zero over up to 5s (time-based, throttle-independent).

static LaunchPowerRamp& getRamp() {
	return engine->module<LaunchPowerRamp>().unmock();
}

static void setupRamp() {
	auto* d = getCustomPage();
	d->launchPowerRampEnabled = true;
	d->smWotTpsThreshold = 90; // WOT gate

	// Simple ramp: 10 deg at t=0, decaying linearly to 0 at t=5s.
	for (int i = 0; i < LAUNCH_POWER_RAMP_CURVE_SIZE; i++) {
		float t = i * (5.0f / (LAUNCH_POWER_RAMP_CURVE_SIZE - 1)); // 0 .. 5s
		d->launchPowerRampTimeBins[i] = t;
		d->launchPowerRampRetardValues[i] = 10.0f * (1.0f - t / 5.0f);
	}
}

// Helper: drive launch-active state and run the slow callback that the ramp hooks into.
static void launchActive(EngineTestHelper& eth, bool active) {
	engine->launchController.isLaunchCondition = active;
	getRamp().onSlowCallback();
	(void)eth;
}

TEST(LaunchPowerRamp, startsOnLaunchReleaseAtWotAndRampsDown) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRamp();

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 100.0f); // WOT

	// Sitting in launch: no retard yet.
	launchActive(eth, true);
	EXPECT_FALSE(getRamp().isLaunchPowerRampActive);
	EXPECT_NEAR(getRamp().getTimingRetard(), 0.0f, 1e-3);

	// Release launch while at WOT -> ramp starts, full retard at t=0.
	launchActive(eth, false);
	EXPECT_TRUE(getRamp().isLaunchPowerRampActive);
	EXPECT_NEAR(getRamp().getTimingRetard(), 10.0f, 0.1f);

	// Halfway through (2.5s) -> ~5 deg.
	eth.moveTimeForwardSec(2.5f);
	getRamp().onSlowCallback();
	EXPECT_NEAR(getRamp().getTimingRetard(), 5.0f, 0.2f);

	// Past the end of the curve (>5s) -> deactivates, no retard.
	eth.moveTimeForwardSec(3.0f);
	getRamp().onSlowCallback();
	EXPECT_FALSE(getRamp().isLaunchPowerRampActive);
	EXPECT_NEAR(getRamp().getTimingRetard(), 0.0f, 1e-3);
}

TEST(LaunchPowerRamp, doesNotStartWhenNotWot) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRamp();

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f); // below WOT gate

	launchActive(eth, true);
	launchActive(eth, false); // release, but not at WOT
	EXPECT_FALSE(getRamp().isLaunchPowerRampActive);
	EXPECT_NEAR(getRamp().getTimingRetard(), 0.0f, 1e-3);
}

TEST(LaunchPowerRamp, continuesAfterThrottleLift) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRamp();

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 100.0f);
	launchActive(eth, true);
	launchActive(eth, false); // start the ramp at WOT
	ASSERT_TRUE(getRamp().isLaunchPowerRampActive);

	// Driver lifts off mid-ramp: the ramp is purely time-based and keeps running.
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 10.0f);
	eth.moveTimeForwardSec(2.5f);
	getRamp().onSlowCallback();
	EXPECT_TRUE(getRamp().isLaunchPowerRampActive);
	EXPECT_NEAR(getRamp().getTimingRetard(), 5.0f, 0.2f);
}

TEST(LaunchPowerRamp, disabledDoesNothing) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRamp();
	getCustomPage()->launchPowerRampEnabled = false;

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 100.0f);
	launchActive(eth, true);
	launchActive(eth, false);
	EXPECT_FALSE(getRamp().isLaunchPowerRampActive);
	EXPECT_NEAR(getRamp().getTimingRetard(), 0.0f, 1e-3);
}
