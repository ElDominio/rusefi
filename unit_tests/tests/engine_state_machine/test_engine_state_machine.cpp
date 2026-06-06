#include "pch.h"

#include "engine_state_machine.h"

// Default cranking RPM threshold used by TEST_ENGINE
static constexpr float TEST_CRANKING_RPM = 400.0f;
static constexpr float TEST_RUNNING_RPM  = 800.0f;

static void setupSmConfig() {
	engineConfiguration->useEngineStateMachine = true;
	engineConfiguration->smShiftTpsThreshold = 10;
	engineConfiguration->smWotTpsThreshold = 90;
	engineConfiguration->smTransientHoldoffCallbacks = 0; // no hold-off by default in tests
	// Lower AE threshold so tests can trigger Transient with small TPS steps on Tps1
	engineConfiguration->tpsAccelEnrichmentThreshold = 5.0f;
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

	MockIdleController mockIdle;
	engine->engineModules.get<IdleController>().set(&mockIdle);

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	// Seed stable state; Tps1 not mocked so AE stays quiet
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Running));
	runAndGetState();

	// Engine in crank-to-idle taper → SM reports Afterstart
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::CrankToIdleTaper));
	EXPECT_EQ(EngineStateMachineState::Afterstart, runAndGetState());

	// Taper complete → SM reports Cruising (throttle mid-range, above idle RPM)
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Running));
	EXPECT_EQ(EngineStateMachineState::Cruising, runAndGetState());
}

TEST(EngineStateMachine, wotState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);

	// WOT above threshold
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 95.0f);
	EXPECT_EQ(EngineStateMachineState::WOT, runAndGetState());
}

TEST(EngineStateMachine, wotPriorityOverTransient) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);

	// Seed AE buffer at low TPS so next jump triggers both WOT and Transient
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);
	Sensor::setMockValue(SensorType::Tps1, 0.0f);
	runAndGetState();

	// Big jump — WOT (DTI > 90%) and AE fires (Tps1 delta 95% > 5% threshold)
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 95.0f);
	Sensor::setMockValue(SensorType::Tps1, 95.0f);
	// WOT has higher priority than Transient
	EXPECT_EQ(EngineStateMachineState::WOT, runAndGetState());
}

TEST(EngineStateMachine, transientState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);

	// Seed AE buffer at 30% Tps1 so the buffer has a stable reference
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 30.0f);
	Sensor::setMockValue(SensorType::Tps1, 30.0f);
	runAndGetState();

	// Step Tps1 to 40% — delta = 10% > 5% threshold → AE isAboveAccelThreshold = true
	Sensor::setMockValue(SensorType::Tps1, 40.0f);
	EXPECT_EQ(EngineStateMachineState::Transient, runAndGetState());
}

TEST(EngineStateMachine, transientHoldoff) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	engineConfiguration->smTransientHoldoffCallbacks = 2; // hold Transient for 2 callbacks after AE drops

	// Use a 2-entry AE buffer so delta from the step scrolls out on the very next stable callback
	engine->module<TpsAccelEnrichment>()->setLength(2);

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);

	// Seed: 2 callbacks to fill the 2-entry buffer at stable 30%
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 30.0f);
	Sensor::setMockValue(SensorType::Tps1, 30.0f);
	runAndGetState();
	runAndGetState();

	// Trigger: step Tps1 to 40% → delta=10% > 5% threshold → AE fires → Transient
	Sensor::setMockValue(SensorType::Tps1, 40.0f);
	EXPECT_EQ(EngineStateMachineState::Transient, runAndGetState());

	// AE drops on next callback (buffer is [40,40], delta=0); hold-off starts
	EXPECT_EQ(EngineStateMachineState::Transient, runAndGetState()); // holdoff 2→1
	EXPECT_EQ(EngineStateMachineState::Transient, runAndGetState()); // holdoff 1→0
	// Hold-off expired — state falls through to Cruising
	EXPECT_NE(EngineStateMachineState::Transient, runAndGetState());
}

TEST(EngineStateMachine, idleState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	MockIdleController mockIdle;
	engine->engineModules.get<IdleController>().set(&mockIdle);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Idling));

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);

	EXPECT_EQ(EngineStateMachineState::Idle, runAndGetState());
}

