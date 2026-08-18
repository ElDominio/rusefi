#include "pch.h"
#include "custom_page.h"
#include "output_shaft_speed_converter.h"

class OutputShaftSpeedConverterTest : public ::testing::Test {
public:
	EngineTestHelper eth;
	OutputShaftSpeedConverter dut;

	OutputShaftSpeedConverterTest() : eth(engine_type_e::TEST_ENGINE) {
	}
};

TEST_F(OutputShaftSpeedConverterTest, convertsFrequencyToRpmByToothCount) {
	getCustomPage()->outputShaftSpeedSensorTeeth = 4;

	// 100 Hz * 60 / 4 teeth = 1500 RPM
	auto result = dut.convert(100.0f);
	ASSERT_TRUE(result.Valid);
	EXPECT_NEAR(1500.0f, result.Value, 1e-3);
}

TEST_F(OutputShaftSpeedConverterTest, higherToothCountLowersRpmForSameFrequency) {
	getCustomPage()->outputShaftSpeedSensorTeeth = 60;

	// 100 Hz * 60 / 60 teeth = 100 RPM
	auto result = dut.convert(100.0f);
	ASSERT_TRUE(result.Valid);
	EXPECT_NEAR(100.0f, result.Value, 1e-3);
}

// Shared plausibility filter: disabled by default (wheelSpeedMaxAccelPercent == 0), a glitched
// reading, recovery after enough consecutive rejects, and the near-zero starting-value floor.
TEST_F(OutputShaftSpeedConverterTest, filterDisabledByDefaultPassesGlitchesThrough) {
	getCustomPage()->outputShaftSpeedSensorTeeth = 60;
	// wheelSpeedMaxAccelPercent left at its zero default -> filter is a no-op.

	dut.convert(100.0f); // 100 RPM
	auto result = dut.convert(10000.0f); // huge, implausible jump - should NOT be rejected
	ASSERT_TRUE(result.Valid);
	EXPECT_NEAR(10000.0f, result.Value, 1e-3);
}

TEST_F(OutputShaftSpeedConverterTest, filterRejectsAndDeadReckonsAGlitchedReading) {
	getCustomPage()->outputShaftSpeedSensorTeeth = 60;
	getCustomPage()->wheelSpeedMaxAccelPercent = 10; // 10%/sec

	auto first = dut.convert(60.0f); // 60 RPM, dt = 1/60s -> establishes baseline
	ASSERT_TRUE(first.Valid);
	EXPECT_NEAR(60.0f, first.Value, 1e-3);

	// A single glitched tooth implying a huge jump should be rejected (dead-reckoned near the
	// previous value), not passed through raw.
	auto glitched = dut.convert(6000.0f); // would be 600 RPM if trusted
	ASSERT_TRUE(glitched.Valid);
	EXPECT_LT(glitched.Value, 100.0f);
}

TEST_F(OutputShaftSpeedConverterTest, filterRecoversAfterEnoughConsecutiveRejects) {
	getCustomPage()->outputShaftSpeedSensorTeeth = 60;
	getCustomPage()->wheelSpeedMaxAccelPercent = 10;

	dut.convert(60.0f); // establish baseline near 60 RPM

	// Sustained real acceleration to 600 RPM: the filter should give up rejecting after its
	// consecutive-reject limit and start tracking the real reading again.
	SensorResult last = UnexpectedCode::Unknown;
	for (int i = 0; i < 40; i++) {
		last = dut.convert(600.0f);
	}
	ASSERT_TRUE(last.Valid);
	EXPECT_NEAR(600.0f, last.Value, 1.0f);
}

TEST_F(OutputShaftSpeedConverterTest, filterRecoversFromNearZeroStart) {
	getCustomPage()->outputShaftSpeedSensorTeeth = 60;
	getCustomPage()->wheelSpeedMaxAccelPercent = 10;

	// Starting from a stop (first reading establishes baseline unconditionally), a real
	// acceleration to a modest RPM must not stay stuck against a near-zero base forever --
	// bounded by the same consecutive-reject give-up as any other value.
	dut.convert(0.0f);
	SensorResult last = UnexpectedCode::Unknown;
	for (int i = 0; i < 40; i++) {
		last = dut.convert(50.0f);
	}
	ASSERT_TRUE(last.Valid);
	EXPECT_NEAR(50.0f, last.Value, 1.0f);
}
