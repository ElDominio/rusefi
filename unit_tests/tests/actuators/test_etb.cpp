/*
 * @file test_etb.cpp
 *
 * @date Dec 13, 2019
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#include "pch.h"

#include "electronic_throttle_impl.h"
#include "dc_motor.h"
#include "idle_thread.h"
#include "custom_page.h"
#include "engine_state_machine.h"

#include "mocks.h"

using ::testing::_;
using ::testing::Ne;
using ::testing::Return;
using ::testing::StrictMock;

TEST(etb, initializationNoPedal) {
	StrictMock<MockEtb> mocks[ETB_COUNT];

	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	for (int i = 0; i < ETB_COUNT; i++) {
		engine->etbControllers[i] = &mocks[i];
	}

	EXPECT_CALL(mocks[0], init(DC_Throttle1, _, _, _)).WillOnce(Return(false));
	EXPECT_CALL(mocks[1], init(DC_Throttle2, _, _, _)).WillOnce(Return(false));

	// This shouldn't throw, since no throttles are configured, but no pedal is configured either
	EXPECT_NO_FATAL_ERROR(doInitElectronicThrottle(true));
}

TEST(etb, initializationMissingThrottle) {
	StrictMock<MockEtb> mocks[ETB_COUNT];

	EngineTestHelper eth(engine_type_e::TEST_ENGINE, [](engine_configuration_s* cfg) {
		cfg->etbFunctions[0] = DC_None;
		cfg->etbFunctions[1] = DC_None;
	});

	for (int i = 0; i < ETB_COUNT; i++) {
		engine->etbControllers[i] = &mocks[i];
	}

	EXPECT_CALL(mocks[0], init(DC_None, _, _, _)).Times(0);
	EXPECT_CALL(mocks[1], init(DC_None, _, _, _)).Times(0);

	// Must have a sensor configured before init
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);
	Sensor::setMockValue(SensorType::AcceleratorPedalPrimary, 0);

	// This should throw: a pedal is configured but no throttles
	EXPECT_FATAL_ERROR(doInitElectronicThrottle(true));
}

TEST(etb, initializationSingleThrottle) {
	StrictMock<MockEtb> mocks[ETB_COUNT];

	EXPECT_CALL(mocks[0], isEtbMode())
	      .WillOnce(Return(true));

	EngineTestHelper eth(engine_type_e::TEST_ENGINE, [](engine_configuration_s* cfg) {
		cfg->etbFunctions[0] = DC_Throttle1;
		cfg->etbFunctions[1] = DC_None;
	});

	for (int i = 0; i < ETB_COUNT; i++) {
		engine->etbControllers[i] = &mocks[i];
	}

	// Must have a sensor configured before init
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);
	Sensor::setMockValue(SensorType::AcceleratorPedalPrimary, 0);

	// Expect mock0 to be init as throttle 1, and PID params
	EXPECT_CALL(mocks[0], init(DC_Throttle1, _, &engineConfiguration->etb, Ne(nullptr))).WillOnce(Return(true));

	// Expect mock1 to be init as none
	EXPECT_CALL(mocks[1], init(DC_None, _, _, _)).Times(0);

	doInitElectronicThrottle(false);
}

TEST(etb, initializationSingleThrottleInSecondSlot) {
	StrictMock<MockEtb> mocks[ETB_COUNT];

	EXPECT_CALL(mocks[1], isEtbMode())
	      .WillOnce(Return(true));

	EngineTestHelper eth(engine_type_e::TEST_ENGINE, [](engine_configuration_s* cfg) {
		cfg->etbFunctions[0] = DC_None;
		cfg->etbFunctions[1] = DC_Throttle1;
	});

	for (int i = 0; i < ETB_COUNT; i++) {
		engine->etbControllers[i] = &mocks[i];
	}

	// Must have a sensor configured before init
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);
	Sensor::setMockValue(SensorType::AcceleratorPedalPrimary, 0, false);

	// Expect mock0 to be init as none
	EXPECT_CALL(mocks[0], init(DC_None, _, _, _)).Times(0);

	// Expect mock1 to be init as throttle 1, and PID params
	EXPECT_CALL(mocks[1], init(DC_Throttle1, _, &engineConfiguration->etb, Ne(nullptr))).WillOnce(Return(true));

	doInitElectronicThrottle(false);
}

TEST(etb, initializationDualThrottle) {
	StrictMock<MockEtb> mocks[ETB_COUNT];

	EXPECT_CALL(mocks[0], isEtbMode())
	      .WillOnce(Return(true));
	EXPECT_CALL(mocks[1], isEtbMode())
	      .WillOnce(Return(true));

	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	for (int i = 0; i < ETB_COUNT; i++) {
		engine->etbControllers[i] = &mocks[i];
	}

	// Must have a sensor configured before init
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);
	Sensor::setMockValue(SensorType::AcceleratorPedalPrimary, 0, false);

	Sensor::setMockValue(SensorType::Tps1, 25.0f, true);
	Sensor::setMockValue(SensorType::Tps2, 25.0f, true);

	engineConfiguration->etbFunctions[0] = DC_Throttle1;
	engineConfiguration->etbFunctions[1] = DC_Throttle2;

	// Expect mock0 to be init as throttle 1, and PID params
	EXPECT_CALL(mocks[0], init(DC_Throttle1, _, &engineConfiguration->etb, Ne(nullptr))).WillOnce(Return(true));

	// Expect mock1 to be init as throttle 2, and PID params
	EXPECT_CALL(mocks[1], init(DC_Throttle2, _, &engineConfiguration->etb, Ne(nullptr))).WillOnce(Return(true));

	doInitElectronicThrottle(false);
}

TEST(etb, initializationWastegate) {
	StrictMock<MockEtb> mocks[ETB_COUNT];

	EXPECT_CALL(mocks[0], isEtbMode())
	      .WillOnce(Return(false));

	EngineTestHelper eth(engine_type_e::TEST_ENGINE, [](engine_configuration_s* cfg) {
		cfg->etbFunctions[0] = DC_Wastegate;
		cfg->etbFunctions[1] = DC_None;
	});

	for (int i = 0; i < ETB_COUNT; i++) {
		engine->etbControllers[i] = &mocks[i];
	}

	// Expect mock0 to be init as throttle 1, and PID wastegate params
	EXPECT_CALL(mocks[0], init(DC_Wastegate, _, &engineConfiguration->etbWastegatePid, Ne(nullptr))).WillOnce(Return(true));

	// Expect mock1 to be init as none
	EXPECT_CALL(mocks[1], init(DC_None, _, _, _)).Times(0);

	doInitElectronicThrottle(false);
}

TEST(etb, initializationNoFunction) {
	StrictMock<MockMotor> motor;

	EtbController dut;

	// When init called with DC_None, should ignore the provided params and return false
	EXPECT_FALSE(dut.init(DC_None, &motor, nullptr, nullptr));

	// This should no-op, it shouldn't call motor.set(float duty)
	dut.setOutput(0.5f);
}

TEST(etb, initializationNotRedundantTps) {
	EtbController dut;

	// Needs pedal for init
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	// Not redundant, should fail upon init
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, false);

	ASSERT_FALSE(dut.init(DC_Throttle1, nullptr, nullptr, nullptr));
}

TEST(etb, initializationNotRedundantPedal) {
	EtbController dut;

	// Init pedal without redundancy
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, false);

	// Normal redundant TPS
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);

	// we require redundancy at runtime, not at initialization time. that would allow for Lua pedal to appear later on.
	ASSERT_TRUE(dut.init(DC_Throttle1, nullptr, nullptr, nullptr));
}

TEST(etb, initializationNoSensor) {
	Sensor::resetAllMocks();

	EtbController dut;

	// Needs pedal for init
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EXPECT_FALSE(dut.init(DC_Throttle1, nullptr, nullptr, nullptr));

	// Redundant
	Sensor::setMockValue(SensorType::Tps1, /*value*/0, /*mockRedundant*/true);

	// With primary TPS, should return true (ie, throttle was configured)
	EXPECT_TRUE(dut.init(DC_Throttle1, nullptr, nullptr, nullptr));
}

