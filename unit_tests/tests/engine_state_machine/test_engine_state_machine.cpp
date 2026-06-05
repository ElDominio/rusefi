#include "pch.h"

#include "engine_state_machine.h"

// Default cranking RPM threshold used by TEST_ENGINE
static constexpr float TEST_CRANKING_RPM = 400.0f;
static constexpr float TEST_RUNNING_RPM  = 800.0f;

static void setupSmConfig() {
	engineConfiguration->useEngineStateMachine = true;
	engineConfiguration->smIdleTpsThreshold = 10;
	engineConfiguration->smWotTpsThreshold = 90;
	engineConfiguration->smTransientTpsRateThreshold = 20; // %/s
	engineConfiguration->smAfterStartDuration = 3;         // seconds
	engineConfiguration->smIdleExitRpm = 1200;
	engineConfiguration->smIdleRpmHysteresis = 50;
	engineConfiguration->smCrankingRpmHysteresis = 50;
}

static EngineStateMachine& getSm() {
	return engine->module<EngineStateMachine>().unmock();
}

static EngineStateMachineState runAndGetState() {
	engine->periodicSlowCallback();
	return getSm().getCurrentState();
}

// ---- Disabled by default ----

TEST(EngineStateMachine, disabledByDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);

	// Feature is off by default
	ASSERT_FALSE(engineConfiguration->useEngineStateMachine);

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);

	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.isEnabled());
	// With SM disabled, engineSmCurrentState must not have been computed
	EXPECT_FALSE(sm.engineSmEnabled);
}

// ---- State transitions ----

TEST(EngineStateMachine, offState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	engine->rpmCalculator.setRpmValue(0);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);

	EXPECT_EQ(EngineStateMachineState::Off, runAndGetState());
}

TEST(EngineStateMachine, crankingState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	// Below cranking RPM threshold → Cranking
	engine->rpmCalculator.setRpmValue(TEST_CRANKING_RPM - 50.0f);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);

	EXPECT_EQ(EngineStateMachineState::Cranking, runAndGetState());
}

TEST(EngineStateMachine, afterStartState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	// Transition to RUNNING resets engineStartTimer
	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);

	// Immediately after reaching running RPM → Afterstart
	EXPECT_EQ(EngineStateMachineState::Afterstart, runAndGetState());

	// Advance past afterstart duration
	advanceTimeUs(static_cast<int>(engineConfiguration->smAfterStartDuration * 1e6f) + 100000);

	// Now with same conditions → Cruising (throttle mid-range)
	EXPECT_EQ(EngineStateMachineState::Cruising, runAndGetState());
}

TEST(EngineStateMachine, wotState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);
	// Burn through afterstart
	advanceTimeUs(static_cast<int>(engineConfiguration->smAfterStartDuration * 1e6f) + 100000);

	// WOT above threshold
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 95.0f);
	EXPECT_EQ(EngineStateMachineState::WOT, runAndGetState());
}

TEST(EngineStateMachine, wotPriorityOverTransient) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	advanceTimeUs(static_cast<int>(engineConfiguration->smAfterStartDuration * 1e6f) + 100000);

	// First callback at low TPS to seed m_prevTps
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);
	runAndGetState();

	// Big jump in TPS — both WOT and Transient conditions are true
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 95.0f);
	// WOT has higher priority than Transient
	EXPECT_EQ(EngineStateMachineState::WOT, runAndGetState());
}

TEST(EngineStateMachine, transientState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	advanceTimeUs(static_cast<int>(engineConfiguration->smAfterStartDuration * 1e6f) + 100000);

	// Seed m_prevTps at mid throttle
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 30.0f);
	runAndGetState();

	// Step to 55% — delta = 25%, at 20Hz that's 25 * 50 = 1250 %/s > threshold of 20
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 55.0f);
	EXPECT_EQ(EngineStateMachineState::Transient, runAndGetState());
}

TEST(EngineStateMachine, idleState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	// Low RPM, closed throttle → Idle
	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	advanceTimeUs(static_cast<int>(engineConfiguration->smAfterStartDuration * 1e6f) + 100000);

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);
	// RPM well below smIdleExitRpm (1200); let hysteresis settle
	runAndGetState(); // seed m_prevTps, hysteresis starts false (idle side)

	EXPECT_EQ(EngineStateMachineState::Idle, runAndGetState());
}

