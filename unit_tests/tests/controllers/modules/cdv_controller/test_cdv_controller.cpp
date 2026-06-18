#include "pch.h"
#include "custom_page.h"
#include "cdv_controller.h"

// Clutch Delay Valve: Simple mode activates on launch/pre-launch entry and stays active until
// a time/VSS/clutch exit fires. Smart mode additionally gates activation on clutch pressure
// sitting inside a configured window, and leaving that window is itself an exit condition.
// Time-based exit means different things per mode: Simple counts from launch exit, Smart is a
// max-active-duration safety timeout counted from when the solenoid turned on.

static CdvController& getCdv() {
	return engine->module<CdvController>().unmock();
}

static void setupCdv() {
	engineConfiguration->cdvControlEnabled = true;
	engineConfiguration->cdvSmartMode = false;
	engineConfiguration->cdvUseClutchExit = false;
	auto* d = getCustomPage();
	d->cdvExitDelay = 1.0f; // seconds
	d->cdvExitVss = 0; // disabled
	d->cdvSmartMinPressure = 100;
	d->cdvSmartMaxPressure = 300;
}

TEST(CdvController, simpleModeActivatesOnLaunchAndExitsAfterDelay) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupCdv();

	engine->launchController.isLaunchCondition = true;
	getCdv().onSlowCallback();
	EXPECT_TRUE(enginePins.cdvSolenoid.getLogicValue());

	// Launch releases: still active, exit timer just started.
	engine->launchController.isLaunchCondition = false;
	getCdv().onSlowCallback();
	EXPECT_TRUE(enginePins.cdvSolenoid.getLogicValue());

	// Not yet past the exit delay.
	eth.moveTimeForwardSec(0.5f);
	getCdv().onSlowCallback();
	EXPECT_TRUE(enginePins.cdvSolenoid.getLogicValue());

	// Past the exit delay -> deactivates.
	eth.moveTimeForwardSec(0.6f);
	getCdv().onSlowCallback();
	EXPECT_FALSE(enginePins.cdvSolenoid.getLogicValue());
}

TEST(CdvController, smartModeRequiresPressureInWindowToActivate) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupCdv();
	engineConfiguration->cdvSmartMode = true;

	// Launch is active, but clutch pressure is below the window -> no activation.
	Sensor::setMockValue(SensorType::ClutchPressure, 50.0f);
	engine->launchController.isLaunchCondition = true;
	getCdv().onSlowCallback();
	EXPECT_FALSE(enginePins.cdvSolenoid.getLogicValue());

	// Pressure rises into the window -> activates.
	Sensor::setMockValue(SensorType::ClutchPressure, 200.0f);
	getCdv().onSlowCallback();
	EXPECT_TRUE(enginePins.cdvSolenoid.getLogicValue());
}

TEST(CdvController, smartModeLeavingWindowDeactivatesImmediately) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupCdv();
	engineConfiguration->cdvSmartMode = true;

	Sensor::setMockValue(SensorType::ClutchPressure, 200.0f);
	engine->launchController.isLaunchCondition = true;
	getCdv().onSlowCallback();
	ASSERT_TRUE(enginePins.cdvSolenoid.getLogicValue());

	// Pressure exits the window (clutch fully released) -> CDV drops immediately,
	// well before the exit-delay safety timeout would fire.
	Sensor::setMockValue(SensorType::ClutchPressure, 350.0f);
	getCdv().onSlowCallback();
	EXPECT_FALSE(enginePins.cdvSolenoid.getLogicValue());
}

TEST(CdvController, smartModeMaxActiveDurationForcesExitEvenInsideWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupCdv();
	engineConfiguration->cdvSmartMode = true;

	Sensor::setMockValue(SensorType::ClutchPressure, 200.0f);
	engine->launchController.isLaunchCondition = true;
	getCdv().onSlowCallback();
	ASSERT_TRUE(enginePins.cdvSolenoid.getLogicValue());

	// Pressure stays inside the window the whole time, launch stays active too --
	// only the max-active-duration safety timer can end this.
	eth.moveTimeForwardSec(0.5f);
	getCdv().onSlowCallback();
	EXPECT_TRUE(enginePins.cdvSolenoid.getLogicValue());

	eth.moveTimeForwardSec(0.6f);
	getCdv().onSlowCallback();
	EXPECT_FALSE(enginePins.cdvSolenoid.getLogicValue());
}

TEST(CdvController, smartModeVssExitStillAppliesInsideWindow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupCdv();
	engineConfiguration->cdvSmartMode = true;
	getCustomPage()->cdvExitVss = 20;

	Sensor::setMockValue(SensorType::ClutchPressure, 200.0f);
	Sensor::setMockValue(SensorType::VehicleSpeed, 0.0f);
	engine->launchController.isLaunchCondition = true;
	getCdv().onSlowCallback();
	ASSERT_TRUE(enginePins.cdvSolenoid.getLogicValue());

	// Vehicle speed exceeds threshold while still inside the pressure window -> forced off.
	Sensor::setMockValue(SensorType::VehicleSpeed, 25.0f);
	getCdv().onSlowCallback();
	EXPECT_FALSE(enginePins.cdvSolenoid.getLogicValue());
}

TEST(CdvController, smartModeClutchReleaseExitStillApplies) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupCdv();
	engineConfiguration->cdvSmartMode = true;
	engineConfiguration->cdvUseClutchExit = true;

	Sensor::setMockValue(SensorType::ClutchPressure, 200.0f);
	engine->engineState.clutchDownState = true;
	engine->launchController.isLaunchCondition = true;
	getCdv().onSlowCallback();
	ASSERT_TRUE(enginePins.cdvSolenoid.getLogicValue());

	// Clutch pedal released (falling edge) while still inside the pressure window -> forced off.
	engine->engineState.clutchDownState = false;
	getCdv().onSlowCallback();
	EXPECT_FALSE(enginePins.cdvSolenoid.getLogicValue());
}

TEST(CdvController, disabledDoesNothing) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupCdv();
	engineConfiguration->cdvControlEnabled = false;

	engine->launchController.isLaunchCondition = true;
	getCdv().onSlowCallback();
	EXPECT_FALSE(enginePins.cdvSolenoid.getLogicValue());
}