TEST(etb, initializationNoThrottles) {
	// This tests the case where you don't want an ETB, and expect everything to go fine
	EtbController duts[2];

	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	for (int i = 0; i < ETB_COUNT; i++) {
		engine->etbControllers[i] = &duts[i];
	}

	// Configure ETB functions, but no pedal
	engineConfiguration->etbFunctions[0] = DC_Throttle1;
	engineConfiguration->etbFunctions[1] = DC_Throttle2;

	// No pedal - don't init ETBs
	Sensor::resetMockValue(SensorType::AcceleratorPedal);

	// Not redundant TPS (aka cable throttle)
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, false);

	initElectronicThrottle();
}

TEST(etb, idlePlumbing) {
	StrictMock<MockEtb> mocks[ETB_COUNT];

	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	Sensor::setMockValue(SensorType::AcceleratorPedal, 50.0f, true);

	for (int i = 0; i < ETB_COUNT; i++) {
		engine->etbControllers[i] = &mocks[i];

		EXPECT_CALL(mocks[i], setIdlePosition(33.0f));
	}

	applyIACposition(33.0f);
}

TEST(etb, testSetpointOnlyPedal) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	EtbController etb;

	// Mock pedal map that's just passthru pedal -> target
	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float xRpm, float y) {
			return y;
		});

	// Uninitialized ETB must return unexpected (and not deference a null pointer)
	EXPECT_EQ(etb.getSetpoint(), unexpected);

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	// Check endpoints and midpoint
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);
	EXPECT_EQ(1, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 50.0f, true);
	EXPECT_EQ(50, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 100.0f, true);
	EXPECT_EQ(100, etb.getSetpoint().value_or(-1));

	// Test some floating point pedal/output values
	Sensor::setMockValue(SensorType::AcceleratorPedal, 50.8302f, true);
	EXPECT_NEAR(50.8302, etb.getSetpoint().value_or(-1), EPS4D);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 51.6605f, true);
	EXPECT_NEAR(51.6605, etb.getSetpoint().value_or(-1), EPS4D);

	// Valid but out of range - should clamp to [0, 100]
	Sensor::setMockValue(SensorType::AcceleratorPedal, -5, true);
	EXPECT_EQ(1, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 105, true);
	EXPECT_EQ(100, etb.getSetpoint().value_or(-1));

	// Test invalid pedal position - should give 0 position
	Sensor::resetMockValue(SensorType::AcceleratorPedal);
	EXPECT_EQ(1, etb.getSetpoint().value_or(-1));

	// Check that adjustable clamping works correctly
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);
	EXPECT_EQ(1, etb.getSetpoint().value_or(-1));

	engineConfiguration->etbMaximumPosition = 90;
	Sensor::setMockValue(SensorType::AcceleratorPedal, 85, true);
	EXPECT_EQ(85, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 90, true);
	EXPECT_EQ(90, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 95, true);
	EXPECT_EQ(90, etb.getSetpoint().value_or(-1));
}

