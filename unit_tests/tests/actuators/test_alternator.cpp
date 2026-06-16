#include "pch.h"

#include "alternator_controller.h"

using ::testing::StrictMock;
using ::testing::Return;

TEST(Alternator, observePlant) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	AlternatorController dut;

	Sensor::setMockValue(SensorType::BatteryVoltage, 13);
	EXPECT_EQ(13, dut.observePlant().value_or(0));
}

TEST(Alternator, openLoop) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	AlternatorController dut;
	engineConfiguration->acRelayAlternatorDutyAdder = 15;

	EXPECT_EQ(dut.getOpenLoop(10).value_or(-1), 0);

	// turn on AC!
	engine->module<AcController>()->acButtonState = true;
	EXPECT_EQ(dut.getOpenLoop(10).value_or(-1), 15);
}

TEST(Alternator, ClosedLoop) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	AlternatorController dut;

	engineConfiguration->alternatorControl.pFactor = 1.5f;
	engineConfiguration->alternatorControl.iFactor = 0;
	engineConfiguration->alternatorControl.dFactor = 0;
	engineConfiguration->alternatorControl.offset = 0;
	// apply PID settings
	dut.update();

	// Target of 30 with position 20 should yield positive duty, P=1.5 means 15% duty for 10% error
	EXPECT_EQ(dut.getClosedLoop(30, 20).value_or(0), 15);
}

TEST(Alternator, openLoopBaseDutyTableMode) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	AlternatorController dut;

	setTable(config->alternatorBaseDutyTable, 30);
	Sensor::setMockValue(SensorType::Rpm, 2000);

	// Table mode OFF (default): base duty is 0, AC off
	engineConfiguration->alternatorBaseDutyUseTable = false;
	EXPECT_EQ(dut.getOpenLoop(14.0f).value_or(-1), 0);

	// Table mode ON: base duty from table
	engineConfiguration->alternatorBaseDutyUseTable = true;
	EXPECT_NEAR(dut.getOpenLoop(14.0f).value_or(-1), 30.0f, EPS5D);

	// Table mode ON, AC on: base duty + acAdder
	engineConfiguration->acRelayAlternatorDutyAdder = 10;
	engine->module<AcController>()->acButtonState = true;
	EXPECT_NEAR(dut.getOpenLoop(14.0f).value_or(-1), 40.0f, EPS5D);
}
