#include "pch.h"
#include "custom_page.h"

// Helper: set all three dual-mode activation thresholds
static void setDualActivation(uint16_t rpm, uint8_t load, uint8_t tps) {
	getCustomPage()->secondaryFpActivationRpm  = rpm;
	getCustomPage()->secondaryFpActivationLoad = load;
	getCustomPage()->secondaryFpActivationTps  = tps;
}

// Helper: set all three dual-mode hysteresis values
static void setDualHysteresis(uint16_t rpm, uint8_t load, uint8_t tps) {
	getCustomPage()->secondaryFpRpmHysteresis  = rpm;
	getCustomPage()->secondaryFpLoadHysteresis = load;
	getCustomPage()->secondaryFpTpsHysteresis  = tps;
}

// Helper: configure secondary pump pin and re-init its GPIO
static void setupDualPin() {
	engineConfiguration->fuelPump2Pin = Gpio::B0;
	enginePins.fuelPumpRelay2.init();
}

TEST(FuelPumpPwm, SingleModeUnchanged) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	engineConfiguration->fuelPumpPin  = Gpio::A0;
	getCustomPage()->fuelPumpMode = FP_MODE_SINGLE;
	enginePins.fuelPumpRelay.init();

	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();
	EXPECT_TRUE(efiReadPin(Gpio::A0));   // pump priming

	advanceTimeUs(10e6);
	dut.onSlowCallback();
	EXPECT_FALSE(efiReadPin(Gpio::A0));  // prime expired, no trigger
}

TEST(FuelPumpPwm, DualModeSecondaryOffBelowThresholds) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	engineConfiguration->fuelPumpPin  = Gpio::A0;
	getCustomPage()->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualHysteresis(500, 10, 10);

	// Primary priming, but RPM/load/TPS all zero (below thresholds)
	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();

	EXPECT_TRUE(efiReadPin(Gpio::A0));   // primary on (priming)
	EXPECT_FALSE(efiReadPin(Gpio::B0));  // secondary off
}

TEST(FuelPumpPwm, DualModeSecondaryOnAboveAllThresholds) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	engineConfiguration->fuelPumpPin  = Gpio::A0;
	getCustomPage()->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualHysteresis(500, 10, 10);

	Sensor::setMockValue(SensorType::Rpm, 4000);
	// fuelingLoad is used for "load"; set it via mock
	getEngineState()->fuelingLoad = 80.0f;
	Sensor::setMockValue(SensorType::Tps1, 70);

	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();

	EXPECT_TRUE(efiReadPin(Gpio::A0));   // primary on
	EXPECT_TRUE(efiReadPin(Gpio::B0));   // secondary on
}

TEST(FuelPumpPwm, DualModeSecondaryStaysOffIfOnlyOneConditionMet) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	engineConfiguration->fuelPumpPin  = Gpio::A0;
	getCustomPage()->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualHysteresis(500, 10, 10);

	// RPM above threshold but load and TPS below
	Sensor::setMockValue(SensorType::Rpm, 4000);
	getEngineState()->fuelingLoad = 30.0f;  // below 60%
	Sensor::setMockValue(SensorType::Tps1, 20);  // below 50%

	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();

	EXPECT_TRUE(efiReadPin(Gpio::A0));   // primary on
	EXPECT_FALSE(efiReadPin(Gpio::B0));  // secondary stays off
}

TEST(FuelPumpPwm, DualHysteresisStaysOnUntilRpmDropsBelow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	engineConfiguration->fuelPumpPin  = Gpio::A0;
	getCustomPage()->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualHysteresis(500, 10, 10);

	// All conditions above activation thresholds — secondary turns on
	Sensor::setMockValue(SensorType::Rpm, 4000);
	getEngineState()->fuelingLoad = 80.0f;
	Sensor::setMockValue(SensorType::Tps1, 70);

	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();
	EXPECT_TRUE(efiReadPin(Gpio::B0));  // secondary on

	// Drop RPM below deactivation point (3000 - 500 = 2500) — secondary must turn off
	Sensor::setMockValue(SensorType::Rpm, 2000);  // below deactivation point 2500
	dut.onSlowCallback();
	EXPECT_FALSE(efiReadPin(Gpio::B0));  // secondary off

	// Restore RPM above activation — secondary should NOT come back until ALL conditions met
	Sensor::setMockValue(SensorType::Rpm, 4000);
	dut.onSlowCallback();
	EXPECT_TRUE(efiReadPin(Gpio::B0));  // secondary on again (all three above thresholds)
}