// Sport Pedal: a Sport-Mode / switch activated curve multiplies the ETB throttle target,
// clamped to 100%. See electronic_throttle.cpp getSetpointEtb / isSportPedalActive.
TEST(etb, sportPedalMultiplier) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	EtbController etb;

	// Passthru pedal map: target == pedal.
	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float /*xRpm*/, float y) {
			return y;
		});

	// Remove the idle-range compression so the setpoint equals the (multiplied) pedal target.
	engineConfiguration->etbIdleThrottleRange = 0;

	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	// Configure a flat 1.5x sport pedal curve, activated by Sport Mode.
	auto& cfg = *getCustomPage();
	cfg.sportPedalActivationMode = SPORT_PEDAL_SPORT_MODE;
	for (size_t i = 0; i < efi::size(cfg.sportPedalPedalBins); i++) {
		cfg.sportPedalPedalBins[i] = i * (100.0f / 7);
		cfg.sportPedalMultValues[i] = 1.5f;
	}

	// Sport Mode off -> sport pedal inactive -> passthru target.
	engine->module<EngineStateMachine>().unmock().engineSmIsSportMode = false;
	Sensor::setMockValue(SensorType::AcceleratorPedal, 40.0f, true);
	EXPECT_NEAR(40, etb.getSetpoint().value_or(-1), EPS4D);

	// Sport Mode on -> sport pedal active -> target multiplied by 1.5.
	engine->module<EngineStateMachine>().unmock().engineSmIsSportMode = true;
	Sensor::setMockValue(SensorType::AcceleratorPedal, 40.0f, true);
	EXPECT_NEAR(60, etb.getSetpoint().value_or(-1), EPS4D);

	// Multiplied target must be clamped to 100% (80 * 1.5 = 120 -> 100).
	Sensor::setMockValue(SensorType::AcceleratorPedal, 80.0f, true);
	EXPECT_NEAR(100, etb.getSetpoint().value_or(-1), EPS4D);

	// Activation OFF -> feature disabled even with Sport Mode active.
	cfg.sportPedalActivationMode = SPORT_PEDAL_OFF;
	Sensor::setMockValue(SensorType::AcceleratorPedal, 40.0f, true);
	EXPECT_NEAR(40, etb.getSetpoint().value_or(-1), EPS4D);
}

TEST(etb, setpointSecondThrottleTrim) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Mock pedal map that's just passthru pedal -> target
	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float xRpm, float y) {
			return y;
		});

	// Should get called with the un-adjusted setpoint
	StrictMock<MockVp3d> throttleTrimTable;
	EXPECT_CALL(throttleTrimTable, getValue(0, 47))
		.WillOnce(Return(4));

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController2 etb(throttleTrimTable);
	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	Sensor::setMockValue(SensorType::AcceleratorPedal, 47, true);
	EXPECT_EQ(51, etb.getSetpoint().value_or(-1));
}

TEST(etb, setpointIdle) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Use ETB for idle, but don't give it any range (yet)
	engineConfiguration->etbIdleThrottleRange = 0;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;

	// Mock pedal map that's just passthru pedal -> target
	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float xRpm, float y) {
			return y;
		});
	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	// No idle range, should just pass pedal
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);
	EXPECT_EQ(1, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 50.0f, true);
	EXPECT_EQ(50, etb.getSetpoint().value_or(-1));

	// Idle should now have 10% range
	engineConfiguration->etbIdleThrottleRange = 10;

	// 50% idle position should increase setpoint by 5% when closed, and 0% when open
	etb.setIdlePosition(50);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);
	EXPECT_FLOAT_EQ(5, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 50.0f, true);
	EXPECT_FLOAT_EQ(52.5, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 100.0f, true);
	EXPECT_FLOAT_EQ(100, etb.getSetpoint().value_or(-1));

	// 100% setpoint should increase by 10% closed, scaled 0% at wot
	etb.setIdlePosition(100);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);
	EXPECT_FLOAT_EQ(10, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 50.0f, true);
	EXPECT_FLOAT_EQ(55, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 100.0f, true);
	EXPECT_FLOAT_EQ(100, etb.getSetpoint().value_or(-1));

	// 125% setpoint should clamp to 10% increase
	etb.setIdlePosition(125);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);
	EXPECT_FLOAT_EQ(10, etb.getSetpoint().value_or(-1));
	Sensor::setMockValue(SensorType::AcceleratorPedal, 50.0f, true);
	EXPECT_FLOAT_EQ(55, etb.getSetpoint().value_or(-1));
}

