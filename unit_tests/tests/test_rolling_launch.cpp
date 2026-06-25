#include "pch.h"
#include "custom_page.h"
#include "rolling_launch.h"

// Rolling Launch Control: a button-held, captured-RPM hold (via the shared hard spark limiter)
// to spool the turbo while the car is moving. Snapshots RPM at the press edge, holds at
// capturedRpm + window (clamped to maxRpm), pulls flat timing + adds fuel while held, and on
// release ends the hold immediately while ramping the pulled timing back to zero.

static RollingLaunchControl& getRl() {
	return engine->module<RollingLaunchControl>().unmock();
}

static void setupRl() {
	auto* d = getCustomPage();
	d->rollingLaunchEnabled = true;
	d->rollingLaunchMinVss = 10;          // km/h
	d->rollingLaunchMinArmRpm = 2500;
	d->rollingLaunchRpmWindow = 500;
	d->rollingLaunchMaxRpm = 7000;
	d->rollingLaunchTimingRetard = 10;    // deg
	d->rollingLaunchFuelAdderPercent = 5; // %
	d->rollingLaunchRampOutTime = 1.0f;   // s
}

// Drive RPM, VSS and the Lua activation, then run the slow callback the module hooks into.
static void step(float rpm, float vss, bool active) {
	Sensor::setMockValue(SensorType::Rpm, rpm);
	Sensor::setMockValue(SensorType::VehicleSpeed, vss);
	getRl().luaRollingLaunchState = active;
	getRl().onSlowCallback();
}

TEST(RollingLaunch, doesNotArmBelowSpeedOrRpm) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRl();

	// Button held but stationary -> not armed (this is the "rolling" gate).
	step(3500, 0, true);
	EXPECT_FALSE(getRl().isRollingLaunchArmed);
	EXPECT_NEAR(getRl().getSparkSkipRatio(), 0.0f, 1e-3);

	// Button held, moving, but lugging below the min arm RPM -> not armed.
	step(1500, 30, true);
	EXPECT_FALSE(getRl().isRollingLaunchArmed);

	// Button held, moving, above min arm RPM -> armed.
	step(3500, 30, true);
	EXPECT_TRUE(getRl().isRollingLaunchArmed);
}

TEST(RollingLaunch, capturesAndClampsTarget) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRl();

	// Press at 3500 with a 500 window -> target 4000.
	step(3500, 30, true);
	EXPECT_NEAR(getRl().capturedRpm, 3500.0f, 1e-3);
	EXPECT_NEAR(getRl().rollingLaunchTargetRpm, 4000.0f, 1e-3);

	// Release, then press again near redline -> target clamps to the max-RPM ceiling.
	step(3500, 30, false);
	step(6800, 30, true);
	EXPECT_NEAR(getRl().capturedRpm, 6800.0f, 1e-3);
	EXPECT_NEAR(getRl().rollingLaunchTargetRpm, 7000.0f, 1e-3); // min(6800+500, 7000)
}

TEST(RollingLaunch, sparkSkipRampsAcrossWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRl();

	// Capture at 3500 -> [3500, 4000].
	step(3500, 30, true);
	EXPECT_NEAR(getRl().getSparkSkipRatio(), 0.0f, 1e-3);

	// Midway through the window -> ~0.5.
	step(3750, 30, true);
	EXPECT_NEAR(getRl().getSparkSkipRatio(), 0.5f, 1e-3);

	// At/above the target -> full cut.
	step(4000, 30, true);
	EXPECT_NEAR(getRl().getSparkSkipRatio(), 1.0f, 1e-3);
	step(4500, 30, true);
	EXPECT_NEAR(getRl().getSparkSkipRatio(), 1.0f, 1e-3);
}

TEST(RollingLaunch, fuelCoefficientOnlyWhileHeld) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRl();

	// Not armed -> no enrichment.
	step(3500, 0, true);
	EXPECT_NEAR(getRl().getFuelCoefficient(), 1.0f, 1e-3);

	// Armed -> +5%.
	step(3500, 30, true);
	EXPECT_NEAR(getRl().getFuelCoefficient(), 1.05f, 1e-3);

	// Released -> back to 1.0.
	step(3500, 30, false);
	EXPECT_NEAR(getRl().getFuelCoefficient(), 1.0f, 1e-3);
}

TEST(RollingLaunch, releaseDropsHoldAndRampsOutTiming) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRl();

	// Capture at 3500 -> target 4000; flat retard applies immediately while armed.
	step(3500, 30, true);
	EXPECT_NEAR(getRl().getTimingRetard(), 10.0f, 1e-3);

	// Rev to the hold target -> full spark cut, retard still flat at 10.
	step(4000, 30, true);
	EXPECT_NEAR(getRl().getTimingRetard(), 10.0f, 1e-3);
	EXPECT_NEAR(getRl().getSparkSkipRatio(), 1.0f, 1e-3);

	// Release: hold ends immediately (no spark cut), ramp starts at the full pulled value.
	step(4000, 30, false);
	EXPECT_FALSE(getRl().isRollingLaunchArmed);
	EXPECT_NEAR(getRl().getSparkSkipRatio(), 0.0f, 1e-3);
	EXPECT_TRUE(getRl().isRollingLaunchRampActive);
	EXPECT_NEAR(getRl().getTimingRetard(), 10.0f, 0.1f);

	// Halfway through the 1s ramp-out -> ~5 deg.
	eth.moveTimeForwardSec(0.5f);
	getRl().onSlowCallback();
	EXPECT_NEAR(getRl().getTimingRetard(), 5.0f, 0.2f);

	// Past the ramp-out time -> deactivates, no retard.
	eth.moveTimeForwardSec(0.6f);
	getRl().onSlowCallback();
	EXPECT_FALSE(getRl().isRollingLaunchRampActive);
	EXPECT_NEAR(getRl().getTimingRetard(), 0.0f, 1e-3);
}

TEST(RollingLaunch, disabledDoesNothing) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRl();
	getCustomPage()->rollingLaunchEnabled = false;

	step(4000, 30, true);
	EXPECT_FALSE(getRl().isRollingLaunchArmed);
	EXPECT_NEAR(getRl().getSparkSkipRatio(), 0.0f, 1e-3);
	EXPECT_NEAR(getRl().getFuelCoefficient(), 1.0f, 1e-3);
	EXPECT_NEAR(getRl().getTimingRetard(), 0.0f, 1e-3);
}
