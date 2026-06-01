#include "pch.h"

#include "fuel_math.h"
#include "advance_map.h"

TEST(VvtCorrections, allCorrectionsDefault_areNeutral) {
	// Default tables all zero => no correction applied
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	engine->periodicFastCallback();

	EXPECT_FLOAT_EQ(0.0f, engine->fuelComputer.vvtFuelIntakeCorrection);
	EXPECT_FLOAT_EQ(0.0f, engine->fuelComputer.vvtFuelExhaustCorrection);
	EXPECT_FLOAT_EQ(0.0f, engine->ignitionState.vvtIntakeTimingCorrection);
	EXPECT_FLOAT_EQ(0.0f, engine->ignitionState.vvtExhaustTimingCorrection);
}

TEST(VvtCorrections, fuelIntakeCorrection_positiveTrim) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Set up bin arrays
	for (int j = 0; j < VVT_TABLE_RPM_SIZE; j++) {
		config->vvtFuelIntakeCorrRpmBins[j] = static_cast<uint16_t>(j * 1000);
	}
	for (int i = 0; i < VVT_TABLE_SIZE; i++) {
		config->vvtFuelIntakeCorrVvtBins[i] = -35.0f + i * 10.0f;
	}

	// Flat +10% trim across the entire table
	setTable(config->vvtFuelIntakeCorrTable, (int8_t)10);

	// Position the intake cam at 0 deg (within the bin span)
	engine->triggerCentral.vvtPosition[0][0] = 0.0f;
	Sensor::setMockValue(SensorType::Rpm, 2000.0f);

	engine->periodicFastCallback();
	getRunningFuel(1.0f);

	EXPECT_FLOAT_EQ(10.0f, engine->fuelComputer.vvtFuelIntakeCorrection);
	// Exhaust correction unchanged (all-zero table)
	EXPECT_FLOAT_EQ(0.0f, engine->fuelComputer.vvtFuelExhaustCorrection);
}

TEST(VvtCorrections, fuelExhaustCorrection_negativeTrim) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	for (int j = 0; j < VVT_TABLE_RPM_SIZE; j++) {
		config->vvtFuelExhaustCorrRpmBins[j] = static_cast<uint16_t>(j * 1000);
	}
	for (int i = 0; i < VVT_TABLE_SIZE; i++) {
		config->vvtFuelExhaustCorrVvtBins[i] = -35.0f + i * 10.0f;
	}

	// Flat -8% trim
	setTable(config->vvtFuelExhaustCorrTable, (int8_t)-8);

	engine->triggerCentral.vvtPosition[0][1] = 0.0f;
	Sensor::setMockValue(SensorType::Rpm, 2000.0f);

	engine->periodicFastCallback();
	getRunningFuel(1.0f);

	EXPECT_FLOAT_EQ(-8.0f, engine->fuelComputer.vvtFuelExhaustCorrection);
	EXPECT_FLOAT_EQ(0.0f, engine->fuelComputer.vvtFuelIntakeCorrection);
}

TEST(VvtCorrections, ignitionIntakeCorrection_positiveTrim) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	for (int j = 0; j < VVT_TABLE_RPM_SIZE; j++) {
		config->vvtIgnIntakeCorrRpmBins[j] = static_cast<uint16_t>(j * 1000);
	}
	for (int i = 0; i < VVT_TABLE_SIZE; i++) {
		config->vvtIgnIntakeCorrVvtBins[i] = -35.0f + i * 10.0f;
	}

	// Flat +4 degree advance across the entire table
	setTable(config->vvtIgnIntakeCorrTable, 4.0f);

	engine->triggerCentral.vvtPosition[0][0] = 0.0f;
	Sensor::setMockValue(SensorType::Rpm, 2000.0f);

	getAdvanceCorrections(50.0f);

	EXPECT_NEAR(4.0f, engine->ignitionState.vvtIntakeTimingCorrection, 0.05f);
	EXPECT_FLOAT_EQ(0.0f, engine->ignitionState.vvtExhaustTimingCorrection);
}

TEST(VvtCorrections, ignitionExhaustCorrection_negativeTrim) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	for (int j = 0; j < VVT_TABLE_RPM_SIZE; j++) {
		config->vvtIgnExhaustCorrRpmBins[j] = static_cast<uint16_t>(j * 1000);
	}
	for (int i = 0; i < VVT_TABLE_SIZE; i++) {
		config->vvtIgnExhaustCorrVvtBins[i] = -35.0f + i * 10.0f;
	}

	// Flat -3 degree retard
	setTable(config->vvtIgnExhaustCorrTable, -3.0f);

	engine->triggerCentral.vvtPosition[0][1] = 0.0f;
	Sensor::setMockValue(SensorType::Rpm, 2000.0f);

	getAdvanceCorrections(50.0f);

	EXPECT_NEAR(-3.0f, engine->ignitionState.vvtExhaustTimingCorrection, 0.05f);
	EXPECT_FLOAT_EQ(0.0f, engine->ignitionState.vvtIntakeTimingCorrection);
}