TEST(etb, setpointCrankingTpsTarget) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	engineConfiguration->cranking.rpm = 500;
	engineConfiguration->crankingTpsTargetEnabled = true;
	engineConfiguration->cranking.tpsTarget = 65;

	// Idle blend active too, so we can confirm cranking bypasses it entirely
	engineConfiguration->etbIdleThrottleRange = 10;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 20.0f, true);

	EtbController etb;

	// Mock pedal map that's just passthru pedal -> target
	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float xRpm, float y) {
			return y;
		});
	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);
	etb.setIdlePosition(50);

	// Below cranking.rpm: forced to the configured cranking TPS target, ignoring pedal/idle blend
	engine->rpmCalculator.setRpmValue(300);
	Sensor::setMockValue(SensorType::Rpm, 300);
	EXPECT_FLOAT_EQ(65, etb.getSetpoint().value_or(-1));

	// The instant RPM crosses cranking.rpm: back to the normal pedal/idle blend
	engine->rpmCalculator.setRpmValue(2000);
	Sensor::setMockValue(SensorType::Rpm, 2000);
	EXPECT_FLOAT_EQ(24, etb.getSetpoint().value_or(-1));

	// Disabled: even while cranking, normal pedal/idle blend applies
	engineConfiguration->crankingTpsTargetEnabled = false;
	engine->rpmCalculator.setRpmValue(300);
	Sensor::setMockValue(SensorType::Rpm, 300);
	EXPECT_FLOAT_EQ(24, etb.getSetpoint().value_or(-1));
}

TEST(etb, setpointRevLimit) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Hard limit 5750, managed for 750rpm below that, i.e. starting at 5000
	engineConfiguration->rpmHardLimit = 5750;
	engineConfiguration->etbRevLimitRange = 750;
	engineConfiguration->cutEtbOnRpmLimit = true;
	// Zero gains isolate the seed value: PID output is just the configured seed regardless of error
	engineConfiguration->etbRevLimitSeedTps = 15;
	engineConfiguration->etbRevLimitKp = 0;
	engineConfiguration->etbRevLimitKi = 0;
	engineConfiguration->etbRevLimitKd = 0;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;

	// Mock pedal map to just return 80%
	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float, float) {
			return 80;
		});
	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	// Below threshold, should return unadjusted throttle
	Sensor::setMockValue(SensorType::Rpm,  1000);
	eth.engine.periodicFastCallback();
	EXPECT_NEAR(80, etb.getSetpoint().value_or(-1), 1e-4);

	// Just below threshold, should return unadjusted throttle
	Sensor::setMockValue(SensorType::Rpm,  4999);
	eth.engine.periodicFastCallback();
	EXPECT_NEAR(80, etb.getSetpoint().value_or(-1), 1e-4);

	// At/above threshold: PID has just engaged and is seeded with the configured TPS
	Sensor::setMockValue(SensorType::Rpm, 5375);
	eth.engine.periodicFastCallback();
	EXPECT_NEAR(15, etb.getSetpoint().value_or(-1), 1e-4);

	// Past the hard limit, still managed by the (seeded) PID, not a hard cut to zero.
	// With zero gains the PID output doesn't move from the seed regardless of dt/error.
	eth.moveTimeForwardMs(10);
	Sensor::setMockValue(SensorType::Rpm, 6000);
	eth.engine.periodicFastCallback();
	EXPECT_NEAR(15, etb.getSetpoint().value_or(-1), 1e-4);
}

TEST(etb, setpointRevLimitDisabledByDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Even configured, the ETB rev limiter PID must stay out of the way unless explicitly enabled
	engineConfiguration->rpmHardLimit = 5750;
	engineConfiguration->etbRevLimitRange = 750;
	engineConfiguration->cutEtbOnRpmLimit = false;

	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;

	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float, float) {
			return 80;
		});
	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	Sensor::setMockValue(SensorType::Rpm, 6000);
	eth.engine.periodicFastCallback();
	EXPECT_NEAR(80, etb.getSetpoint().value_or(-1), 1e-4);
}

TEST(etb, setpointNoPedalMap) {
	EtbController etb;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	// Don't pass a pedal map
	etb.init(DC_Throttle1, nullptr, nullptr, nullptr);

	EXPECT_EQ(etb.getSetpoint(), unexpected);
}

