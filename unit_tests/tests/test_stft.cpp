#include "pch.h"

#include "closed_loop_fuel_cell.h"
#include "closed_loop_fuel.h"
#include "engine_state_machine.h"
#include "custom_page.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

class MockClCell : public ClosedLoopFuelCellBase {
public:
	MOCK_METHOD(float, getLambdaError, (), (const));
	MOCK_METHOD(float, getMaxAdjustment, (), (const));
	MOCK_METHOD(float, getMinAdjustment, (), (const));
	MOCK_METHOD(float, getIntegratorGain, (), (const));
};

TEST(ClosedLoopCell, TestDeadband) {
	StrictMock<MockClCell> cl;

	// Error is more than deadtime, so nothing else should be called
	EXPECT_CALL(cl, getLambdaError())
		.WillOnce(Return(0.05f));

	cl.update(0.1f, true);

	// Should be zero adjustment
	EXPECT_FLOAT_EQ(cl.getAdjustment(), 1.0f);
}

TEST(ClosedLoopFuelCell, AdjustRate) {
	StrictMock<MockClCell> cl;

	EXPECT_CALL(cl, getLambdaError())
		.WillOnce(Return(0.1f));
	EXPECT_CALL(cl, getMinAdjustment())
		.WillOnce(Return(-0.2f));
	EXPECT_CALL(cl, getMaxAdjustment())
		.WillOnce(Return(0.2f));
	EXPECT_CALL(cl, getIntegratorGain())
		.WillOnce(Return(2.0f));

	cl.update(0.0f, false);

	// Should have integrated 0.2 * dt
	// dt = 1000.0f / FAST_CALLBACK_PERIOD_MS
	EXPECT_FLOAT_EQ(cl.getAdjustment(), 1 + (0.2f / (1000.0f / FAST_CALLBACK_PERIOD_MS)));
}

TEST(ClosedLoopFuel, CellSelection) {
	// computeStftBin reads engineConfiguration (for the state-based-STFT branch), so a valid
	// engine context is required. useEngineStateMachine defaults off, so the threshold path runs.
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	ShortTermFuelTrim stft;
	stft_s cfg;

	// Sensible region config
	cfg.maxIdleRegionRpm = 1500;
	cfg.minPowerLoad = 80;
	cfg.maxOverrunLoad = 30;

	stft.init(&cfg);

	// Test idle
	EXPECT_EQ(0u, stft.computeStftBin(1000, 10, cfg));
	EXPECT_EQ(0u, stft.computeStftBin(1000, 50, cfg));
	EXPECT_EQ(0u, stft.computeStftBin(1000, 90, cfg));

	// Test overrun
	EXPECT_EQ(1u, stft.computeStftBin(2000, 10, cfg));
	EXPECT_EQ(1u, stft.computeStftBin(4000, 10, cfg));
	EXPECT_EQ(1u, stft.computeStftBin(10000, 10, cfg));

	// Test load
	EXPECT_EQ(2u, stft.computeStftBin(2000, 90, cfg));
	EXPECT_EQ(2u, stft.computeStftBin(4000, 90, cfg));
	EXPECT_EQ(2u, stft.computeStftBin(10000, 90, cfg));

	// Main cell
	EXPECT_EQ(3u, stft.computeStftBin(2000, 50, cfg));
	EXPECT_EQ(3u, stft.computeStftBin(4000, 50, cfg));
	EXPECT_EQ(3u, stft.computeStftBin(10000, 50, cfg));
}

TEST(ClosedLoopFuel, StateBasedRegionMapping) {
	using S = EngineStateMachineState;

	// Idle group -> idle cell
	EXPECT_EQ(ftRegionIdle, ShortTermFuelTrim::regionForSmState(S::Idle));
	EXPECT_EQ(ftRegionIdle, ShortTermFuelTrim::regionForSmState(S::Afterstart));

	// Overrun group -> overrun cell
	EXPECT_EQ(ftRegionOverrun, ShortTermFuelTrim::regionForSmState(S::Coasting));
	EXPECT_EQ(ftRegionOverrun, ShortTermFuelTrim::regionForSmState(S::Overrun));

	// WOT -> power cell
	EXPECT_EQ(ftRegionPower, ShortTermFuelTrim::regionForSmState(S::WOT));

	// Cruising and all inactive-STFT states -> main cruise cell
	EXPECT_EQ(ftRegionCruise, ShortTermFuelTrim::regionForSmState(S::Cruising));
	EXPECT_EQ(ftRegionCruise, ShortTermFuelTrim::regionForSmState(S::Off));
	EXPECT_EQ(ftRegionCruise, ShortTermFuelTrim::regionForSmState(S::Cranking));
	EXPECT_EQ(ftRegionCruise, ShortTermFuelTrim::regionForSmState(S::Transient));
	EXPECT_EQ(ftRegionCruise, ShortTermFuelTrim::regionForSmState(S::LaunchControl));
	EXPECT_EQ(ftRegionCruise, ShortTermFuelTrim::regionForSmState(S::Limp));
}

TEST(ClosedLoopFuel, StateBasedSelectionOverridesThresholds) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	ShortTermFuelTrim stft;
	stft_s cfg;
	cfg.maxIdleRegionRpm = 1500;
	cfg.minPowerLoad = 80;
	cfg.maxOverrunLoad = 30;
	stft.init(&cfg);

	// With state-based STFT off, RPM/load thresholds pick the region (idle here).
	engineConfiguration->useEngineStateMachine = true;
	getCustomPage()->stateBasedStft = false;
	EXPECT_EQ(ftRegionIdle, stft.computeStftBin(1000, 10, cfg));

	// With state-based STFT on, the SM state wins regardless of RPM/load. The SM defaults to
	// the Off state, which maps to the main cruise cell.
	getCustomPage()->stateBasedStft = true;
	EXPECT_EQ(ftRegionCruise, stft.computeStftBin(1000, 10, cfg));
}

TEST(ClosedLoopFuel, afrLimits) {
	ShortTermFuelTrim stft;
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->stft.minAfr = 10;  // 10.0 AFR
	engineConfiguration->stft.maxAfr = 18;  // 18.0 AFR

	stft.init(&engineConfiguration->stft);

	Sensor::setMockValue(SensorType::Lambda1, 0.1f);
	EXPECT_NE(stft.getLearningState(SensorType::Lambda1), stftEnabled);

	Sensor::setMockValue(SensorType::Lambda1, 1.0f);
	EXPECT_EQ(stft.getLearningState(SensorType::Lambda1), stftEnabled);

	Sensor::setMockValue(SensorType::Lambda1, 2.0f);
	EXPECT_NE(stft.getLearningState(SensorType::Lambda1), stftEnabled);
}
