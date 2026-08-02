/*
 * @file	test_tcu.cpp
 *
 * @date Oct 24, 2025
 * @author David Holdeman, (c) 2025
*/

#include "gear_controller.h"
#include "defaults.h"
#include "engine_state_machine.h"
#include "pch.h"

void blipGearControllerPin(EngineTestHelper* eth, brain_pin_e pin, int time) {
	engine->gearController->update();
	// Close switch/button
	setMockState(pin, false);
	engine->gearController->update();

	// update gearController every ms
	for (int i = 0; i < time; i = i+1000) {
		eth->moveTimeForwardAndInvokeEventsUs(minI(time - i, 1000));
		engine->gearController->update();
	}
	// And release
	setMockState(pin, true);
	engine->gearController->update();
}

TEST(tcu, testButtonshift) {
	EngineTestHelper eth(engine_type_e::TCU_4R70W);
	engineConfiguration->gearControllerMode = GearControllerMode::ButtonShift;
	initGearController();

	// pinMode is PI_PULLUP, so true = off
	setMockState(engineConfiguration->tcuUpshiftButtonPin, true);
	setMockState(engineConfiguration->tcuDownshiftButtonPin, true);

	ASSERT_NE(nullptr, engine->gearController);
	ASSERT_EQ(NEUTRAL, engine->gearController->getDesiredGear());

	// Press upshift button for 200ms
	blipGearControllerPin(&eth, engineConfiguration->tcuUpshiftButtonPin, 200000);

	// and now a bounce
	eth.moveTimeForwardAndInvokeEventsUs(20);
	blipGearControllerPin(&eth, engineConfiguration->tcuUpshiftButtonPin, 20);

	ASSERT_EQ(GEAR_1, engine->gearController->getDesiredGear());

	// Wait 500ms
	eth.moveTimeForwardAndInvokeEventsUs(500000);
	// Press upshift button for 200ms
	blipGearControllerPin(&eth, engineConfiguration->tcuUpshiftButtonPin, 200000);

	ASSERT_EQ(GEAR_2, engine->gearController->getDesiredGear());

	// Wait 500ms
	eth.moveTimeForwardAndInvokeEventsUs(500000);
	// Press upshift button for 1ms
	blipGearControllerPin(&eth, engineConfiguration->tcuUpshiftButtonPin, 1000);

	ASSERT_EQ(GEAR_3, engine->gearController->getDesiredGear());

	// Wait 500ms
	eth.moveTimeForwardAndInvokeEventsUs(500000);
	// Press upshift button for 3s
	blipGearControllerPin(&eth, engineConfiguration->tcuUpshiftButtonPin, 3000000);

	ASSERT_EQ(GEAR_4, engine->gearController->getDesiredGear());

	// Wait 10ms
	// Because this is a different pin, the 500ms debounce timeout is not in play.
	eth.moveTimeForwardAndInvokeEventsUs(10000);
	// Press downshift button for 200ms
	blipGearControllerPin(&eth, engineConfiguration->tcuDownshiftButtonPin, 200000);

	ASSERT_EQ(GEAR_3, engine->gearController->getDesiredGear());

	// Wait 10ms
	// This shouldn't be long enough for the debounce to have reset,
	// so the downshift won't trigger until ~490ms after pressing the button.
	eth.moveTimeForwardAndInvokeEventsUs(10000);
	// Press downshift button for 1.2s
	blipGearControllerPin(&eth, engineConfiguration->tcuDownshiftButtonPin, 1200000);

	ASSERT_EQ(GEAR_1, engine->gearController->getDesiredGear());
}

TEST(tcu, testGenericGC) {
	EngineTestHelper eth(engine_type_e::TCU_4R70W);
	engineConfiguration->gearControllerMode = GearControllerMode::Generic;
	initGearController();

	// Need to set some engine settings for airmass calc
	engineConfiguration->cylindersCount = 8.0;

	// pinMode is PI_PULLUP, so true = off
	setMockState(engineConfiguration->tcuUpshiftButtonPin, true);
	setMockState(engineConfiguration->tcuDownshiftButtonPin, true);
	setMockState(engineConfiguration->tcu_rangeInput[1], true);
	setMockState(engineConfiguration->tcu_rangeInput[2], true);

	ASSERT_NE(nullptr, engine->gearController);
	ASSERT_EQ(NEUTRAL, engine->gearController->getDesiredGear());

	Sensor::setMockValue(SensorType::VehicleSpeed, 55);
	Sensor::setMockValue(SensorType::Rpm, 2500);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 15);
	Sensor::setMockValue(SensorType::Maf, 0.1f);

	engine->gearController->update();
	// Make sure we stay in neutral with undefined range selector pins
	ASSERT_EQ(NEUTRAL, engine->gearController->getDesiredGear());

	Sensor::setMockValue(SensorType::RangeInput1, 2000);
	engine->gearController->update();
	ASSERT_EQ(GEAR_2, engine->gearController->getDesiredGear());
}

