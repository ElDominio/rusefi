#include "pch.h"
#include "vehicle_speed_converter.h"


float GetVssFor(float revPerKm, float axle, float teeth, float hz) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	VehicleSpeedConverter dut;

	engineConfiguration->driveWheelRevPerKm = revPerKm;
	engineConfiguration->vssGearRatio = axle;
	engineConfiguration->vssToothCount = teeth;

	return dut.convert(hz).value_or(-1);
}

TEST(VehicleSpeed, FakeCases) {
	// 0hz -> 0kph
	EXPECT_NEAR_M3(0, GetVssFor(500, 5, 10, 0));

	// 1000hz -> 144 kph
	EXPECT_NEAR_M3(144, GetVssFor(500, 5, 10, 1000));

	// Half size tires -> half speed
	EXPECT_NEAR_M3(72, GetVssFor(1000, 5, 10, 1000));

	// Double the axle ratio -> half the speed
	EXPECT_NEAR_M3(72, GetVssFor(500, 10, 10, 1000));

	// Twice as many teeth -> half speed
	EXPECT_NEAR_M3(72, GetVssFor(500, 5, 20, 1000));
}

TEST(VehicleSpeed, RealCases) {
	// V8 Volvo
	// 205/50R16 tire -> 521 rev/km
	// 3.73 axle ratio
	// 17 tooth speedo gear
	EXPECT_NEAR_M3(108.970f, GetVssFor(521, 3.73, 17, 1000));

	// NB miata
	// 205/50R15 tire -> 544 rev/km
	// 4.3 axle ratio
	// 21 tooth speedo gear
	EXPECT_NEAR_M3(73.285f, GetVssFor(544, 4.3, 21, 1000));

	// Some truck with ABS sensors
	// 265/65R18 tire -> 391 rev/km
	// 1.0 ratio because ABS sensors are hub mounted
	// 48 tooth abs sensor
	EXPECT_NEAR_M3(191.816f, GetVssFor(391, 1, 48, 1000));
}

TEST(VehicleSpeed, PlausibilityFilterDisabledByDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	VehicleSpeedConverter dut;

	engineConfiguration->driveWheelRevPerKm = 500;
	engineConfiguration->vssGearRatio = 5;
	engineConfiguration->vssToothCount = 10;
	// vssMaxAcceleration left at 0 -> filter disabled

	EXPECT_NEAR_M3(144, dut.convert(1000).value_or(-1));
	// Even an impossible jump passes straight through when disabled
	EXPECT_NEAR_M3(720, dut.convert(5000).value_or(-1));
}

TEST(VehicleSpeed, PlausibilityFilterRejectsSpike) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	VehicleSpeedConverter dut;

	engineConfiguration->driveWheelRevPerKm = 500;
	engineConfiguration->vssGearRatio = 5;
	engineConfiguration->vssToothCount = 10;
	engineConfiguration->vssMaxAcceleration = 50; // km/h/sec

	// Bootstrap: the very first reading is always accepted, however implausible
	EXPECT_NEAR_M3(144, dut.convert(1000).value_or(-1));

	// A single-pulse jump to 720 km/h implies ~576 km/h/s of acceleration -
	// far past the 50 km/h/s threshold, so it's rejected and held at the
	// last accepted value. The reject budget is 30 readings.
	for (int i = 0; i < 30; i++) {
		EXPECT_NEAR_M3(144, dut.convert(5000).value_or(-1));
	}

	// Budget exhausted - trust the sensor again and accept the new reading.
	EXPECT_NEAR_M3(720, dut.convert(5000).value_or(-1));
}