TEST(etb, setpointIdleValveController) {
	EtbController etb;

	etb.init(DC_IdleValve, nullptr, nullptr, nullptr);

	etb.setIdlePosition(0);
	EXPECT_FLOAT_EQ(0, etb.getSetpoint().value_or(-1));
	etb.setIdlePosition(50);
	EXPECT_FLOAT_EQ(50, etb.getSetpoint().value_or(-1));
	etb.setIdlePosition(100);
	EXPECT_FLOAT_EQ(100, etb.getSetpoint().value_or(-1));

	// Out of range should be clamped
	etb.setIdlePosition(-10);
	EXPECT_FLOAT_EQ(0, etb.getSetpoint().value_or(-1));
	etb.setIdlePosition(110);
	EXPECT_FLOAT_EQ(100, etb.getSetpoint().value_or(-1));
}

TEST(etb, setpointWastegateController) {
	EtbController etb;

	etb.init(DC_Wastegate, nullptr, nullptr, nullptr);

	etb.setWastegatePosition(0);
	EXPECT_FLOAT_EQ(0, etb.getSetpoint().value_or(-1));
	etb.setWastegatePosition(50);
	EXPECT_FLOAT_EQ(50, etb.getSetpoint().value_or(-1));
	etb.setWastegatePosition(100);
	EXPECT_FLOAT_EQ(100, etb.getSetpoint().value_or(-1));

	// Out of range should be clamped
	etb.setWastegatePosition(-10);
	EXPECT_FLOAT_EQ(0, etb.getSetpoint().value_or(-1));
	etb.setWastegatePosition(110);
	EXPECT_FLOAT_EQ(100, etb.getSetpoint().value_or(-1));
}

TEST(etb, setpointLuaAdder) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;

	// Mock pedal map to just return 50%
	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float, float) {
			return 50;
		});
	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	// No adjustment, should be unadjusted
	etb.setLuaAdjustment(0);
	EXPECT_EQ(50, etb.getSetpoint().value_or(-1));

	// Normal adjustments should do as expected
	etb.setLuaAdjustment(10);
	EXPECT_EQ(60, etb.getSetpoint().value_or(-1));
	etb.setLuaAdjustment(-10);
	EXPECT_EQ(40, etb.getSetpoint().value_or(-1));

	// Crazy adjustments don't cause unreasonable target
	etb.setLuaAdjustment(1000);
	EXPECT_EQ(100, etb.getSetpoint().value_or(-1));
	etb.setLuaAdjustment(-1000);
	EXPECT_EQ(1, etb.getSetpoint().value_or(-1));

	int startTime = 1e6;
	setTimeNowUs(startTime);

	// Adjustment works immediately after setting
	etb.setLuaAdjustment(10);
	EXPECT_EQ(60, etb.getSetpoint().value_or(-1));

	// Adjustment works 0.19 second after setting
	setTimeNowUs(startTime + 0.19 * 1e6);
	EXPECT_EQ(60, etb.getSetpoint().value_or(-1));

	// Adjustment resets to 0 after 0.21 second
	setTimeNowUs(startTime + 0.21 * 1e6);
	EXPECT_EQ(50, etb.getSetpoint().value_or(-1));
}

TEST(etb, etbTpsSensor) {
	// Throw some distinct values on the TPS sensors so we can identify that we're getting the correct one
	Sensor::setMockValue(SensorType::Tps1, 25.0f, true);
	Sensor::setMockValue(SensorType::Tps2, 75.0f, true);
	Sensor::setMockValue(SensorType::WastegatePosition, 33.0f);
	Sensor::setMockValue(SensorType::IdlePosition, 66.0f);

	// Redundant accelerator pedal required for init
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);

	// Test first throttle
	{
		EtbController etb;
		etb.init(DC_Throttle1, nullptr, nullptr, nullptr);
		EXPECT_EQ(etb.observePlant().Value, 25.0f);
	}

	// Test second throttle
	{
		EtbController etb;
		etb.init(DC_Throttle2, nullptr, nullptr, nullptr);
		EXPECT_EQ(etb.observePlant().Value, 75.0f);
	}

	// Test wastegate control
	{
		EtbController etb;
		etb.init(DC_Wastegate, nullptr, nullptr, nullptr);
		EXPECT_EQ(etb.observePlant().Value, 33.0f);
	}

	// Test idle valve control
	{
		EtbController etb;
		etb.init(DC_IdleValve, nullptr, nullptr, nullptr);
		EXPECT_EQ(etb.observePlant().Value, 66.0f);
	}
}

TEST(etb, setOutputInvalid) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Redundant TPS & accelerator pedal required for init
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);

	StrictMock<MockMotor> motor;

	EtbController etb;
	etb.init(DC_Throttle1, &motor, nullptr, nullptr);

	// Should be disabled in case of unexpected
	EXPECT_CALL(motor, disable(_));

	etb.setOutput(unexpected);
}

TEST(etb, setOutputValid) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	StrictMock<MockMotor> motor;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;
	etb.init(DC_Throttle1, &motor, nullptr, nullptr);

	// Should be enabled and value set
	EXPECT_CALL(motor, enable());
	EXPECT_CALL(motor, set(0.25f))
		.WillOnce(Return(false));

	etb.setOutput(25.0f);
}