TEST(EngineStateMachine, coastingState) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	// Keep RPM below the DFCO overrun threshold so the state stays Coasting, not Overrun
	engineConfiguration->coastingFuelCutRpmHigh = 4000;

	MockIdleController mockIdle;
	engine->engineModules.get<IdleController>().set(&mockIdle);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Coasting));

	engine->rpmCalculator.setRpmValue(2000.0f);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);

	EXPECT_EQ(EngineStateMachineState::Coasting, runAndGetState());
}

TEST(EngineStateMachine, overrunWhenDfcoConditionsMet) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	// Configure overrun (DFCO) detection thresholds
	engineConfiguration->coastingFuelCutTps = 5;        // TPS < 5% → overrun allowed
	engineConfiguration->coastingFuelCutRpmHigh = 2000; // RPM > 2000 → activate
	engineConfiguration->coastingFuelCutRpmLow  = 1500; // RPM < 1500 → deactivate
	engineConfiguration->coastingFuelCutVssHigh = 0;    // VSS always qualifies
	engineConfiguration->coastingFuelCutVssLow  = 0;
	engineConfiguration->coastingFuelCutMap = 0;        // MAP check disabled (no MAP sensor)

	MockIdleController mockIdle;
	engine->engineModules.get<IdleController>().set(&mockIdle);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Coasting));

	// Throttle closed, RPM high → isOverrun() true → Overrun state
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);
	engine->rpmCalculator.setRpmValue(3000.0f);
	EXPECT_EQ(EngineStateMachineState::Overrun, runAndGetState());

	// Throttle open → isOverrun() false → Coasting
	// Tps1 not mocked so AE stays quiet; seed a callback to stabilise state
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	runAndGetState();
	EXPECT_EQ(EngineStateMachineState::Coasting, runAndGetState());

	// Throttle closed, RPM too low → not overrun → Coasting
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);
	engine->rpmCalculator.setRpmValue(1000.0f);
	runAndGetState();
	EXPECT_EQ(EngineStateMachineState::Coasting, runAndGetState());
}

TEST(EngineStateMachine, cruisingDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	MockIdleController mockIdle;
	engine->engineModules.get<IdleController>().set(&mockIdle);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Running));

	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);

	// Mid throttle, part-throttle, IdleController in Running → Cruising
	// Tps1 not mocked → AE sees 0, no transient fires
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	runAndGetState();

	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	EXPECT_EQ(EngineStateMachineState::Cruising, runAndGetState());
}

// ---- Helper: put the engine into a stable running state ----
static void enterRunning(float rpmVal = 2000.0f, float tpsVal = 50.0f) {
	engine->rpmCalculator.setRpmValue(rpmVal);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tpsVal);
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

// ---- Limp Mode overlay ----

// Sync LimpManager transient state by running its fast-callback logic.
static void updateLimpManager() {
	getLimpManager()->updateState(engine->rpmCalculator.getCachedRpm(), getTimeNowNt());
}

TEST(EngineStateMachine, limpOverlayWhenFuelCut) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(2000.0f, 50.0f);

	// No cut → Limp bit clear, primary state shown
	updateLimpManager();
	engine->periodicSlowCallback();
	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsLimp);

	// Cut fuel via Lua → Limp bit set, display overrides to Limp (12)
	engine->engineState.lua.luaFuelCut = true;
	updateLimpManager();
	engine->periodicSlowCallback();
	EXPECT_TRUE(sm.engineSmIsLimp);
	EXPECT_EQ(static_cast<uint8_t>(EngineStateMachineState::Limp), sm.engineSmCurrentState);

	// Restore fuel → Limp clears
	engine->engineState.lua.luaFuelCut = false;
	updateLimpManager();
	engine->periodicSlowCallback();
	EXPECT_FALSE(sm.engineSmIsLimp);
}

TEST(EngineStateMachine, limpOverlayWhenIgnitionCut) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(2000.0f, 50.0f);

	engine->engineState.lua.luaIgnCut = true;
	updateLimpManager();
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_TRUE(sm.engineSmIsLimp);
	EXPECT_EQ(static_cast<uint8_t>(EngineStateMachineState::Limp), sm.engineSmCurrentState);
}

TEST(EngineStateMachine, limpPriorityOverUpshifting) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(3000.0f, 95.0f);

	engine->shiftTorqueReductionController.isFlatShiftConditionSatisfied = true;
	engine->engineState.lua.luaFuelCut = true;
	updateLimpManager();

	engine->periodicSlowCallback();
	// Limp (12) must beat Upshifting (10) in the display integer
	EXPECT_EQ(static_cast<uint8_t>(EngineStateMachineState::Limp), getSm().engineSmCurrentState);
}
