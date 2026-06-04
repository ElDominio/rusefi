#include "pch.h"

// Helper: set all three dual-mode activation thresholds
static void setDualActivation(uint16_t rpm, uint8_t load, uint8_t tps) {
	engineConfiguration->secondaryFpActivationRpm  = rpm;
	engineConfiguration->secondaryFpActivationLoad = load;
	engineConfiguration->secondaryFpActivationTps  = tps;
}

// Helper: set all three dual-mode deactivation thresholds
static void setDualDeactivation(uint16_t rpm, uint8_t load, uint8_t tps) {
	engineConfiguration->secondaryFpDeactivationRpm  = rpm;
	engineConfiguration->secondaryFpDeactivationLoad = load;
	engineConfiguration->secondaryFpDeactivationTps  = tps;
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
	engineConfiguration->fuelPumpMode = FP_MODE_SINGLE;
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
	engineConfiguration->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualDeactivation(2500, 50, 40);

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
	engineConfiguration->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualDeactivation(2500, 50, 40);

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
	engineConfiguration->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualDeactivation(2500, 50, 40);

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
	engineConfiguration->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualDeactivation(2500, 50, 40);

	// All conditions above activation thresholds — secondary turns on
	Sensor::setMockValue(SensorType::Rpm, 4000);
	getEngineState()->fuelingLoad = 80.0f;
	Sensor::setMockValue(SensorType::Tps1, 70);

	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();
	EXPECT_TRUE(efiReadPin(Gpio::B0));  // secondary on

	// Drop RPM below deactivation threshold — secondary must turn off
	Sensor::setMockValue(SensorType::Rpm, 2000);  // below deactivation 2500
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
	engineConfiguration->fuelPumpMode = FP_MODE_DUAL;
	enginePins.fuelPumpRelay.init();
	setupDualPin();

	setDualActivation(3000, 60, 50);
	setDualDeactivation(2500, 50, 40);

	// Activate secondary
	Sensor::setMockValue(SensorType::Rpm, 4000);
	getEngineState()->fuelingLoad = 80.0f;
	Sensor::setMockValue(SensorType::Tps1, 70);
	setTimeNowUs(0.5e6);
	dut.onIgnitionStateChanged(true);
	dut.onSlowCallback();
	EXPECT_TRUE(efiReadPin(Gpio::B0));

	// Drop only load below its deactivation threshold; RPM and TPS remain above activation
	getEngineState()->fuelingLoad = 30.0f;  // below deactivation 50%
	dut.onSlowCallback();
	// OR logic: secondary must turn off even though RPM and TPS are fine
	EXPECT_FALSE(efiReadPin(Gpio::B0));
}

TEST(FuelPumpPwm, PwmGetSetpointFromTable) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	engineConfiguration->fuelPumpMode = FP_MODE_PWM;

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

	engineConfiguration->fuelPumpMode = FP_MODE_PWM;

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

	engineConfiguration->fuelPumpMode = FP_MODE_PWM;
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

	engineConfiguration->fuelPumpMode  = FP_MODE_PWM;
	engineConfiguration->fuelPumpMinDuty = 20;
	engineConfiguration->fuelPumpMaxDuty = 90;

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

TEST(FuelPumpPwm, PwmPrimeForcesMaxDuty) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	FuelPumpController dut;

	engineConfiguration->fuelPumpMode       = FP_MODE_PWM;
	engineConfiguration->fuelPumpMinDuty    = 20;
	engineConfiguration->fuelPumpMaxDuty    = 90;
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