TEST(etb, setOutputValid2) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	StrictMock<MockMotor> motor;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;
	etb.init(DC_Throttle1, &motor, nullptr, nullptr);

	// Should be enabled and value set
	EXPECT_CALL(motor, enable());
	EXPECT_CALL(motor, set(-0.25f))
		.WillOnce(Return(false));

	etb.setOutput(-25.0f);
}

TEST(etb, setOutputOutOfRangeHigh) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	StrictMock<MockMotor> motor;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;
	etb.init(DC_Throttle1, &motor, nullptr, nullptr);

	// Should be enabled and value set
	EXPECT_CALL(motor, enable());
	EXPECT_CALL(motor, set(0.90f));

	// Off scale - should get clamped to 90%
	etb.setOutput(110);
}

TEST(etb, setOutputOutOfRangeLow) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	StrictMock<MockMotor> motor;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;
	etb.init(DC_Throttle1, &motor, nullptr, nullptr);

	// Should be enabled and value set
	EXPECT_CALL(motor, enable());
	EXPECT_CALL(motor, set(-0.90f));

	// Off scale - should get clamped to -90%
	etb.setOutput(-110);
}

TEST(etb, setOutputPauseControl) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	StrictMock<MockMotor> motor;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;
	etb.init(DC_Throttle1, &motor, nullptr, nullptr);

	// Pause control - should get no output
	engineConfiguration->pauseEtbControl = true;

	// Disable should be called, and set shouldn't be called
	EXPECT_CALL(motor, disable(_));

	etb.setOutput(25.0f);
}

TEST(etb, setOutputLimpHome) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	StrictMock<MockMotor> motor;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;
	etb.init(DC_Throttle1, &motor, nullptr, nullptr);

	// Should be disabled when in ETB limp mode
	EXPECT_CALL(motor, disable(_));

	// Trip a fatal error
	getLimpManager()->fatalError();

	etb.setOutput(25.0f);
}

TEST(etb, closedLoopPid) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	pid_s pid = {};
	pid.pFactor = 5;
	pid.maxValue = 75;
	pid.minValue = -60;

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController etb;
	etb.init(DC_Throttle1, nullptr, &pid, nullptr);

	// Disable autotune for now
	engine->etbAutoTune = false;

	// Setpoint greater than actual, should be positive output
	EXPECT_FLOAT_EQ(etb.getClosedLoop(50, 40).value_or(-1), 50);
	// Likewise but negative
	EXPECT_FLOAT_EQ(etb.getClosedLoop(50, 52).value_or(-1), -10);

	// Test PID limiting
	EXPECT_FLOAT_EQ(etb.getClosedLoop(50, 70).value_or(-1), -60);
	EXPECT_FLOAT_EQ(etb.getClosedLoop(50, 30).value_or(-1), 75);
}

TEST(etb, jamDetection) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	MockIgnitionController ignController;

	EXPECT_CALL(ignController, getIgnState).WillRepeatedly(Return(true));

	// This only works when the ignition is on!
	engine->module<IgnitionController>().set(&ignController);

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	// Limit of 5%, 1 second
	engineConfiguration->etbJamDetectThreshold = 5;
	engineConfiguration->etbJamTimeout = 1;

	EtbController etb;
	etb.init(DC_Throttle1, nullptr, nullptr, nullptr);

	setTimeNowUs(0);

	// Reset timer while under error limit
	etb.checkJam(10, 14);
	EXPECT_EQ(etb.jamTimer, 0);
	EXPECT_FALSE(etb.jamDetected);

	// Start a jam
	etb.checkJam(10, 16);

	// Just under time limit, no jam yet
	setTimeNowUs(0.9e6);
	etb.checkJam(10, 16);
	EXPECT_NEAR(etb.jamTimer, 0.9f, 1e-3);
	EXPECT_FALSE(etb.jamDetected);

	// Above the time limit, jam detected!
	setTimeNowUs(1.1e6);
	etb.checkJam(10, 16);
	EXPECT_TRUE(etb.jamDetected);
}

TEST(etb, openLoopThrottle) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Redundant TPS & accelerator pedal required for init
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);

	EtbController etb;
	etb.init(DC_Throttle1, nullptr, nullptr, nullptr);

	// Map [0, 100] -> [-50, 50]
	setLinearCurve(config->etbBiasBins, 0, 100);
	setLinearCurve(config->etbBiasValues, -50, 50);

	EXPECT_NEAR(-50, etb.getOpenLoop(0).value_or(-1), EPS4D);
	EXPECT_NEAR(-25, etb.getOpenLoop(25).value_or(-1), EPS4D);
	EXPECT_NEAR(0, etb.getOpenLoop(50).value_or(-1), EPS4D);
	EXPECT_NEAR(25, etb.getOpenLoop(75).value_or(-1), EPS4D);
	EXPECT_NEAR(50, etb.getOpenLoop(100).value_or(-1), EPS4D);
}