TEST(EngineStateMachine, coastingState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	// High RPM, closed throttle → Coasting
	engine->rpmCalculator.setRpmValue(2000.0f); // well above smIdleExitRpm=1200
	advanceTimeUs(static_cast<int>(engineConfiguration->smAfterStartDuration * 1e6f) + 100000);

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);
	runAndGetState(); // seed

	EXPECT_EQ(EngineStateMachineState::Coasting, runAndGetState());
}

TEST(EngineStateMachine, idleHysteresis) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	// smIdleExitRpm=1200, smIdleRpmHysteresis=50
	// Rising threshold (idle→coasting): 1250 RPM
	// Falling threshold (coasting→idle): 1150 RPM

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	advanceTimeUs(static_cast<int>(engineConfiguration->smAfterStartDuration * 1e6f) + 100000);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);

	// Start clearly in idle (RPM below 1150)
	engine->rpmCalculator.setRpmValue(1000.0f);
	runAndGetState(); // seed hysteresis
	EXPECT_EQ(EngineStateMachineState::Idle, runAndGetState());

	// Rise into band (1200) — hysteresis should hold Idle
	engine->rpmCalculator.setRpmValue(1200.0f);
	EXPECT_EQ(EngineStateMachineState::Idle, runAndGetState());

	// Rise above rising threshold (1250+) → transitions to Coasting
	engine->rpmCalculator.setRpmValue(1300.0f);
	EXPECT_EQ(EngineStateMachineState::Coasting, runAndGetState());

	// Drop back into band (1200) — hysteresis should hold Coasting
	engine->rpmCalculator.setRpmValue(1200.0f);
	EXPECT_EQ(EngineStateMachineState::Coasting, runAndGetState());

	// Drop below falling threshold (1150) → transitions back to Idle
	engine->rpmCalculator.setRpmValue(1100.0f);
	EXPECT_EQ(EngineStateMachineState::Idle, runAndGetState());
}

TEST(EngineStateMachine, cruisingDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	advanceTimeUs(static_cast<int>(engineConfiguration->smAfterStartDuration * 1e6f) + 100000);

	// Mid throttle, RPM above idle, no transient → Cruising
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	runAndGetState(); // seed m_prevTps so delta = 0

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	EXPECT_EQ(EngineStateMachineState::Cruising, runAndGetState());
}

// ---- Helper: put the engine into a stable running state past afterstart ----
static void enterRunning(float rpmVal = 2000.0f, float tpsVal = 50.0f) {
	engine->rpmCalculator.setRpmValue(rpmVal);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tpsVal);
	advanceTimeUs(static_cast<int>(engineConfiguration->smAfterStartDuration * 1e6f) + 100000);
	// Seed m_prevTps so rate-of-change starts at zero
	engine->periodicSlowCallback();
}

// ---- Flat-shift torque reduction ----

TEST(EngineStateMachine, flatShiftSetsUpshiftAndTorqueReduction) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(3000.0f, 95.0f);

	// Simulate flat-shift torque reduction engaging
	engine->shiftTorqueReductionController.isFlatShiftConditionSatisfied = true;

	engine->periodicSlowCallback();
	auto& sm = getSm();

	EXPECT_TRUE(sm.engineSmIsUpshifting);
	EXPECT_TRUE(sm.engineSmIsTorqueReduction);
	EXPECT_FALSE(sm.engineSmIsDownshifting);
	// Primary display state should be overridden to Upshifting (10)
	EXPECT_EQ(10u, sm.engineSmCurrentState);
}

TEST(EngineStateMachine, flatShiftClearsWhenInactive) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(3000.0f, 95.0f);

	engine->shiftTorqueReductionController.isFlatShiftConditionSatisfied = true;
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	engine->shiftTorqueReductionController.isFlatShiftConditionSatisfied = false;
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsTorqueReduction);
}

// ---- Launch Control overlay ----

TEST(EngineStateMachine, launchControlOverlay) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(2000.0f, 50.0f);

	engine->launchController.isLaunchCondition = true;

	engine->periodicSlowCallback();
	auto& sm = getSm();
	EXPECT_TRUE(sm.engineSmIsLaunchControl);
	// Launch overrides primary display (9)
	EXPECT_EQ(9u, sm.engineSmCurrentState);
}

TEST(EngineStateMachine, launchControlDoesNotSetShiftBits) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(2000.0f, 50.0f);

	engine->launchController.isLaunchCondition = true;
	engine->periodicSlowCallback();
	auto& sm = getSm();

	EXPECT_FALSE(sm.engineSmIsUpshifting);
	EXPECT_FALSE(sm.engineSmIsDownshifting);
}