// "Transmission Settings" no longer exposes the Gear Controller / Transmission Controller
// dropdowns, so a fresh tune that leaves them at None must get a working default once TCU
// Enabled is set, via defaultsOrFixOnBurn().
TEST(tcu, testDefaultModeOnEnable) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	engineConfiguration->tcuEnabled = true;
	engineConfiguration->gearControllerMode = GearControllerMode::None;
	engineConfiguration->transmissionControllerMode = TransmissionControllerMode::None;

	defaultsOrFixOnBurn();

	ASSERT_EQ(GearControllerMode::Automatic, engineConfiguration->gearControllerMode);
	ASSERT_EQ(TransmissionControllerMode::Generic4, engineConfiguration->transmissionControllerMode);
}

// With TCU Enabled off, defaultsOrFixOnBurn() shouldn't force a mode -- nothing to run anyway.
TEST(tcu, testModeNotForcedWhenTcuDisabled) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	engineConfiguration->tcuEnabled = false;
	engineConfiguration->gearControllerMode = GearControllerMode::None;
	engineConfiguration->transmissionControllerMode = TransmissionControllerMode::None;

	defaultsOrFixOnBurn();

	ASSERT_EQ(GearControllerMode::None, engineConfiguration->gearControllerMode);
	ASSERT_EQ(TransmissionControllerMode::None, engineConfiguration->transmissionControllerMode);
}

// Automatic + Generic4 is the only mode combination reachable through the UI (the Gear/
// Transmission Controller dropdowns and the Button Shift / range-selector dialogs those other
// modes need are gone), so a tune left on a different mode -- whether from the old
// configureTcu4R70W() preset or a tune saved before this UI change -- gets forced onto
// Automatic/Generic4 on every boot rather than silently sitting inert with no way to fix it
// through TS.
TEST(tcu, testDefaultModeOverridesExplicitChoice) {
	EngineTestHelper eth(engine_type_e::TCU_4R70W);

	// Simulate a tune saved with a different mode from before this UI change (or from the old
	// configureTcu4R70W() preset) -- by the time EngineTestHelper finishes booting,
	// gearControllerMode has already been forced to Automatic once (startup runs
	// defaultsOrFixOnBurn() too), so set it back to Generic here to exercise the real scenario:
	// a stale persisted value being loaded at boot.
	engineConfiguration->gearControllerMode = GearControllerMode::Generic;

	defaultsOrFixOnBurn();

	ASSERT_EQ(GearControllerMode::Automatic, engineConfiguration->gearControllerMode);
	ASSERT_EQ(TransmissionControllerMode::Generic4, engineConfiguration->transmissionControllerMode);
}

TEST(tcu, testIdleShiftToFirst) {
	EngineTestHelper eth(engine_type_e::TCU_4R70W);
	engineConfiguration->gearControllerMode = GearControllerMode::Automatic;
	initGearController();

	ASSERT_NE(nullptr, engine->gearController);
	ASSERT_NE(nullptr, engine->gearController->transmissionController);
	ASSERT_EQ(NEUTRAL, engine->gearController->getDesiredGear());

	// Drive into 2nd gear: enough speed/throttle to clear the 1->2 shift point. Not idle.
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 44);
	Sensor::setMockValue(SensorType::VehicleSpeed, 30);
	engine->gearController->update();
	ASSERT_EQ(GEAR_2, engine->gearController->getDesiredGear());
	ASSERT_FALSE(engine->gearController->transmissionController->tcu_idleShiftToFirst);

	// The Engine State Machine says idle -- same source MisfireController et al use, not a
	// separate RPM/TPS check. Vehicle stopped; default tcuIdleShiftToFirstMaxVss is 0 (ignore
	// speed) so this would force the downshift even if the VSS reading were noisy/nonzero.
	engine->module<EngineStateMachine>().unmock().engineSmIsIdle = true;
	Sensor::setMockValue(SensorType::VehicleSpeed, 0);
	engine->gearController->update();

	ASSERT_EQ(GEAR_1, engine->gearController->getDesiredGear());
	ASSERT_TRUE(engine->gearController->transmissionController->tcu_idleShiftToFirst);

	// And the flag clears once no longer idle.
	engine->module<EngineStateMachine>().unmock().engineSmIsIdle = false;
	engine->gearController->update();
	ASSERT_FALSE(engine->gearController->transmissionController->tcu_idleShiftToFirst);
}

