/**
 * @file test_cranking_no_spark.cpp
 *
 * Cranking No-Spark (EFI_CRANKING_NO_SPARK): for engines with a distributor/module-based
 * ignition that fires spark on its own during cranking, suppress rusEFI's own spark
 * scheduling entirely while cranking, and resume normal ECU spark control as soon as
 * cranking ends (crosses cranking.rpm). See LimpManager::updateState().
 */

#include "pch.h"

#include "limp_manager.h"

TEST(crankingNoSpark, disabledByDefaultDoesNotCutCranking) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	LimpManager dut;

	engineConfiguration->cranking.rpm = 500;

	// well below cranking.rpm, but the feature defaults to disabled
	engine->rpmCalculator.setRpmValue(300);
	Sensor::setMockValue(SensorType::Rpm, 300);

	dut.updateState(300, 0);
	EXPECT_TRUE(dut.allowIgnition());
}

TEST(crankingNoSpark, enabledCutsSparkWhileCranking) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	LimpManager dut;

	engineConfiguration->cranking.rpm = 500;
	getCustomPage()->crankingNoSparkEnabled = true;

	// cranking: RPM below cranking.rpm
	engine->rpmCalculator.setRpmValue(300);
	Sensor::setMockValue(SensorType::Rpm, 300);

	dut.updateState(300, 0);
	EXPECT_FALSE(dut.allowIgnition());
	EXPECT_EQ(ClearReason::CrankingNoSpark, dut.allowIgnition().reason);

	// fuel is untouched
	EXPECT_TRUE(dut.allowInjection());
}

TEST(crankingNoSpark, resumesNormalSparkOnceCrankingEnds) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	LimpManager dut;

	engineConfiguration->cranking.rpm = 500;
	getCustomPage()->crankingNoSparkEnabled = true;

	// cranking: spark cut
	engine->rpmCalculator.setRpmValue(300);
	Sensor::setMockValue(SensorType::Rpm, 300);
	dut.updateState(300, 0);
	EXPECT_FALSE(dut.allowIgnition());

	// the instant RPM crosses cranking.rpm: normal ECU spark control resumes immediately
	engine->rpmCalculator.setRpmValue(2000);
	Sensor::setMockValue(SensorType::Rpm, 2000);
	dut.updateState(2000, 0);
	EXPECT_TRUE(dut.allowIgnition());
}