TEST(FuelPumpPwm, DualHysteresisOrLogicOnDeactivation) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	engineConfiguration->fuelPumpPin  = Gpio::A0;
	getCustomPage()->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualHysteresis(500, 10, 10);

	// Activate secondary
	Sensor::setMockValue(SensorType::Rpm, 4000);
	getEngineState()->fuelingLoad = 80.0f;
	Sensor::setMockValue(SensorType::Tps1, 70);
	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();
	EXPECT_TRUE(efiReadPin(Gpio::B0));

	// Drop only load below its deactivation point (60 - 10 = 50); RPM and TPS remain above activation
	getEngineState()->fuelingLoad = 30.0f;  // below deactivation point 50%
	dut.onSlowCallback();
	// OR logic: secondary must turn off even though RPM and TPS are fine
	EXPECT_FALSE(efiReadPin(Gpio::B0));
}

TEST(FuelPumpPwm, PwmGetSetpointFromTable) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	getCustomPage()->fuelPumpMode = FP_MODE_PWM;

	// Fill target table with a constant 300 kPa
	for (int i = 0; i < FP_PRESSURE_TABLE_SIZE; i++) {
		for (int j = 0; j < FP_PRESSURE_RPM_SIZE; j++) {
			config->fuelPressureTargetTable[i][j] = 300;
		}
	}

	// Pump must be on for getSetpoint to return a value
	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();

	auto setpoint = dut.getSetpoint();
	ASSERT_TRUE(setpoint.Valid);
	EXPECT_NEAR(300.0f, setpoint.Value, 1.0f);
}

TEST(FuelPumpPwm, PwmOpenLoopNoSensor) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	getCustomPage()->fuelPumpMode = FP_MODE_PWM;

	// Fill base duty table: at 300 kPa target -> 50% duty
	for (int i = 0; i < FP_DUTY_TABLE_SIZE; i++) {
		for (int j = 0; j < FP_DUTY_RPM_SIZE; j++) {
			config->fuelPumpBaseDutyTable[i][j] = 50;
		}
	}

	// No fuel pressure sensor configured — observePlant returns unexpected
	EXPECT_FALSE(dut.observePlant().Valid);

	// Open loop at 300 kPa target should return 50%
	auto openLoop = dut.getOpenLoop(300.0f);
	ASSERT_TRUE(openLoop.Valid);
	EXPECT_NEAR(50.0f, openLoop.Value, 1.0f);
}

TEST(FuelPumpPwm, PwmClosedLoopWithSensor) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	getCustomPage()->fuelPumpMode = FP_MODE_PWM;
	engineConfiguration->fuelPumpControl.pFactor = 1.0f;
	engineConfiguration->fuelPumpControl.iFactor = 0;
	engineConfiguration->fuelPumpControl.dFactor = 0;
	engineConfiguration->fuelPumpControl.offset   = 0;
	engineConfiguration->fuelPumpControl.minValue = -100;
	engineConfiguration->fuelPumpControl.maxValue = 100;

	Sensor::setMockValue(SensorType::FuelPressureLow, 280.0f);
	EXPECT_NEAR(280.0f, dut.observePlant().value_or(-1), 1.0f);

	// Error = 300 - 280 = 20, P=1.0, closed loop output = 20%
	auto cl = dut.getClosedLoop(300.0f, 280.0f);
	ASSERT_TRUE(cl.Valid);
	EXPECT_NEAR(20.0f, cl.Value, 1.0f);
}