// "Shift to First if Idle" off -> no forced downshift even when the state machine says idle.
TEST(tcu, testIdleShiftToFirstDisabled) {
	EngineTestHelper eth(engine_type_e::TCU_4R70W);
	engineConfiguration->gearControllerMode = GearControllerMode::Automatic;
	config->tcuIdleShiftToFirstEnabled = false;
	initGearController();

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 44);
	Sensor::setMockValue(SensorType::VehicleSpeed, 30);
	engine->gearController->update();
	ASSERT_EQ(GEAR_2, engine->gearController->getDesiredGear());

	engine->module<EngineStateMachine>().unmock().engineSmIsIdle = true;
	engine->gearController->update();

	ASSERT_EQ(GEAR_2, engine->gearController->getDesiredGear());
	ASSERT_FALSE(engine->gearController->transmissionController->tcu_idleShiftToFirst);
}

// A non-zero tcuIdleShiftToFirstMaxVss adds a speed gate on top of the state machine's idle
// condition -- idle-shift is blocked above the threshold, applies below it.
TEST(tcu, testIdleShiftToFirstVssThreshold) {
	EngineTestHelper eth(engine_type_e::TCU_4R70W);
	engineConfiguration->gearControllerMode = GearControllerMode::Automatic;
	config->tcuIdleShiftToFirstMaxVss = 5; // km/h
	initGearController();

	// TPS=11 exactly hits tcu_shiftTpsBins[0] (tcu_shiftSpeed12[0]=10, tcu_shiftSpeed21[0]=5,
	// tcu_shiftSpeed23[0]=20), keeping the ordinary VSS-based shift schedule out of the way of
	// the VSS values used below so only the idle-shift feature is under test.
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 11);
	Sensor::setMockValue(SensorType::VehicleSpeed, 15);
	engine->gearController->update();
	ASSERT_EQ(GEAR_2, engine->gearController->getDesiredGear());

	// State machine says idle, but VSS (10) is still above the configured 5 km/h threshold.
	engine->module<EngineStateMachine>().unmock().engineSmIsIdle = true;
	Sensor::setMockValue(SensorType::VehicleSpeed, 10);
	engine->gearController->update();
	ASSERT_EQ(GEAR_2, engine->gearController->getDesiredGear());
	ASSERT_FALSE(engine->gearController->transmissionController->tcu_idleShiftToFirst);

	// Slow down below the threshold -> the idle-shift now applies.
	Sensor::setMockValue(SensorType::VehicleSpeed, 3);
	engine->gearController->update();
	ASSERT_EQ(GEAR_1, engine->gearController->getDesiredGear());
	ASSERT_TRUE(engine->gearController->transmissionController->tcu_idleShiftToFirst);
}

TEST(tcu, testAutomaticGaugeFields) {
	EngineTestHelper eth(engine_type_e::TCU_4R70W);
	engineConfiguration->gearControllerMode = GearControllerMode::Automatic;
	initGearController();

	ASSERT_NE(nullptr, engine->gearController);
	TransmissionControllerBase* tc = engine->gearController->transmissionController;
	ASSERT_NE(nullptr, tc);

	// TPS=44 lands exactly on a shift-table bin, avoiding interpolation rounding.
	Sensor::setMockValue(SensorType::Rpm, 3000);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 44);
	Sensor::setMockValue(SensorType::VehicleSpeed, 30);

	// First update: 1->2 upshift (30 > tcu_shiftSpeed12[3]=23).
	engine->gearController->update();
	ASSERT_EQ(GEAR_2, engine->gearController->getDesiredGear());

	// Second update, same inputs: now in gear 2, so shift() runs for the 2->1 and 2->3 edges,
	// publishing fresh margins for the current gear (neither edge triggers a further shift).
	engine->gearController->update();
	ASSERT_EQ(GEAR_2, engine->gearController->getDesiredGear());

	// tcu_shiftSpeed21[3]=17.0 -> downshiftMargin = speed(30) - 17 = 13
	EXPECT_NEAR(13.0f, tc->tcu_downshiftMargin, 0.15f);
	// tcu_shiftSpeed23[3]=37.0 -> upshiftMargin = 37 - speed(30) = 7
	EXPECT_NEAR(7.0f, tc->tcu_upshiftMargin, 0.15f);

	// configureTcu4R70W()'s solenoid table has both solenoids off for gear 2.
	ASSERT_FALSE(tc->tcu_solenoid1On);
	ASSERT_FALSE(tc->tcu_solenoid2On);
}