TEST(etb, openLoopDCWastegate) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Redundant TPS & accelerator pedal required for init
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);

	EtbController etb;
	etb.init(DC_Wastegate, nullptr, nullptr, nullptr);

	// Map [0, 100] -> [-50, 50]
	setLinearCurve(config->dcWastegateBiasBins, 0, 100);
	setLinearCurve(config->dcWastegateBiasValues, -50, 50);

	EXPECT_NEAR(-50, etb.getOpenLoop(0).value_or(-1), EPS2D);
	EXPECT_NEAR(-24.48, etb.getOpenLoop(25).value_or(-1), EPS2D);
	EXPECT_NEAR(0.47, etb.getOpenLoop(50).value_or(-1), EPS2D);
	EXPECT_NEAR(25.51, etb.getOpenLoop(75).value_or(-1), EPS2D);
	EXPECT_NEAR(50, etb.getOpenLoop(100).value_or(-1), EPS2D);
}

TEST(etb, openLoopNonThrottle) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Redundant TPS & accelerator pedal required for init
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0, true);

	EtbController etb;
	etb.init(DC_IdleValve, nullptr, nullptr, nullptr);

	// Map [0, 100] -> [-50, 50]
	setLinearCurve(config->etbBiasBins, 0, 100);
	setLinearCurve(config->etbBiasValues, -50, 50);

	// Should all return 0, as non-throttles don't use open loop table
	EXPECT_EQ(0, etb.getOpenLoop(0).value_or(-1));
	EXPECT_EQ(0, etb.getOpenLoop(25).value_or(-1));
	EXPECT_EQ(0, etb.getOpenLoop(50).value_or(-1));
	EXPECT_EQ(0, etb.getOpenLoop(75).value_or(-1));
	EXPECT_EQ(0, etb.getOpenLoop(100).value_or(-1));
}

TEST(etb, tractionControlEtbDrop) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Table stores a positive drop magnitude (0-100%); it can only remove throttle, never add it.
	setTable(engineConfiguration->tractionControlEtbDrop, 10);
	setLinearCurve(engineConfiguration->tractionControlSlipBins, /*from*/0.9, /*to*/1.2, 0.05);
	setLinearCurve(engineConfiguration->tractionControlSpeedBins, /*from*/10, /*to*/120, 5);

	// Mock pedal map that's just passthru pedal -> target
	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float xRpm, float y) {
			return y;
		});

	// Must have TPS & PPS initialized for ETB setup
	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 0.0f, true);

	EtbController1 etb;
	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	Sensor::setMockValue(SensorType::AcceleratorPedal, 47, true);
	engine->tractionController.update();
	EXPECT_EQ(37, etb.getSetpoint().value_or(-1));

	// test correct X/Y on table
	Sensor::setMockValue(SensorType::VehicleSpeed, 40.0);
	Sensor::setMockValue(SensorType::WheelSlipRatio, 0.9);

	// tractionControlEtbDrop is indexed [slipIndex][speedIndex]. Slip = 0.9 is exactly slip bin 0
	// (frac 0), so only slip-row 0 contributes at the probe below regardless of speed -- write to
	// slip-row 1 so these cells are guaranteed unreached rather than relying on the speed bin's
	// fractional position not to overlap.
	engineConfiguration->tractionControlEtbDrop[1][0] = 15;
	engineConfiguration->tractionControlEtbDrop[1][1] = 15;

	size_t lastYIndex = TRACTION_CONTROL_ETB_DROP_SLIP_SIZE - 1;
	size_t lastXIndex = TRACTION_CONTROL_ETB_DROP_SPEED_SIZE - 1;

	engineConfiguration->tractionControlEtbDrop[lastYIndex - 1][lastXIndex - 1] = 25;
	engineConfiguration->tractionControlEtbDrop[lastYIndex][lastXIndex] = 25;

	// [1][0]/[1][1] aren't the cells sampled at this point, so the setpoint is unaffected: still 37.
	// The corner cells (25) are only reached at the extreme coordinates probed below: 47 - 25 = 22.

	engine->tractionController.update();
	EXPECT_EQ(37, etb.getSetpoint().value_or(-1));

	Sensor::setMockValue(SensorType::VehicleSpeed, 120.0);
	Sensor::setMockValue(SensorType::WheelSlipRatio, 1.2);

	engine->tractionController.update();
	EXPECT_EQ(22, etb.getSetpoint().value_or(-1));
}

TEST(etb, tractionControlEtbDropNeverAddsThrottle) {
	// Regression test: even an out-of-range negative raw table value (e.g. leftover from an old
	// tune, or flash corruption) must never result in throttle being added by traction control.
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	setTable(engineConfiguration->tractionControlEtbDrop, -15);
	setLinearCurve(engineConfiguration->tractionControlSlipBins, /*from*/0.9, /*to*/1.2, 0.05);
	setLinearCurve(engineConfiguration->tractionControlSpeedBins, /*from*/10, /*to*/120, 5);

	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float xRpm, float y) {
			return y;
		});

	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 47, true);

	EtbController1 etb;
	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	Sensor::setMockValue(SensorType::VehicleSpeed, 40.0);
	Sensor::setMockValue(SensorType::WheelSlipRatio, 0.9);
	engine->tractionController.update();

	EXPECT_EQ(47, etb.getSetpoint().value_or(-1));
}

