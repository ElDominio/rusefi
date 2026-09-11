#include "pch.h"

#include "fan_control.h"

static void updateFans() {
	engine->module<FanControl1>()->onSlowCallback();
}

struct MockAcOff : public AcController {
	bool isAcEnabled() const override { return false; }
};

struct MockAcOn : public AcController {
	bool isAcEnabled() const override { return true; }
};

TEST(Actuators, Fan) {
	struct MockAc : public AcController {
		bool acState = false;

		bool isAcEnabled() const override {
			return acState;
		}
	};

	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAc mockAc;
	engine->module<AcController>().set(&mockAc);

	engineConfiguration->fanOnTemperature = 90;
	engineConfiguration->fanOffTemperature = 80;
	getCustomPage()->fan1AcMode = fan_ac_mode_e::Disabled;

	// Cold, fan should be off
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());

	// Between thresholds, should still be off
	Sensor::setMockValue(SensorType::Clt, 85);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());

	// Hot, fan should turn on
	Sensor::setMockValue(SensorType::Clt, 95);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());

	// Between thresholds, should stay on
	Sensor::setMockValue(SensorType::Clt, 85);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());

	// Below threshold, should turn off
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());

	// Break the CLT sensor - fan turns on
	Sensor::setInvalidMockValue(SensorType::Clt);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());

	// CLT sensor back to normal, fan turns off
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());

	getCustomPage()->fan1AcMode = fan_ac_mode_e::Relay;
	// Now AC is on, fan should turn on!
	mockAc.acState = true;
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());

	// Turn off AC, fan should turn off too.
	mockAc.acState = false;
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());

	// Back to hot, fan should turn on
	Sensor::setMockValue(SensorType::Clt, 95);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());

	// Engine starts cranking, fan should turn off
	engine->rpmCalculator.setRpmValue(100);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());

	// Engine running, fan should turn back on
	engine->rpmCalculator.setRpmValue(1000);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());

	// Stop the engine, fan should stay on
	engine->rpmCalculator.setRpmValue(0);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());

	// Set configuration to inhibit fan while engine is stopped, fan should stop
	engineConfiguration->disableFan1WhenStopped = true;
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());
}

// Helper: configure fan 1 for PWM mode with a linear 80-110°C → 0-100% curve.
// min/max 0-100%, no soft-start unless overridden by caller.
static void setupFan1Pwm(EngineTestHelper& /*eth*/) {
	engineConfiguration->fan1PwmEnabled = true;
	setLinearCurve(engineConfiguration->fan1TempBins, 80, 110);
	setLinearCurve(engineConfiguration->fan1PwmValues, 0, 100);
	engineConfiguration->fan1MinPwm = 0;
	engineConfiguration->fan1MaxPwm = 100;
	engineConfiguration->fan1SoftStartSec = 0.0f;
}

TEST(Actuators, FanPwm_ActiveFlag) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);

	Sensor::setMockValue(SensorType::Clt, 95);

	// Relay mode: pwmActive must be false
	engineConfiguration->fan1PwmEnabled = false;
	updateFans();
	EXPECT_EQ(false, engine->module<FanControl1>()->pwmActive);

	// PWM mode: pwmActive must be true
	setupFan1Pwm(eth);
	updateFans();
	EXPECT_EQ(true, engine->module<FanControl1>()->pwmActive);
}

TEST(Actuators, FanPwm_CurveInterpolation) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	setupFan1Pwm(eth);

	// Below curve start → 0%
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_NEAR(0.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Midpoint of curve ~95°C → ~50%
	Sensor::setMockValue(SensorType::Clt, 95);
	updateFans();
	EXPECT_NEAR(50.0f, engine->module<FanControl1>()->pwmAppliedPwm, 2.0f);

	// Above curve end → 100%
	Sensor::setMockValue(SensorType::Clt, 115);
	updateFans();
	EXPECT_NEAR(100.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Relay pin must NOT be driven in PWM mode
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());
}

