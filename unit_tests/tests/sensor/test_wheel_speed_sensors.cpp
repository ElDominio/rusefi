#include "pch.h"
#include "custom_page.h"
#include "init.h"
#include "axle_speed_converter.h"

// Main Speed Sensor: SensorType::VehicleSpeed is a passthrough of whichever of
// None / Output Shaft Speed / Front Axle / Rear Axle the Source dropdown selects.
TEST(WheelSpeedSensors, mainSpeedSensorFromOutputShaftSpeed) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	getCustomPage()->ossRevPerKm = 507.0f;
	getCustomPage()->mainSpeedSensorSource = main_speed_sensor_source_e::OutputShaftSpeed;
	// Sensors are NOT auto-registered by EngineTestHelper (initNewSensors() is compiled out under
	// EFI_UNIT_TEST so each test can selectively mock) -- register explicitly.
	initVehicleSpeedSensor();

	Sensor::setMockValue(SensorType::OutputShaftSpeed, 1000.0f);

	auto speed = Sensor::get(SensorType::VehicleSpeed);
	ASSERT_TRUE(speed.Valid);
	// speedKmh = ossRpm * 60 / ossRevPerKm = 1000 * 60 / 507
	EXPECT_NEAR(1000.0f * 60.0f / 507.0f, speed.Value, 1e-2);
}

TEST(WheelSpeedSensors, mainSpeedSensorFromOutputShaftSpeedInvalidWithoutOssReading) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	getCustomPage()->ossRevPerKm = 507.0f;
	getCustomPage()->mainSpeedSensorSource = main_speed_sensor_source_e::OutputShaftSpeed;
	initVehicleSpeedSensor();

	// OutputShaftSpeed never set -> invalid, not some bogus computed value
	auto speed = Sensor::get(SensorType::VehicleSpeed);
	EXPECT_FALSE(speed.Valid);
}

// None is the safe default for cars with no speed sensor at all -- VehicleSpeed must stay
// invalid even if OSS/axle sensors happen to have real readings available.
TEST(WheelSpeedSensors, mainSpeedSensorNoneStaysInvalid) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	getCustomPage()->mainSpeedSensorSource = main_speed_sensor_source_e::None;
	initVehicleSpeedSensor();

	Sensor::setMockValue(SensorType::OutputShaftSpeed, 3000.0f);
	Sensor::setMockValue(SensorType::WheelSpeedFront, 100.0f);

	auto speed = Sensor::get(SensorType::VehicleSpeed);
	EXPECT_FALSE(speed.Valid);
}

TEST(WheelSpeedSensors, mainSpeedSensorFromFrontAxlePassthrough) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	getCustomPage()->mainSpeedSensorSource = main_speed_sensor_source_e::FrontAxle;
	initVehicleSpeedSensor();

	Sensor::setMockValue(SensorType::WheelSpeedFront, 87.5f);

	auto speed = Sensor::get(SensorType::VehicleSpeed);
	ASSERT_TRUE(speed.Valid);
	EXPECT_NEAR(87.5f, speed.Value, 1e-3);
}

TEST(WheelSpeedSensors, mainSpeedSensorFromRearAxlePassthrough) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	getCustomPage()->mainSpeedSensorSource = main_speed_sensor_source_e::RearAxle;
	initVehicleSpeedSensor();

	Sensor::setMockValue(SensorType::WheelSpeedRear, 42.0f);

	auto speed = Sensor::get(SensorType::VehicleSpeed);
	ASSERT_TRUE(speed.Valid);
	EXPECT_NEAR(42.0f, speed.Value, 1e-3);
}

// AxleSpeedConverter: Hz -> km/h via each axle's own tooth count + wheel revs/km, independently
// for Front and Rear (constructor-parameterized, one class serves both).
TEST(WheelSpeedSensors, axleConverterFrontMath) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->wheelSpeedFrontTeeth = 4;
	getCustomPage()->wheelSpeedFrontRevPerKm = 600.0f;

	AxleSpeedConverter dut(/*isFront*/true);
	// pulsePerKm = 600 * 4 = 2400; speedKmh = freq * 3600 / 2400
	auto result = dut.convert(100.0f);
	ASSERT_TRUE(result.Valid);
	EXPECT_NEAR(150.0f, result.Value, 1e-2);
}

TEST(WheelSpeedSensors, axleConverterRearMathIndependentOfFront) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	getCustomPage()->wheelSpeedFrontTeeth = 4;
	getCustomPage()->wheelSpeedFrontRevPerKm = 600.0f;
	getCustomPage()->wheelSpeedRearTeeth = 8;
	getCustomPage()->wheelSpeedRearRevPerKm = 500.0f;

	AxleSpeedConverter dut(/*isFront*/false);
	// pulsePerKm = 500 * 8 = 4000; speedKmh = freq * 3600 / 4000
	auto result = dut.convert(100.0f);
	ASSERT_TRUE(result.Valid);
	EXPECT_NEAR(90.0f, result.Value, 1e-2);
}

// Configurable Wheel Slip Ratio Source1/Source2 selector -- unchanged mechanism from before this
// redesign, just without the retired wheelSpeedSensorsEnabled master-enable bit.
TEST(WheelSpeedSensors, configurableSlipRatioComputesFromConfiguredSources) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	getCustomPage()->wheelSlipRatioSource1 = wheel_speed_source_e::FrontAxle;
	getCustomPage()->wheelSlipRatioSource2 = wheel_speed_source_e::RearAxle;
	initConfigurableWheelSlipRatio();

	Sensor::setMockValue(SensorType::WheelSpeedFront, 105.0f);
	Sensor::setMockValue(SensorType::WheelSpeedRear, 100.0f);

	auto ratio = Sensor::get(SensorType::WheelSlipRatio);
	ASSERT_TRUE(ratio.Valid);
	EXPECT_NEAR(1.05f, ratio.Value, 1e-3);
}

TEST(WheelSpeedSensors, configurableSlipRatioNotRegisteredWhenSourcesAreNone) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Defaults leave wheelSlipRatioSource1/2 at None, so the selector must not register anything.
	initConfigurableWheelSlipRatio();
	auto ratio = Sensor::get(SensorType::WheelSlipRatio);
	EXPECT_FALSE(ratio.Valid);
}

// The configurable selector must win over the Aux-Speed-based slip ratio (init_aux_speed_sensor.cpp)
// instead of both trying to register SensorType::WheelSlipRatio (which would throw under EFI_UNIT_TEST).
TEST(WheelSpeedSensors, configurableSourceTakesPriorityOverAuxSpeedSlipRatio) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	getCustomPage()->wheelSlipRatioSource1 = wheel_speed_source_e::FrontAxle;
	getCustomPage()->wheelSlipRatioSource2 = wheel_speed_source_e::RearAxle;
	engineConfiguration->useAuxSpeedForSlipRatio = true;

	EXPECT_NO_THROW({
		initConfigurableWheelSlipRatio();
		initAuxSpeedSensors();
	});

	Sensor::setMockValue(SensorType::WheelSpeedFront, 110.0f);
	Sensor::setMockValue(SensorType::WheelSpeedRear, 100.0f);

	auto ratio = Sensor::get(SensorType::WheelSlipRatio);
	ASSERT_TRUE(ratio.Valid);
	EXPECT_NEAR(1.10f, ratio.Value, 1e-3);
}