TEST(etb, tractionControlHoldAndDecay) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Enable traction control hold/decay
	engineConfiguration->tractionControlHoldTime = 100;  // 100 ms
	engineConfiguration->tractionControlDecayTime = 200; // 200 ms

	// Set slip bins starting from 0.0 so that 0.0 slip returns 0 drop
	setLinearCurve(engineConfiguration->tractionControlSlipBins, /*from*/0.0, /*to*/1.0, 0.2);
	setLinearCurve(engineConfiguration->tractionControlSpeedBins, /*from*/10, /*to*/120, 5);

	// Set drop to 0% for slip bin 0 (slip = 0.0) and 10% (positive table value) for slip bins 1-5,
	// the same for every speed bin. tractionControlEtbDrop is indexed [slipIndex][speedIndex].
	for (int speedIdx = 0; speedIdx < TRACTION_CONTROL_ETB_DROP_SPEED_SIZE; speedIdx++) {
		engineConfiguration->tractionControlEtbDrop[0][speedIdx] = 0;
		for (int slipIdx = 1; slipIdx < TRACTION_CONTROL_ETB_DROP_SLIP_SIZE; slipIdx++) {
			engineConfiguration->tractionControlEtbDrop[slipIdx][speedIdx] = 10;
		}
	}

	// Mock pedal map
	StrictMock<MockVp3d> pedalMap;
	EXPECT_CALL(pedalMap, getValue(_, _))
		.WillRepeatedly([](float, float y) {
			return y;
		});

	Sensor::setMockValue(SensorType::Tps1Primary, 0);
	Sensor::setMockValue(SensorType::Tps1, 0.0f, true);
	Sensor::setMockValue(SensorType::AcceleratorPedal, 50.0f, true);

	EtbController1 etb;
	etb.init(DC_Throttle1, nullptr, nullptr, &pedalMap);

	engine->tractionController.init();

	// Time = 0. No slip.
	setTimeNowUs(0);
	Sensor::setMockValue(SensorType::VehicleSpeed, 40.0);
	Sensor::setMockValue(SensorType::WheelSlipRatio, 0.0);
	engine->tractionController.update();
	EXPECT_EQ(50, etb.getSetpoint().value_or(-1)); // pedal only

	// Time = 5ms. Slip increases to 1.0 (raw drop should be -10).
	// The "is slip increasing" trigger only re-checks every tractionControlSlipCheckRateMs (floored
	// to the fast-loop period, 5ms, since the field is unset/0 here) -- advance by that much so this
	// tick's check actually runs, rather than two updates at the same simulated instant.
	setTimeNowUs(5000); // 5 ms
	Sensor::setMockValue(SensorType::WheelSlipRatio, 1.0);
	engine->tractionController.update();
	EXPECT_EQ(40, etb.getSetpoint().value_or(-1)); // 50 - 10 = 40

	// Time = 55ms (50ms after slip increase). Slip decreases to 0.0. But we hold for 100ms.
	setTimeNowUs(55000);
	Sensor::setMockValue(SensorType::WheelSlipRatio, 0.0);
	engine->tractionController.update();
	EXPECT_EQ(40, etb.getSetpoint().value_or(-1)); // held at 40

	// Time = 105ms (100ms after slip increase). Slip still 0. Hold timer is exactly at 0 (since
	// 100ms hold time elapsed). It should start to decay. Since decay is 200ms, and 50ms has
	// elapsed since hold expired (100ms - 50ms),
	// wait, hold timer started when slip was 1.0, and slip decreased 50ms later.
	// Wait, does the hold timer reset when slip decreases? No, it only resets when slip increases.
	// So the hold timer started at the slip-increase instant. It expires 100ms later.
	// At 20ms into decay, it should decay.
	// Let's test at 50ms into decay.
	// Decay progress = 50ms / 200ms = 0.25.
	// Applied drop = heldDrop * (1 - 0.25) = -10 * 0.75 = -7.5%.
	// Note: tcEtbDrop is an int8_t, so -7.5% is truncated to -7%.
	// Setpoint = 50 - 7 = 43.
	setTimeNowUs(155000); // 150ms after slip increase
	engine->tractionController.update();
	EXPECT_NEAR(43.0f, etb.getSetpoint().value_or(-1), EPS4D);

	// 150ms into decay.
	// Decay progress = 150ms / 200ms = 0.75.
	// Applied drop = -10 * (1 - 0.75) = -2.5%.
	// Note: tcEtbDrop is an int8_t, so -2.5% is truncated to -2%.
	// Setpoint = 50 - 2 = 48.
	setTimeNowUs(255000); // 250ms after slip increase
	engine->tractionController.update();
	EXPECT_NEAR(48.0f, etb.getSetpoint().value_or(-1), EPS4D);

	// Hold expired at 100ms + 200ms decay = 300ms after slip increase.
	// Decay should be complete (applied drop = 0).
	setTimeNowUs(305000); // 300ms after slip increase
	engine->tractionController.update();
	EXPECT_EQ(50, etb.getSetpoint().value_or(-1));
}