// ---- Overlay priority: Upshifting > Downshifting > LaunchControl ----

TEST(EngineStateMachine, overlayPriorityUpshiftBeatsLaunch) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(3000.0f, 95.0f);

	engine->shiftTorqueReductionController.isFlatShiftConditionSatisfied = true;
	engine->launchController.isLaunchCondition = true;

	engine->periodicSlowCallback();
	// Upshifting (10) takes priority over LaunchControl (9)
	EXPECT_EQ(10u, getSm().engineSmCurrentState);
}

// ---- Clutch-based shift detection ----

static void setupShiftDetectionConfig() {
	setupSmConfig();
	// Both directions mapped to Clutch Down switch, Simple Throttle mode
	engineConfiguration->smUpshiftClutchSwitch   = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smDownshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smShiftDetectionMode    = sm_shift_detection_mode_e::SimpleThrottle;
	engineConfiguration->smShiftLookbackMs       = 300;
}

// Seed the history buffer with several callbacks so getHistoryAt has data
static void seedHistory(int callbacks = 10, float rpm = 2000.0f, float tps = 50.0f) {
	engine->rpmCalculator.setRpmValue(rpm);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tps);
	for (int i = 0; i < callbacks; i++) {
		engine->periodicSlowCallback();
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	}
}

TEST(EngineStateMachine, upshiftDetectedSimpleThrottle) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig();
	enterRunning();

	// Seed history with WOT-level TPS (above idle threshold)
	seedHistory(10, 3000.0f, 80.0f);

	// Press clutch down (rising edge)
	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	auto& sm = getSm();
	// TPS history was above idle threshold → confirmed upshift
	EXPECT_TRUE(sm.engineSmIsUpshifting);
	EXPECT_FALSE(sm.engineSmIsDownshifting);
	EXPECT_EQ(10u, sm.engineSmCurrentState);
}

TEST(EngineStateMachine, downshiftDetectedSimpleThrottle) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig();
	enterRunning();

	// Seed history with closed-throttle TPS (below idle threshold)
	seedHistory(10, 2000.0f, 2.0f);

	// Press clutch down (rising edge)
	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	auto& sm = getSm();
	// TPS history was below idle threshold → confirmed downshift
	EXPECT_FALSE(sm.engineSmIsUpshifting);
	EXPECT_TRUE(sm.engineSmIsDownshifting);
	EXPECT_EQ(11u, sm.engineSmCurrentState);
}

TEST(EngineStateMachine, shiftClearedOnClutchRelease) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig();
	enterRunning();
	seedHistory(10, 3000.0f, 80.0f);

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	// Release clutch — window should close
	engine->engineState.lua.clutchDownState = false;
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

TEST(EngineStateMachine, shiftClearedOnTimeout) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig();
	enterRunning();
	seedHistory(10, 3000.0f, 80.0f);

	// Press clutch and hold for longer than SM_SHIFT_TIMEOUT_MS (3000ms)
	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	// Advance past the 3-second timeout
	advanceTimeUs(3100 * 1000);
	engine->periodicSlowCallback();

	EXPECT_FALSE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

TEST(EngineStateMachine, noShiftBitsWithoutClutchSwitch) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	// No clutch switch configured (default = None)
	engineConfiguration->smUpshiftClutchSwitch   = sm_clutch_switch_e::None;
	engineConfiguration->smDownshiftClutchSwitch = sm_clutch_switch_e::None;
	enterRunning();

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	EXPECT_FALSE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

TEST(EngineStateMachine, flatShiftOverridesClutchDetection) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig();
	enterRunning();
	seedHistory(10, 3000.0f, 2.0f); // History says downshift (low TPS)

	// Flat shift active — must win regardless of history
	engine->shiftTorqueReductionController.isFlatShiftConditionSatisfied = true;
	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	EXPECT_TRUE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
	EXPECT_TRUE(getSm().engineSmIsTorqueReduction);
}

TEST(EngineStateMachine, liveDataFieldsPopulated) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	engine->rpmCalculator.setRpmValue(0);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_TRUE(sm.engineSmEnabled);
	EXPECT_EQ(0, sm.engineSmCurrentState);  // Off = 0
	EXPECT_TRUE(sm.engineSmIsOff);
	EXPECT_FALSE(sm.engineSmIsCranking);
	EXPECT_FALSE(sm.engineSmIsWot);
}