TEST(FuelPumpPwm, PwmDutyClampedToMinMax) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	getCustomPage()->fuelPumpMode  = FP_MODE_PWM;
	getCustomPage()->fuelPumpMinDuty = 20;
	getCustomPage()->fuelPumpMaxDuty = 90;

	// Ignite at t=0, then advance past the prime window
	setTimeNowUs(0);
	dut.onIgnitionStateChanged(true);
	setTimeNowUs(10e6);  // 10s past prime duration
	dut.onSlowCallback();

	// Output above max should be clamped
	dut.setOutput(expected<percent_t>(150.0f));
	EXPECT_EQ(90, dut.fuelPumpDuty);

	// Output below min should be clamped
	dut.setOutput(expected<percent_t>(5.0f));
	EXPECT_EQ(20, dut.fuelPumpDuty);

	// Output in range passes through
	dut.setOutput(expected<percent_t>(60.0f));
	EXPECT_EQ(60, dut.fuelPumpDuty);
}

TEST(FuelPumpPwm, AggressiveReliefHoldsThroughSlowPressureBleedOnReturnlessSystem) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	getCustomPage()->fuelPumpMode    = FP_MODE_PWM;
	getCustomPage()->fuelPumpMinDuty = 20;
	getCustomPage()->fuelPumpMaxDuty = 90;

	engineConfiguration->fuelPumpControl.pFactor  = 1.0f;
	engineConfiguration->fuelPumpControl.iFactor  = 0.5f;
	engineConfiguration->fuelPumpControl.dFactor  = 0;
	engineConfiguration->fuelPumpControl.offset   = 0;
	engineConfiguration->fuelPumpControl.minValue = -100;
	engineConfiguration->fuelPumpControl.maxValue = 100;
	getCustomPage()->fuelPump_iTermMin = -100;
	getCustomPage()->fuelPump_iTermMax = 100;

	// Returnless fuel system: the pump can only add pressure to the rail, never bleed it off, so
	// once demand drops the only way pressure comes back down is via the injectors consuming it —
	// a slow multi-step decay, never an instant step to target.
	getCustomPage()->fuelPumpAggressiveRelief = true;
	getCustomPage()->fuelPumpReliefMaxInjectedMass = 5.0f;      // mg/cycle
	getCustomPage()->fuelPumpReliefEngageOverpressure = 0;      // engage as soon as above target
	getCustomPage()->fuelPumpReliefRecoverOverpressure = 0;     // resume once back at/below target
	engine->fuelComputer.running.fuel = 2.0f;                   // mg/cycle, low demand (below threshold)

	const float setpoint = 300.0f;
	float pressure = 340.0f;  // rail overpressured after demand dropped

	// Step pressure down a little at a time (never jump straight to target) to model consumption-
	// driven bleed-down. Relief must hold the pump at minDuty (PID bypassed and reset) at every
	// step while pressure remains above target.
	while (pressure > setpoint) {
		auto cl = dut.getClosedLoop(setpoint, pressure);
		EXPECT_FALSE(cl.Valid) << "relief should hold at pressure=" << pressure;
		dut.setOutput(cl);
		EXPECT_EQ(getCustomPage()->fuelPumpMinDuty, dut.fuelPumpDuty);
		EXPECT_FALSE(dut.isFpPidActive);

		pressure -= 2.0f;
	}

	// Pressure has bled back down to target: relief must release control back to the PID
	// immediately. Since the PID was held/reset throughout the bleed-down instead of winding its
	// I-term down against pressure it had no authority to correct, the output at zero error is
	// ~0 rather than a residual negative pull from a wound-down integrator.
	auto cl = dut.getClosedLoop(setpoint, pressure);
	ASSERT_TRUE(cl.Valid);
	dut.setOutput(cl);
	EXPECT_TRUE(dut.isFpPidActive);
	EXPECT_NEAR(0.0f, cl.Value, 1.0f);
}