TEST(Actuators, FanPwm_MinMaxAsOffAndFullDuty) {
	// fan1MinPwm/fan1MaxPwm are the duty cycles meaning "off" (0% demand) and "full speed"
	// (100% demand); the demand curve is interpolated between them, not clamped against them.
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	setupFan1Pwm(eth);

	// Cold -> fan disabled -> output at the "off" duty, 30%
	engineConfiguration->fan1MinPwm = 30;
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_NEAR(30.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Hot -> fan enabled, curve saturated at 100% demand -> output at the "full speed" duty, 60%
	engineConfiguration->fan1MaxPwm = 60;
	Sensor::setMockValue(SensorType::Clt, 115);
	updateFans();
	EXPECT_NEAR(60.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);
}

TEST(Actuators, FanPwm_InvertedRange) {
	// Hardware that drives the fan through an inverted PWM path (e.g. NPN transistor + pull-up):
	// a high duty cycle keeps the fan off, a low duty cycle drives it at full speed. fan1MinPwm
	// (the "off" duty) can be set higher than fan1MaxPwm (the "full speed" duty) to describe this.
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	setupFan1Pwm(eth);
	engineConfiguration->fan1MinPwm = 90;
	engineConfiguration->fan1MaxPwm = 10;

	// Fan should be off (cold) -> duty at the "off" level, 90%
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_NEAR(90.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Fan should be full speed (hot) -> duty at the "full" level, 10%
	Sensor::setMockValue(SensorType::Clt, 115);
	updateFans();
	EXPECT_NEAR(10.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Midpoint of the curve (~95C, ~50% demand) -> duty halfway between 90 and 10, i.e. 50%
	Sensor::setMockValue(SensorType::Clt, 95);
	updateFans();
	EXPECT_NEAR(50.0f, engine->module<FanControl1>()->pwmAppliedPwm, 2.0f);
}

TEST(Actuators, FanPwm_DisableGatesUseOffDuty) {
	// Cranking/disable-when-stopped/disable-at-speed (isHardInhibited(), shared with the relay
	// path) still force PWM mode to the curve's lowest-bin ("off") demand, regardless of what the
	// curve says for the current temperature.
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	setupFan1Pwm(eth);
	engineConfiguration->fan1MinPwm = 15;
	engineConfiguration->fan1MaxPwm = 100;

	// Hot enough that the curve alone would call for full speed
	Sensor::setMockValue(SensorType::Clt, 115);
	updateFans();
	EXPECT_NEAR(100.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Cranking inhibits the fan even in PWM mode -> output drops to the "off" duty (15%), not 0%
	engine->rpmCalculator.setRpmValue(100);
	updateFans();
	EXPECT_NEAR(15.0f, engine->module<FanControl1>()->pwmAppliedPwm, 0.01f);

	// Running again, still hot -> back to full speed
	engine->rpmCalculator.setRpmValue(1000);
	updateFans();
	EXPECT_NEAR(100.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// disableFan1WhenStopped also inhibits PWM mode now
	engineConfiguration->disableFan1WhenStopped = true;
	engine->rpmCalculator.setRpmValue(0);
	updateFans();
	EXPECT_NEAR(15.0f, engine->module<FanControl1>()->pwmAppliedPwm, 0.01f);
}

TEST(Actuators, FanPwm_IgnoresOnOffTemperature) {
	// PWM mode has no separate on/off temperature threshold - the curve is the sole source of
	// truth. Deliberately set fanOnTemperature/fanOffTemperature to values that would force the
	// fan off under the old (relay-style) logic, and confirm PWM output tracks the curve anyway.
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	setupFan1Pwm(eth);
	engineConfiguration->fanOnTemperature = 200;
	engineConfiguration->fanOffTemperature = 190;

	// Well below fanOffTemperature (190) - old logic would call this "cold" -> off. New logic:
	// just reads the curve, which says ~50% at 95C.
	Sensor::setMockValue(SensorType::Clt, 95);
	updateFans();
	EXPECT_NEAR(50.0f, engine->module<FanControl1>()->pwmAppliedPwm, 2.0f);
}

TEST(Actuators, FanPwm_CurveEndpointsAreOffAndFull) {
	// The curve's own lowest/highest bin values - not a hardcoded 0/100 - are used as the "off"
	// and "full speed" demand for CLT-broken and hard-inhibited (e.g. cranking) cases. Use a curve
	// whose endpoints are NOT 0/100 to prove this isn't coincidental.
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	setupFan1Pwm(eth);
	setLinearCurve(engineConfiguration->fan1PwmValues, 20, 80); // lowest bin=20%, highest bin=80%
	engineConfiguration->fan1MinPwm = 0;
	engineConfiguration->fan1MaxPwm = 100;

	// Below the curve's range -> clamped to the lowest bin's value, 20%, not 0%.
	Sensor::setMockValue(SensorType::Clt, 50);
	updateFans();
	EXPECT_NEAR(20.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Above the curve's range -> clamped to the highest bin's value, 80%, not 100%.
	Sensor::setMockValue(SensorType::Clt, 200);
	updateFans();
	EXPECT_NEAR(80.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Broken CLT -> fail safe at the highest bin's value, 80%, not 100%.
	Sensor::setInvalidMockValue(SensorType::Clt);
	updateFans();
	EXPECT_NEAR(80.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Cranking (hard-inhibited) -> lowest bin's value, 20%, not 0%.
	Sensor::setMockValue(SensorType::Clt, 200);
	engine->rpmCalculator.setRpmValue(100);
	updateFans();
	EXPECT_NEAR(20.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);
}

TEST(Actuators, FanPwm_AcRelayForcesMaxSpeed) {
	// Relay mode: the condenser needs full airflow whenever the compressor relay is engaged, so
	// PWM mode should just run flat out - no adder, no partial ramp.
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAcOff;
	MockAcOn mockAcOn;
	engine->module<AcController>().set(&mockAcOff);
	setupFan1Pwm(eth);
	engineConfiguration->fan1MinPwm = 10; // "off" duty, to prove AC-on isn't just landing on 0
	getCustomPage()->fan1AcMode = fan_ac_mode_e::Relay;

	// Cool engine, AC off -> curve says 0% -> disabled (cold) -> "off" duty
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_NEAR(10.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Still cool, but AC compressor engages -> fan must jump straight to 100%, not a partial adder
	engine->module<AcController>().set(&mockAcOn);
	updateFans();
	EXPECT_NEAR(100.0f, engine->module<FanControl1>()->pwmAppliedPwm, 0.01f);

	// AC off again -> back to curve-driven (still cold -> "off" duty)
	engine->module<AcController>().set(&mockAcOff);
	updateFans();
	EXPECT_NEAR(10.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);
}

TEST(Actuators, FanPwm_AcPressureProportional) {
	// Pressure mode: demand ramps from 0% at the Off threshold to 100% at the On threshold, and
	// is never lower than what the temperature curve alone would call for.
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	setupFan1Pwm(eth);

	// Keep the temperature curve out of it: cold engine, unreachable on/off thresholds
	engineConfiguration->fanOnTemperature = 200;
	engineConfiguration->fanOffTemperature = 190;
	Sensor::setMockValue(SensorType::Clt, 75);

	getCustomPage()->fan1AcMode = fan_ac_mode_e::Pressure;
	getCustomPage()->fan1AcPressureOn = 1400;
	getCustomPage()->fan1AcPressureOff = 1100;

	// At/above the On threshold -> full speed
	Sensor::setMockValue(SensorType::AcPressure, 1500);
	updateFans();
	EXPECT_NEAR(100.0f, engine->module<FanControl1>()->pwmAppliedPwm, 0.01f);

	// Halfway between Off (1100) and On (1400) -> ~50%
	Sensor::setMockValue(SensorType::AcPressure, 1250);
	updateFans();
	EXPECT_NEAR(50.0f, engine->module<FanControl1>()->pwmAppliedPwm, 2.0f);

	// Below the Off threshold -> pressure demand is 0, curve alone (cold) also says 0 -> "off" duty
	Sensor::setMockValue(SensorType::AcPressure, 1000);
	updateFans();
	EXPECT_NEAR(0.0f, engine->module<FanControl1>()->pwmAppliedPwm, 0.01f);
}

TEST(Actuators, FanPwm_InvalidClt) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	setupFan1Pwm(eth);
	engineConfiguration->fan1MaxPwm = 100;

	// Start cold so we know current PWM is low
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_NEAR(0.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Break the sensor → output must jump to maxPwm (fail-safe, not 0)
	Sensor::setInvalidMockValue(SensorType::Clt);
	updateFans();
	EXPECT_NEAR(100.0f, engine->module<FanControl1>()->pwmAppliedPwm, 0.01f);
	EXPECT_EQ(true, engine->module<FanControl1>()->pwmActive);

	// Sensor restored → normal output resumes
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_NEAR(0.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);
}

TEST(Actuators, FanPwm_SoftStart) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	setupFan1Pwm(eth);

	// 2s soft-start at 20 Hz = 40 steps to ramp 0→100%, so 2.5% per step
	engineConfiguration->fan1SoftStartSec = 2.0f;

	// Hot temperature so curve target is 100%
	Sensor::setMockValue(SensorType::Clt, 115);

	// First step: applied must be ~2.5%, not 100%
	updateFans();
	float after1 = engine->module<FanControl1>()->pwmAppliedPwm;
	EXPECT_NEAR(2.5f, after1, 0.5f);

	// After enough steps the ramp should reach the target
	for (int i = 0; i < 50; i++) {
		updateFans();
	}
	EXPECT_NEAR(100.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);

	// Downward change (cooler) is instant – no ramp on the way down
	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_NEAR(0.0f, engine->module<FanControl1>()->pwmAppliedPwm, 1.0f);
}

TEST(Actuators, FanVssHysteresis) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);
	getCustomPage()->fan1AcMode = fan_ac_mode_e::Disabled;

	engineConfiguration->fanOnTemperature = 90;
	engineConfiguration->fanOffTemperature = 80;
	engineConfiguration->disableFan1AtSpeed = 50;
	engineConfiguration->disableFan1AtSpeedHysteresis = 10;

	// Hot CLT, VSS below threshold → fan on
	Sensor::setMockValue(SensorType::Clt, 95);
	Sensor::setMockValue(SensorType::VehicleSpeed, 30);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());

	// VSS exceeds threshold → fan disabled by speed
	Sensor::setMockValue(SensorType::VehicleSpeed, 55);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());
	EXPECT_EQ(true, engine->module<FanControl1>()->disabledBySpeed);

	// VSS drops below threshold but still inside hysteresis band (45 < 50 but > 50-10=40) → stays off
	Sensor::setMockValue(SensorType::VehicleSpeed, 45);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());
	EXPECT_EQ(true, engine->module<FanControl1>()->disabledBySpeed);

	// VSS drops below (threshold - hysteresis) → fan re-enabled
	Sensor::setMockValue(SensorType::VehicleSpeed, 38);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());
	EXPECT_EQ(false, engine->module<FanControl1>()->disabledBySpeed);
}

TEST(Actuators, FanAcPressureModeIgnoresCompressorState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);

	// Cold CLT and speed/etc. all neutral so only the pressure logic can turn the fan on.
	engineConfiguration->fanOnTemperature = 200;
	engineConfiguration->fanOffTemperature = 190;
	Sensor::setMockValue(SensorType::Clt, 75);

	getCustomPage()->fan1AcMode = fan_ac_mode_e::Pressure;
	getCustomPage()->fan1AcPressureOn = 1400;
	getCustomPage()->fan1AcPressureOff = 1100;

	// Compressor is off, but pressure is above the On threshold -> fan must turn on anyway.
	Sensor::setMockValue(SensorType::AcPressure, 1500);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());

	// Pressure drops below the Off threshold -> fan turns back off, still with compressor off.
	Sensor::setMockValue(SensorType::AcPressure, 1000);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());

	// Invalid pressure reading -> fan must not be commanded on by pressure logic.
	Sensor::setMockValue(SensorType::AcPressure, 1500);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());
	Sensor::setInvalidMockValue(SensorType::AcPressure);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());
}

TEST(Actuators, FanPwm_RelayModeUnchanged) {
	// Ensure PWM disabled still uses the existing on/off relay path
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	MockAcOff mockAc;
	engine->module<AcController>().set(&mockAc);

	engineConfiguration->fan1PwmEnabled = false;
	engineConfiguration->fanOnTemperature = 90;
	engineConfiguration->fanOffTemperature = 80;

	Sensor::setMockValue(SensorType::Clt, 95);
	updateFans();
	EXPECT_EQ(true, enginePins.fanRelay.getLogicValue());
	EXPECT_EQ(false, engine->module<FanControl1>()->pwmActive);

	Sensor::setMockValue(SensorType::Clt, 75);
	updateFans();
	EXPECT_EQ(false, enginePins.fanRelay.getLogicValue());
}