TEST(FuelPumpPwm, AggressiveReliefDoesNotEngageWhenInjectedMassAboveThreshold) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	getCustomPage()->fuelPumpMode = FP_MODE_PWM;
	engineConfiguration->fuelPumpControl.pFactor  = 1.0f;
	engineConfiguration->fuelPumpControl.iFactor  = 0;
	engineConfiguration->fuelPumpControl.dFactor  = 0;
	engineConfiguration->fuelPumpControl.offset   = 0;
	engineConfiguration->fuelPumpControl.minValue = -100;
	engineConfiguration->fuelPumpControl.maxValue = 100;

	getCustomPage()->fuelPumpAggressiveRelief = true;
	getCustomPage()->fuelPumpReliefMaxInjectedMass = 5.0f;   // mg/cycle
	engine->fuelComputer.running.fuel = 50.0f;                // mg/cycle, well above threshold

	// Pressure is above target, but the engine is actually consuming plenty of fuel (not the
	// low-demand returnless scenario relief targets) — PID must stay in control.
	auto cl = dut.getClosedLoop(300.0f, 340.0f);
	ASSERT_TRUE(cl.Valid);
	EXPECT_TRUE(dut.isFpPidActive);
}

TEST(FuelPumpPwm, AggressiveReliefHysteresisEngageAndRecoverThresholdsDiffer) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	getCustomPage()->fuelPumpMode = FP_MODE_PWM;
	engineConfiguration->fuelPumpControl.pFactor  = 1.0f;
	engineConfiguration->fuelPumpControl.iFactor  = 0;
	engineConfiguration->fuelPumpControl.dFactor  = 0;
	engineConfiguration->fuelPumpControl.offset   = 0;
	engineConfiguration->fuelPumpControl.minValue = -100;
	engineConfiguration->fuelPumpControl.maxValue = 100;

	getCustomPage()->fuelPumpAggressiveRelief = true;
	getCustomPage()->fuelPumpReliefMaxInjectedMass = 5.0f;       // mg/cycle
	getCustomPage()->fuelPumpReliefEngageOverpressure = 10;      // must be >10kPa over target to engage
	getCustomPage()->fuelPumpReliefRecoverOverpressure = 2;      // stays latched until back within 2kPa
	engine->fuelComputer.running.fuel = 2.0f;                    // low demand throughout

	const float setpoint = 300.0f;

	// Below the engage threshold (target+10): PID stays in control, relief never latches.
	auto cl = dut.getClosedLoop(setpoint, setpoint + 5.0f);
	EXPECT_TRUE(cl.Valid);

	// Cross above the engage threshold: relief latches on.
	cl = dut.getClosedLoop(setpoint, setpoint + 15.0f);
	EXPECT_FALSE(cl.Valid);

	// Pressure falls back into the hysteresis band (between recover and engage thresholds) —
	// relief must stay latched rather than immediately releasing back to PID.
	cl = dut.getClosedLoop(setpoint, setpoint + 5.0f);
	EXPECT_FALSE(cl.Valid) << "relief should still be latched inside the hysteresis band";

	// Pressure falls to/below the recover threshold (target+2): relief releases, PID resumes.
	cl = dut.getClosedLoop(setpoint, setpoint + 1.0f);
	EXPECT_TRUE(cl.Valid);
}

TEST(FuelPumpPwm, PwmPrimeForcesMaxDuty) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	getCustomPage()->fuelPumpMode       = FP_MODE_PWM;
	getCustomPage()->fuelPumpMinDuty    = 20;
	getCustomPage()->fuelPumpMaxDuty    = 90;
	engineConfiguration->startUpFuelPumpDuration = 4;

	// Within prime window
	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();
	EXPECT_TRUE(dut.isPrime);

	// Even if PID says low duty, prime forces max
	dut.setOutput(expected<percent_t>(30.0f));
	EXPECT_EQ(90, dut.fuelPumpDuty);
}
