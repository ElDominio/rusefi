#include "pch.h"
#include "custom_page.h"

#include "engine_state_machine.h"
#include "dfco.h"

// Default cranking RPM threshold used by TEST_ENGINE
static constexpr float TEST_CRANKING_RPM = 400.0f;
static constexpr float TEST_RUNNING_RPM  = 800.0f;

static void setupSmConfig() {
	engineConfiguration->useEngineStateMachine = true;
	getCustomPage()->smWotTpsThreshold = 90;
	getCustomPage()->smTransientHoldoffCallbacks = 0; // no hold-off by default in tests
	// Lower AE threshold so tests can trigger Transient with small TPS steps on Tps1
	engineConfiguration->tpsAccelEnrichmentThreshold = 5.0f;
}

// Standard accumulator config used across direction-detection tests.
// At threshold (500 RPM/s or 5 km/h/s), gain=20 %/s → 1%/callback @50ms.
// Strong signals (4× threshold) build ±80 % in 20 callbacks; flat signal decays to 0.
static void setupAccumulatorConfig() {
	getCustomPage()->smAccelRateThreshold           = 500;
	getCustomPage()->smDecelRateThreshold           = 500;
	getCustomPage()->smAccumulatorGain              = 20.0f;
	getCustomPage()->smAccumulatorDecayRate         = 10.0f;
	getCustomPage()->smAccumulatorUpshiftThreshold  = 40;
	getCustomPage()->smAccumulatorDownshiftThreshold = 40;
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
	getCustomPage()->smTransientHoldoffCallbacks = 2; // hold Transient for 2 callbacks after AE drops

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

// ---- Traction control torque reduction ----

TEST(EngineStateMachine, tractionControlSetsTorqueReduction) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	// Configure a non-zero ETB drop across the whole traction-control table, then reload
	// the controller's Map3D tables from the freshly-written config.
	for (size_t s = 0; s < efi::size(engineConfiguration->tractionControlEtbDrop); s++) {
		for (size_t v = 0; v < efi::size(engineConfiguration->tractionControlEtbDrop[0]); v++) {
			engineConfiguration->tractionControlEtbDrop[s][v] = -10;
		}
	}
	engineConfiguration->tractionControlYAxisSource = TC_Y_AXIS_WHEEL_SLIP;
	engineConfiguration->tractionControlHoldTime = 0;
	engineConfiguration->tractionControlDecayTime = 0;
	engine->tractionController.init();

	enterRunning(3000.0f, 50.0f);

	// Feed slip / speed so the table returns its (non-zero) ETB drop
	Sensor::setMockValue(SensorType::WheelSlipRatio, 1.1f);
	Sensor::setMockValue(SensorType::VehicleSpeed, 40.0f);

	// Fast callback computes the applied traction-control corrections; slow callback runs the SM
	engine->periodicFastCallback();
	EXPECT_TRUE(engine->tractionController.isActive());

	engine->periodicSlowCallback();
	auto& sm = getSm();
	EXPECT_TRUE(sm.engineSmIsTorqueReduction);
	// Traction control is not a gear shift — it must not raise the shift-direction bits
	EXPECT_FALSE(sm.engineSmIsUpshifting);
	EXPECT_FALSE(sm.engineSmIsDownshifting);
}

TEST(EngineStateMachine, torqueReductionClearsWhenTractionInactive) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	// Leave the traction-control table at its default (all zero) → no correction applied
	engine->tractionController.init();
	enterRunning(3000.0f, 50.0f);

	Sensor::setMockValue(SensorType::WheelSlipRatio, 1.1f);
	Sensor::setMockValue(SensorType::VehicleSpeed, 40.0f);

	engine->periodicFastCallback();
	EXPECT_FALSE(engine->tractionController.isActive());

	engine->periodicSlowCallback();
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
	// Both directions mapped to Clutch Down switch, RPM Rate accumulator mode
	getCustomPage()->smUpshiftClutchSwitch   = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smDownshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smShiftDetectionMode    = sm_shift_detection_mode_e::RpmRate;
	setupAccumulatorConfig();
}

// Run N callbacks at constant RPM/TPS. With constant RPM the accumulator stays at 0
// (rate=0 → dead-band decay from 0). Used by single-direction tests where no
// accumulator build-up is required before pressing the clutch.
static void seedHistory(int callbacks = 10, float rpm = 2000.0f, float tps = 50.0f) {
	engine->rpmCalculator.setRpmValue(rpm);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tps);
	for (int i = 0; i < callbacks; i++) {
		engine->periodicSlowCallback();
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	}
}

// Run N callbacks with linearly ramping RPM. Used to pre-build the shift-direction accumulator
// before pressing the clutch. Rising RPM → positive accumulator (upshift); falling → negative.
static void seedRampedRpm(float startRpm, float rpmPerCallback, int callbacks, float tps) {
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tps);
	for (int i = 0; i < callbacks; i++) {
		engine->rpmCalculator.setRpmValue(startRpm + rpmPerCallback * i);
		engine->periodicSlowCallback();
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	}
}

TEST(EngineStateMachine, upshiftDetectedAccumulator) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig();
	enterRunning();

	// Rising RPM builds a positive accumulator → upshift confirmed.
	// 100 RPM/callback × 20 Hz = 2000 RPM/s, ratio = 4× → 4 %/callback → 80 % after 20 callbacks.
	seedRampedRpm(2000.0f, 100.0f, 20, 50.0f);

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_TRUE(sm.engineSmIsUpshifting);
	EXPECT_FALSE(sm.engineSmIsDownshifting);
	EXPECT_EQ(10u, sm.engineSmCurrentState);
}

TEST(EngineStateMachine, downshiftDetectedAccumulator) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig();
	// Anchor the RPM-rate calc at the ramp's actual starting RPM (4000) -- otherwise the first
	// quantized recompute would measure against enterRunning()'s default 2000 RPM anchor, a
	// same-tick discontinuity that doesn't occur with real (continuous) RPM.
	enterRunning(4000.0f, 0.0f);

	// Falling RPM builds a negative accumulator → downshift confirmed.
	// TPS=0% ensures tps < coastingFuelCutTps (default 2%) so SM reaches Overrun state.
	seedRampedRpm(4000.0f, -100.0f, 20, 0.0f);

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsUpshifting);
	EXPECT_TRUE(sm.engineSmIsDownshifting);
	EXPECT_EQ(11u, sm.engineSmCurrentState);
}

TEST(EngineStateMachine, shiftClearedOnClutchRelease) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig();
	enterRunning();
	seedRampedRpm(2000.0f, 100.0f, 20, 50.0f); // build upshift accumulator

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
	seedRampedRpm(2000.0f, 100.0f, 20, 50.0f); // build upshift accumulator

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

// ---- RPM Rate accumulator shift detection ----

static void setupRpmRateModeConfig() {
	setupSmConfig();
	getCustomPage()->smUpshiftClutchSwitch   = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smDownshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smShiftDetectionMode    = sm_shift_detection_mode_e::RpmRate;
	setupAccumulatorConfig(); // sets rate thresholds and accumulator params
}

// Rising RPM builds a positive accumulator → upshift confirmed.
// 2000 RPM/s at 500 RPM/s threshold → 4× → 4 %/callback → 80 % after 20 callbacks (threshold 40 %).
TEST(EngineStateMachine, upshiftDetectedRpmMode) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRpmRateModeConfig();
	enterRunning();

	seedRampedRpm(2000.0f, 100.0f, 20, 50.0f);

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_TRUE(sm.engineSmIsUpshifting);
	EXPECT_FALSE(sm.engineSmIsDownshifting);
}

// Falling RPM builds a negative accumulator → downshift confirmed.
TEST(EngineStateMachine, downshiftDetectedRpmMode) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRpmRateModeConfig();
	// Anchor the RPM-rate calc at the ramp's actual starting RPM (4000) -- see
	// downshiftDetectedAccumulator for why.
	enterRunning(4000.0f, 0.0f);

	// TPS=0% ensures tps < coastingFuelCutTps (default 2%) so SM reaches Overrun state.
	seedRampedRpm(4000.0f, -100.0f, 20, 0.0f);

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsUpshifting);
	EXPECT_TRUE(sm.engineSmIsDownshifting);
}

// Flat RPM → rate = 0 → dead-band decay → accumulator stays at 0 → neither direction confirmed.
TEST(EngineStateMachine, rpmModeFlatRpmNotConfirmed) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRpmRateModeConfig();
	enterRunning();

	seedHistory(20, 3000.0f, 50.0f);

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsUpshifting);
	EXPECT_FALSE(sm.engineSmIsDownshifting);
}

TEST(EngineStateMachine, noShiftBitsWithoutClutchSwitch) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	// No clutch switch configured (default = None)
	getCustomPage()->smUpshiftClutchSwitch   = sm_clutch_switch_e::None;
	getCustomPage()->smDownshiftClutchSwitch = sm_clutch_switch_e::None;
	enterRunning();

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	EXPECT_FALSE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

// ---- RPM/t configurable rate window ----

// For a linear RPM ramp, the rate is identical regardless of window length, because the rate
// calc always divides by the *actual* elapsed time between the two sampled points -- this is
// what lets smAccelRateThreshold/smDecelRateThreshold stay in RPM/s without re-tuning when the
// user changes the window.
TEST(EngineStateMachine, rpmRateWindowInvariantOnLinearRamp) {
	float shortWindowRate;
	float longWindowRate;
	{
		EngineTestHelper eth(engine_type_e::TEST_ENGINE);
		setupSmConfig();
		// The rate calc only runs once a shift-detection direction is configured -- see
		// updateShiftDetection()'s "no direction configured" early-out.
		getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
		getCustomPage()->smRpmRateWindowMs = 50; // one tick
		enterRunning();

		// 100 RPM/tick @ 50 ms/tick == 2000 RPM/s.
		seedRampedRpm(2000.0f, 100.0f, 10, 50.0f);
		shortWindowRate = getSm().getLastRpmRate();
	}
	{
		EngineTestHelper eth(engine_type_e::TEST_ENGINE);
		setupSmConfig();
		getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
		getCustomPage()->smRpmRateWindowMs = 500; // ten ticks
		enterRunning();

		seedRampedRpm(2000.0f, 100.0f, 12, 50.0f); // extra ticks so a full window elapses
		longWindowRate = getSm().getLastRpmRate();
	}

	EXPECT_NEAR(2000.0f, shortWindowRate, 5.0f);
	EXPECT_NEAR(2000.0f, longWindowRate, 5.0f);
}

// The rate value now only recomputes once smRpmRateWindowMs has actually elapsed since the last
// recompute -- holding the previous value on every tick in between -- rather than recomputing on
// every 50 ms tick regardless of the configured window. This is what makes the number visibly
// update at the rate the user configures (in multiples of the 50 ms tick), instead of jittering
// every tick no matter what window is set.
TEST(EngineStateMachine, rpmRateHoldsUntilWindowElapsesThenUpdates) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smRpmRateWindowMs = 200; // four 50 ms ticks

	// First-ever sample: no reference yet, rate reads 0 and anchors here.
	engine->rpmCalculator.setRpmValue(2000.0f);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	engine->periodicSlowCallback();
	EXPECT_EQ(0.0f, getSm().getLastRpmRate());
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	// RPM jumps immediately, but the window (200 ms) hasn't elapsed since the anchor yet on any
	// of the next three ticks (50/100/150 ms in) -- the rate must hold at the old value.
	engine->rpmCalculator.setRpmValue(2500.0f);
	for (int i = 0; i < 3; i++) {
		engine->periodicSlowCallback();
		EXPECT_EQ(0.0f, getSm().getLastRpmRate());
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	}

	// The fourth tick lands exactly on the 200 ms window boundary -- the rate now recomputes
	// over the full window, picking up the jump.
	engine->periodicSlowCallback();
	EXPECT_NEAR(2500.0f, getSm().getLastRpmRate(), 1.0f); // (2500-2000)/0.2s
}

// A shorter window recomputes (and thus can respond to an RPM change) more often than a longer
// one -- directly exercising that smRpmRateWindowMs controls the recompute cadence.
TEST(EngineStateMachine, shorterWindowRecomputesMoreOften) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smRpmRateWindowMs = 100; // two 50 ms ticks

	engine->rpmCalculator.setRpmValue(2000.0f);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	engine->periodicSlowCallback(); // anchor
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	engine->rpmCalculator.setRpmValue(2100.0f);
	engine->periodicSlowCallback(); // 50 ms since anchor -- window not elapsed yet, holds
	EXPECT_EQ(0.0f, getSm().getLastRpmRate());
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	engine->periodicSlowCallback(); // 100 ms since anchor -- window elapsed, recomputes
	EXPECT_NEAR(1000.0f, getSm().getLastRpmRate(), 1.0f); // (2100-2000)/0.1s
}

// smRpmRateWindowMs == 0 (e.g. an old tune saved before this field existed) must floor to one
// slow-callback tick and behave identically to an explicit 50 ms window -- calibration
// compatibility for existing tunes.
TEST(EngineStateMachine, rpmRateWindowZeroMatchesOneTickWindow) {
	float zeroWindowRate;
	float explicitOneTickRate;
	{
		EngineTestHelper eth(engine_type_e::TEST_ENGINE);
		setupSmConfig();
		getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
		getCustomPage()->smRpmRateWindowMs = 0;
		enterRunning(2000.0f, 50.0f);
		seedHistory(6, 2000.0f, 50.0f);
		engine->rpmCalculator.setRpmValue(2500.0f);
		engine->periodicSlowCallback();
		zeroWindowRate = getSm().getLastRpmRate();
	}
	{
		EngineTestHelper eth(engine_type_e::TEST_ENGINE);
		setupSmConfig();
		getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
		getCustomPage()->smRpmRateWindowMs = 50;
		enterRunning(2000.0f, 50.0f);
		seedHistory(6, 2000.0f, 50.0f);
		engine->rpmCalculator.setRpmValue(2500.0f);
		engine->periodicSlowCallback();
		explicitOneTickRate = getSm().getLastRpmRate();
	}
	EXPECT_NEAR(explicitOneTickRate, zeroWindowRate, 0.01f);
}

// No reference sample yet on the very first slow callback -- rate must read 0, not garbage.
TEST(EngineStateMachine, rpmRateZeroBeforeEnoughHistory) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smRpmRateWindowMs = 500;

	engine->rpmCalculator.setRpmValue(2000.0f);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	engine->periodicSlowCallback(); // first-ever sample, no reference yet

	EXPECT_EQ(0.0f, getSm().getLastRpmRate());
}

// The RPM-rate value now has a consumer beyond the accumulator: the Accelerating state itself,
// gated directly on smAccelRateThreshold against the windowed rate.
TEST(EngineStateMachine, acceleratingStateEnteredOnConfiguredRate) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smAccelRateThreshold = 500; // RPM/s
	getCustomPage()->smRpmRateWindowMs = 100;
	enterRunning(2000.0f, 50.0f);

	// 100 RPM/tick @ 50 ms/tick == 2000 RPM/s, comfortably above the 500 RPM/s threshold.
	seedRampedRpm(2000.0f, 100.0f, 5, 50.0f);
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_TRUE(sm.engineSmIsAccelerating);
	EXPECT_EQ(14u, sm.engineSmCurrentState);
}

// ---- Accelerating hold (smAccelHoldMs) ----
//
// determineState() consumes m_lastRpmRate one slow-callback tick after it's computed (see the
// comment on m_lastRpmRate / recordRpmSampleAndComputeRate), so with smRpmRateWindowMs floored to
// one tick, a single RPM step becomes visible to determineState() only on the *following* tick.
// The helpers below spell out each tick's real time and rate explicitly to keep that lag honest.

// Once the rate threshold is met, Accelerating must keep reporting for smAccelHoldMs after the
// rate drops back below threshold, then fall through once the hold expires.
TEST(EngineStateMachine, acceleratingHoldsAfterRateDrops) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	// m_lastRpmRate is only ever computed inside updateShiftAccumulator(), which
	// updateShiftDetection() skips entirely unless a clutch switch is configured -- so a switch
	// must be wired even though these tests don't exercise shift detection itself.
	getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smAccelRateThreshold = 500; // RPM/s
	getCustomPage()->smRpmRateWindowMs = 50;     // recompute every tick (one slow-callback tick)
	getCustomPage()->smAccelHoldMs = 120;

	engine->rpmCalculator.setRpmValue(2000.0f);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	engine->periodicSlowCallback(); // t=0: establishes the rate anchor, no rate yet
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	// 500 RPM over one 50 ms tick == 10,000 RPM/s, comfortably above threshold. Not yet visible
	// to determineState() this same tick (one-tick lag).
	engine->rpmCalculator.setRpmValue(2500.0f);
	EXPECT_NE(EngineStateMachineState::Accelerating, runAndGetState()); // t=50 ms
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	// RPM goes flat from here. The 10,000 RPM/s computed last tick is now visible -- Accelerating
	// triggers and the hold timer resets, even though the rate itself is about to read 0.
	EXPECT_EQ(EngineStateMachineState::Accelerating, runAndGetState()); // t=100 ms: trigger
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	// Rate has now recomputed to 0 (flat RPM) -- below threshold -- but the hold keeps it latched.
	EXPECT_EQ(EngineStateMachineState::Accelerating, runAndGetState()); // t=150 ms: 50 ms into hold
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	EXPECT_EQ(EngineStateMachineState::Accelerating, runAndGetState()); // t=200 ms: 100 ms into hold

	// Past smAccelHoldMs (120 ms since the t=100 ms trigger) -- hold expires, falls through.
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	EXPECT_EQ(EngineStateMachineState::Cruising, runAndGetState()); // t=250 ms: 150 ms, expired
}

// A fresh threshold crossing during the hold window must reset it back to the full duration,
// not just let the original window run out underneath it.
TEST(EngineStateMachine, acceleratingHoldResetsOnReTrigger) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	// m_lastRpmRate is only ever computed inside updateShiftAccumulator(), which
	// updateShiftDetection() skips entirely unless a clutch switch is configured -- so a switch
	// must be wired even though these tests don't exercise shift detection itself.
	getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smAccelRateThreshold = 500; // RPM/s
	getCustomPage()->smRpmRateWindowMs = 50;
	getCustomPage()->smAccelHoldMs = 120;

	engine->rpmCalculator.setRpmValue(2000.0f);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	engine->periodicSlowCallback(); // t=0
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	engine->rpmCalculator.setRpmValue(2500.0f);
	engine->periodicSlowCallback(); // t=50 ms: rate computed, not yet visible
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	EXPECT_EQ(EngineStateMachineState::Accelerating, runAndGetState()); // t=100 ms: trigger #1
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	// Still within trigger #1's hold. Jump RPM again -- this re-trigger's rate isn't visible yet
	// (one-tick lag), so this tick is still riding out the *original* hold.
	engine->rpmCalculator.setRpmValue(3000.0f);
	EXPECT_EQ(EngineStateMachineState::Accelerating, runAndGetState()); // t=150 ms
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	// The re-trigger's rate (10,000 RPM/s) is now visible -- Accelerating re-triggers and the
	// hold timer resets to t=200 ms.
	EXPECT_EQ(EngineStateMachineState::Accelerating, runAndGetState()); // t=200 ms: re-trigger
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	// t=250/300 ms: 50/100 ms since the re-trigger -- still held. Without the reset, the
	// *original* trigger's 120 ms hold (from t=100 ms) would already have expired by t=220 ms.
	EXPECT_EQ(EngineStateMachineState::Accelerating, runAndGetState()); // t=250 ms
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	EXPECT_EQ(EngineStateMachineState::Accelerating, runAndGetState()); // t=300 ms
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	// Past the reset hold (150 ms since the t=200 ms re-trigger) -- now falls through.
	EXPECT_EQ(EngineStateMachineState::Cruising, runAndGetState()); // t=350 ms
}

// A lift into Coasting/Overrun must break the Accelerating hold immediately -- a closed throttle
// physically contradicts a stale "still accelerating" claim, and Overrun/DFCO must never be
// masked by a latch that was still holding from an earlier tip-in.
TEST(EngineStateMachine, overrunBreaksAcceleratingHoldImmediately) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	// m_lastRpmRate is only ever computed inside updateShiftAccumulator(), which
	// updateShiftDetection() skips entirely unless a clutch switch is configured -- so a switch
	// must be wired even though these tests don't exercise shift detection itself.
	getCustomPage()->smUpshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smAccelRateThreshold = 500; // RPM/s
	getCustomPage()->smRpmRateWindowMs = 50;
	getCustomPage()->smAccelHoldMs = 200; // long hold relative to the steps below

	MockIdleController mockIdle;
	engine->engineModules.get<IdleController>().set(&mockIdle);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Running));

	engine->rpmCalculator.setRpmValue(2000.0f);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 50.0f);
	engine->periodicSlowCallback(); // t=0
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	engine->rpmCalculator.setRpmValue(2500.0f);
	engine->periodicSlowCallback(); // t=50 ms: rate computed, not yet visible
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	EXPECT_EQ(EngineStateMachineState::Accelerating, runAndGetState()); // t=100 ms: trigger
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);

	// Lift off: throttle closes and the idle controller reports Coasting. Only 50 ms have
	// elapsed since the trigger, deep inside the 200 ms hold -- but Coasting must override it
	// immediately rather than waiting out the hold.
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Coasting));
	engineConfiguration->coastingFuelCutRpmHigh = 9000; // keep this plain Coasting, not Overrun
	EXPECT_EQ(EngineStateMachineState::Coasting, runAndGetState()); // t=150 ms
}

// ---- Clutch-Up-only shift detection (single switch, no Clutch-Down wired) ----

static void setupClutchUpOnlyConfig() {
	setupSmConfig();
	getCustomPage()->smUpshiftClutchSwitch         = sm_clutch_switch_e::ClutchUp;
	getCustomPage()->smDownshiftClutchSwitch       = sm_clutch_switch_e::None;
	getCustomPage()->smShiftDetectionMode          = sm_shift_detection_mode_e::RpmRate;
	getCustomPage()->smSecondClutchSwitchAvailable = false;
	setupAccumulatorConfig();
}

// The pedal leaving rest (falling edge) is the moment the drivetrain is still locked, which is
// when the window must open. This is the edge that was previously missed entirely.
TEST(EngineStateMachine, clutchUpOnlyOpensOnFallingEdge) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupClutchUpOnlyConfig();
	enterRunning();

	// Pedal at rest while history is seeded.
	engine->engineState.lua.clutchUpState = true;
	seedHistory(10, 3000.0f, 80.0f);

	// Pedal leaves rest — window should open and confirm immediately (no second direction to
	// disambiguate, zero delay).
	engine->engineState.lua.clutchUpState = false;
	engine->periodicSlowCallback();

	EXPECT_TRUE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

// Regression guard for the original bug: the pedal returning to rest (rising edge) must NOT by
// itself open a new shift window.
TEST(EngineStateMachine, clutchUpOnlyDoesNotOpenOnRisingEdge) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupClutchUpOnlyConfig();
	enterRunning();

	// Pedal already pressed (not at rest) while history is seeded.
	engine->engineState.lua.clutchUpState = false;
	seedHistory(10, 3000.0f, 80.0f);

	// Pedal returns to rest — must not open the window.
	engine->engineState.lua.clutchUpState = true;
	engine->periodicSlowCallback();

	EXPECT_FALSE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

TEST(EngineStateMachine, clutchUpOnlyClosesOnRisingEdge) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupClutchUpOnlyConfig();
	enterRunning();

	engine->engineState.lua.clutchUpState = true;
	seedHistory(10, 3000.0f, 80.0f);

	engine->engineState.lua.clutchUpState = false; // falling edge: window opens
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	engine->engineState.lua.clutchUpState = true; // rising edge: clutch re-engaged, window closes
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

// ---- smSecondClutchSwitchAvailable: using the complementary switch to refine timing ----

// Shared-switch configuration (both directions keyed to Clutch Down, accumulator-disambiguated)
// but a Clutch-Up switch is also physically wired. The window should open on the earlier Clutch-Up
// falling edge instead of waiting for the (later) Clutch-Down rising edge.
TEST(EngineStateMachine, secondSwitchOpensEarlyOnComplementaryFallingEdge) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig(); // up & down both ClutchDown
	getCustomPage()->smSecondClutchSwitchAvailable = true;
	enterRunning();

	engine->engineState.lua.clutchUpState   = true;
	engine->engineState.lua.clutchDownState = false;
	// Build upshift accumulator so direction can be confirmed when the window opens.
	seedRampedRpm(2000.0f, 100.0f, 20, 50.0f);

	// Pedal leaves rest (Clutch-Up falling) well before reaching full press (Clutch-Down still
	// false) — the window must open here, not wait for the Down switch.
	engine->engineState.lua.clutchUpState = false;
	engine->periodicSlowCallback();

	EXPECT_TRUE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

static void setupSecondSwitchUpPrimaryConfig() {
	setupSmConfig();
	getCustomPage()->smUpshiftClutchSwitch   = sm_clutch_switch_e::ClutchUp;
	getCustomPage()->smDownshiftClutchSwitch = sm_clutch_switch_e::None;
	getCustomPage()->smShiftDetectionMode    = sm_shift_detection_mode_e::RpmRate;
	setupAccumulatorConfig();
}

// With a Clutch-Down switch also wired, its falling edge (release starting) should close the
// window earlier than waiting for Clutch-Up to fully return to rest.
TEST(EngineStateMachine, secondSwitchClosesEarlyOnComplementaryFallingEdge) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSecondSwitchUpPrimaryConfig();
	getCustomPage()->smSecondClutchSwitchAvailable = true;
	enterRunning();

	engine->engineState.lua.clutchUpState   = true;
	engine->engineState.lua.clutchDownState = false;
	seedHistory(10, 3000.0f, 80.0f);

	engine->engineState.lua.clutchUpState = false; // press begins: window opens
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	engine->engineState.lua.clutchDownState = true; // full press reached
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting) << "still mid-shift, no close edge yet";

	// Release starts (Clutch-Down falling) while Clutch-Up has NOT yet returned to rest.
	engine->engineState.lua.clutchDownState = false;
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

// Without the second-switch flag, the same Clutch-Down falling edge must NOT close the window —
// only Clutch-Up returning to rest (or the timeout) may close it.
TEST(EngineStateMachine, withoutSecondSwitchFlagComplementaryEdgeDoesNotCloseEarly) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSecondSwitchUpPrimaryConfig();
	getCustomPage()->smSecondClutchSwitchAvailable = false;
	enterRunning();

	engine->engineState.lua.clutchUpState   = true;
	engine->engineState.lua.clutchDownState = false;
	seedHistory(10, 3000.0f, 80.0f);

	engine->engineState.lua.clutchUpState = false; // press begins: window opens
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	engine->engineState.lua.clutchDownState = true; // full press reached
	engine->periodicSlowCallback();

	// Release starts (Clutch-Down falling) — must NOT close without the second-switch flag.
	engine->engineState.lua.clutchDownState = false;
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting) << "must stay open until Clutch-Up itself returns to rest";

	// Clutch-Up finally returns to rest — now it closes.
	engine->engineState.lua.clutchUpState = true;
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
}

TEST(EngineStateMachine, flatShiftOverridesClutchDetection) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig();
	enterRunning();
	seedHistory(10, 3000.0f, 2.0f); // accumulator stays 0 (flat RPM) — no direction confirmed

	// Flat shift active — must win regardless of accumulator state
	engine->shiftTorqueReductionController.isFlatShiftConditionSatisfied = true;
	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	EXPECT_TRUE(getSm().engineSmIsUpshifting);
	EXPECT_FALSE(getSm().engineSmIsDownshifting);
	EXPECT_TRUE(getSm().engineSmIsTorqueReduction);
}

// ---- VSS Rate mode: minimum-speed gate (smShiftMinVss) ----

static void setupVssRateModeConfig() {
	setupSmConfig();
	getCustomPage()->smUpshiftClutchSwitch    = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smDownshiftClutchSwitch  = sm_clutch_switch_e::ClutchDown;
	getCustomPage()->smShiftDetectionMode     = sm_shift_detection_mode_e::VssRate;
	getCustomPage()->smAccelRateThreshold     = 5;  // km/h per second
	getCustomPage()->smDecelRateThreshold     = 5;
	getCustomPage()->smAccumulatorGain              = 20.0f;
	getCustomPage()->smAccumulatorDecayRate         = 10.0f;
	getCustomPage()->smAccumulatorUpshiftThreshold  = 40;
	getCustomPage()->smAccumulatorDownshiftThreshold = 40;
}

// Seed the history buffer with linearly ramping VSS, mirroring seedRampedRpm.
static void seedRampedVss(float startVss, float vssPerCallback, int callbacks, float tps) {
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tps);
	for (int i = 0; i < callbacks; i++) {
		Sensor::setMockValue(SensorType::VehicleSpeed, startVss + vssPerCallback * i);
		engine->periodicSlowCallback();
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	}
}

// Below smShiftMinVss the accumulator decays instead of accumulating, so the confirmation
// threshold is never reached even when the VSS rate would otherwise qualify.
TEST(EngineStateMachine, vssRateModeBlockedBelowMinVss) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupVssRateModeConfig();
	getCustomPage()->smShiftMinVss = 15; // require at least 15 km/h
	enterRunning();

	// Speed stays below 15 km/h throughout (0.5 to 10.0 km/h) — accumulator decays the whole time.
	seedRampedVss(0.5f, 0.5f, 20, 80.0f);

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsUpshifting);
	EXPECT_FALSE(sm.engineSmIsDownshifting);
}

// With smShiftMinVss = 0 the accumulator builds on every callback.
// 10 km/h/s at 5 km/h/s threshold → 2× → ~2 %/callback → ~50 % after 25 callbacks (threshold 40 %).
TEST(EngineStateMachine, vssRateModeAllowedAboveMinVss) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupVssRateModeConfig();
	getCustomPage()->smShiftMinVss = 0;
	enterRunning();

	// Start at 0.5 km/h so the first callback already has a non-zero rate vs. the initial 0.
	// Use 25 callbacks to land at ~50% — well above the 40% threshold despite float rounding.
	seedRampedVss(0.5f, 0.5f, 25, 80.0f);

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_TRUE(sm.engineSmIsUpshifting);
	EXPECT_FALSE(sm.engineSmIsDownshifting);
}

// ---- Shift latch (smShiftLatchTimeMs): masks rapid on/off flicker ----

TEST(EngineStateMachine, latchDefaultIsZeroAndClearsInstantly) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupClutchUpOnlyConfig(); // smShiftLatchTimeMs left at its 0 default
	enterRunning();

	engine->engineState.lua.clutchUpState = true;
	seedHistory(10, 3000.0f, 80.0f);

	engine->engineState.lua.clutchUpState = false; // window opens and confirms immediately
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	engine->engineState.lua.clutchUpState = true; // window closes
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting)
		<< "0ms latch must behave exactly like the old instantaneous signal";
}

// Clutch-Up returning to rest must force the overlay off immediately, overriding any still-active
// latch. The blipper/RPM-hold modules trust this flag with no clutch check of their own, so the
// latch may never outlive the pedal actually being back up.
TEST(EngineStateMachine, clutchUpForceOffOverridesActiveLatch) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupClutchUpOnlyConfig();
	getCustomPage()->smShiftLatchTimeMs = 200;
	enterRunning();

	engine->engineState.lua.clutchUpState = true;
	seedHistory(10, 3000.0f, 80.0f);

	engine->engineState.lua.clutchUpState = false; // window opens, confirms, latch armed for 200ms
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	engine->engineState.lua.clutchUpState = true; // pedal back at rest: force-off wins despite the latch
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting) << "Clutch-Up true must force the overlay off even with an active latch";
	EXPECT_FALSE(getSm().engineSmIsDownshifting);

	// Stays cleared rather than reappearing on a later tick.
	advanceTimeUs(50 * 1000);
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting);
}

// A later Clutch-Down full-press must not resurrect an overlay already killed by Clutch-Up — the
// force-off clears the latch outright, so relatch-on-clutch-down has nothing left to extend.
TEST(EngineStateMachine, clutchUpForceOffSurvivesSubsequentRelatchAttempt) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupClutchUpOnlyConfig();
	getCustomPage()->smSecondClutchSwitchAvailable = true;
	getCustomPage()->smShiftLatchTimeMs = 100;
	getCustomPage()->smRelatchOnClutchDown = true;
	enterRunning();

	engine->engineState.lua.clutchUpState   = true;
	engine->engineState.lua.clutchDownState = false;
	seedHistory(10, 3000.0f, 80.0f);

	engine->engineState.lua.clutchUpState = false; // window opens, confirms, latch until +100ms
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	engine->engineState.lua.clutchUpState = true; // pedal back at rest: force-off wins, latch destroyed
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting);

	advanceTimeUs(10 * 1000);
	engine->engineState.lua.clutchDownState = true; // fresh full-press
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting)
		<< "relatch-on-clutch-down must not be able to resurrect an overlay already killed by Clutch-Up";
}

// ---- smRelatchOnClutchDown: a fresh Clutch-Down press refreshes an active latch ----

static void setupRelatchConfig() {
	setupClutchUpOnlyConfig();
	getCustomPage()->smSecondClutchSwitchAvailable = true;
	getCustomPage()->smShiftLatchTimeMs = 100;
}

TEST(EngineStateMachine, repeatedConfirmationDoesNotExtendLatchWithoutRelatch) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRelatchConfig();
	getCustomPage()->smRelatchOnClutchDown = false;
	enterRunning();

	engine->engineState.lua.clutchUpState   = true;
	engine->engineState.lua.clutchDownState = false;
	seedHistory(10, 3000.0f, 80.0f);

	engine->engineState.lua.clutchUpState = false; // window opens, confirms, latch until +100ms
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	engine->engineState.lua.clutchDownState = true; // full press, well short of Clutch-Up returning to rest
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	// Release starts (Clutch-Down falling): earlyClose shuts the window early. Clutch-Up is still
	// false (pedal not yet at rest), so the latch — not the Clutch-Up force-off — covers us here.
	engine->engineState.lua.clutchDownState = false;
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	// 90ms later, a fresh Clutch-Down full-press reconfirms (opens a new window, same direction)
	// but must NOT push the original 100ms deadline back out.
	advanceTimeUs(90 * 1000);
	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	// 15ms later (105ms after the original confirmation) the original deadline has passed.
	advanceTimeUs(15 * 1000);
	engine->engineState.lua.clutchDownState = false;
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsUpshifting)
		<< "without smRelatchOnClutchDown, the latch must not be extended by a later confirmation";
}

TEST(EngineStateMachine, relatchOnClutchDownExtendsLatch) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupRelatchConfig();
	getCustomPage()->smRelatchOnClutchDown = true;
	enterRunning();

	engine->engineState.lua.clutchUpState   = true;
	engine->engineState.lua.clutchDownState = false;
	seedHistory(10, 3000.0f, 80.0f);

	engine->engineState.lua.clutchUpState = false; // window opens, confirms, latch until +100ms
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	engine->engineState.lua.clutchDownState = true; // full press, well short of Clutch-Up returning to rest
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	// Release starts (Clutch-Down falling): earlyClose shuts the window early. Clutch-Up is still
	// false (pedal not yet at rest), so the latch — not the Clutch-Up force-off — covers us here.
	engine->engineState.lua.clutchDownState = false;
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	// 90ms later, a fresh Clutch-Down full-press relatches: deadline becomes (now + 100ms).
	advanceTimeUs(90 * 1000);
	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting);

	// 15ms later (105ms after the original confirmation) — past the ORIGINAL deadline, but the
	// relatch pushed it out to 190ms, so the overlay must still be asserted. Clutch-Up stays false
	// throughout, so the force-off override never engages here.
	advanceTimeUs(15 * 1000);
	engine->engineState.lua.clutchDownState = false;
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsUpshifting) << "smRelatchOnClutchDown should have pushed the deadline out";
}

// ---- Shifting states block the overrun (DFCO) fuel cut ----

TEST(EngineStateMachine, shiftingBlocksOverrunFuelCut) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupShiftDetectionConfig(); // up & down both ClutchDown, RPM accumulator
	// Anchor the RPM-rate calc at the ramp's actual starting RPM (4000) -- see
	// downshiftDetectedAccumulator for why.
	enterRunning(4000.0f, 2.0f);

	engineConfiguration->coastingFuelCutEnabled = true;
	engineConfiguration->coastingFuelCutTps     = 5;
	engineConfiguration->coastingFuelCutRpmHigh = 1500;
	engineConfiguration->coastingFuelCutVssHigh = 0;
	engineConfiguration->coastingFuelCutClt     = 40.0f;
	Sensor::setMockValue(SensorType::Clt, 80.0f);

	// Falling RPM + closed throttle → downshift accumulator builds, DFCO conditions met.
	// End RPM = 4000 - 100*20 = 2000 > 1500 (coastingFuelCutRpmHigh), TPS=2% < 5%.
	seedRampedRpm(4000.0f, -100.0f, 20, 2.0f);

	engine->engineState.lua.clutchDownState = true;
	engine->periodicSlowCallback();
	engine->periodicFastCallback(); // runs DfcoController::update()

	EXPECT_TRUE(getSm().engineSmIsDownshifting);
	EXPECT_FALSE(eth.engine.module<DfcoController>().unmock().cutFuel())
		<< "overrun fuel cut must be blocked while a shift is in progress";
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

// Limp mode is a dedicated latch (reportLimpCondition), NOT derived from generic cuts.
TEST(EngineStateMachine, limpLatchOverlay) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(2000.0f, 50.0f);

	// No latch → Limp bit clear, primary state shown
	engine->periodicSlowCallback();
	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsLimp);

	// Latch limp → Limp bit set, display overrides to Limp (12)
	sm.reportLimpCondition();
	engine->periodicSlowCallback();
	EXPECT_TRUE(sm.engineSmIsLimp);
	EXPECT_EQ(static_cast<uint8_t>(EngineStateMachineState::Limp), sm.engineSmCurrentState);

	// Latch is sticky — stays set on subsequent callbacks (until power cycle)
	engine->periodicSlowCallback();
	EXPECT_TRUE(sm.engineSmIsLimp);
}

// A normal fuel/ignition cut (e.g. the rev limit or a Lua cut) must NOT enter limp mode.
TEST(EngineStateMachine, ordinaryCutDoesNotEnterLimp) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(2000.0f, 50.0f);

	engine->engineState.lua.luaFuelCut = true;
	engine->engineState.lua.luaIgnCut = true;
	updateLimpManager();
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsLimp);
}

// An ETB jam latches limp mode through the state machine when the SM is enabled.
TEST(EngineStateMachine, etbJamLatchesLimp) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning(2000.0f, 50.0f);

	getLimpManager()->reportEtbJammed();
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
	getSm().reportLimpCondition();

	engine->periodicSlowCallback();
	// Limp (12) must beat Upshifting (10) in the display integer
	EXPECT_EQ(static_cast<uint8_t>(EngineStateMachineState::Limp), getSm().engineSmCurrentState);
}

// ---- Sport Mode ----

TEST(EngineStateMachine, sportModeOffByDefault) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning();

	getCustomPage()->smSportModeActivationMode = SPORT_MODE_OFF;
	engine->periodicSlowCallback();

	EXPECT_FALSE(getSm().engineSmIsSportMode);
}

TEST(EngineStateMachine, sportModeLuaGaugeActivation) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning();

	getCustomPage()->smSportModeActivationMode = SPORT_MODE_LUA_GAUGE;
	getCustomPage()->smSportModeLuaGauge = LUA_GAUGE_1;
	getCustomPage()->smSportModeLuaGaugeMeaning = LUA_GAUGE_LOWER_BOUND; // assert when value >= threshold
	getCustomPage()->smSportModeLuaGaugeThreshold = 0.5f;

	Sensor::setMockValue(SensorType::LuaGauge1, 0.0f);
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsSportMode);

	Sensor::setMockValue(SensorType::LuaGauge1, 1.0f);
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsSportMode);
}

TEST(EngineStateMachine, limpOutVotesSportMode) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning();

	getCustomPage()->smSportModeActivationMode = SPORT_MODE_LUA_GAUGE;
	getCustomPage()->smSportModeLuaGauge = LUA_GAUGE_1;
	getCustomPage()->smSportModeLuaGaugeMeaning = LUA_GAUGE_LOWER_BOUND;
	getCustomPage()->smSportModeLuaGaugeThreshold = 0.5f;
	Sensor::setMockValue(SensorType::LuaGauge1, 1.0f); // would otherwise assert Sport Mode

	getSm().reportLimpCondition();
	engine->periodicSlowCallback();

	EXPECT_TRUE(getSm().engineSmIsLimp);
	EXPECT_FALSE(getSm().engineSmIsSportMode);
}

TEST(EngineStateMachine, ghostCamActivatesViaSportMode) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	MockIdleController mockIdle;
	engine->engineModules.get<IdleController>().set(&mockIdle);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Idling));
	engine->rpmCalculator.setRpmValue(TEST_RUNNING_RPM);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, 0.0f);

	getCustomPage()->ghostCamEnabled = true;
	getCustomPage()->ghostCamActivationSource = true; // 1 = Sport Mode
	getCustomPage()->ghostCamCltMin = -40;
	getCustomPage()->smSportModeActivationMode = SPORT_MODE_LUA_GAUGE;
	getCustomPage()->smSportModeLuaGauge = LUA_GAUGE_1;
	getCustomPage()->smSportModeLuaGaugeMeaning = LUA_GAUGE_LOWER_BOUND;
	getCustomPage()->smSportModeLuaGaugeThreshold = 0.5f;

	Sensor::setMockValue(SensorType::LuaGauge1, 0.0f);
	engine->periodicSlowCallback();
	EXPECT_FALSE(getSm().engineSmIsGhostCam);

	Sensor::setMockValue(SensorType::LuaGauge1, 1.0f);
	engine->periodicSlowCallback();
	EXPECT_TRUE(getSm().engineSmIsGhostCam);
}

// ---- Temperature overlay ----

TEST(EngineStateMachine, tempOverlay_cold) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	getCustomPage()->smColdTempThreshold = 60;
	getCustomPage()->smHotTempThreshold  = 100;
	enterRunning();

	Sensor::setMockValue(SensorType::Clt, 30.0f);
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_TRUE(sm.engineSmIsCold);
	EXPECT_FALSE(sm.engineSmIsOperating);
	EXPECT_FALSE(sm.engineSmIsHot);
}

TEST(EngineStateMachine, tempOverlay_operating) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	getCustomPage()->smColdTempThreshold = 60;
	getCustomPage()->smHotTempThreshold  = 100;
	enterRunning();

	Sensor::setMockValue(SensorType::Clt, 80.0f);
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsCold);
	EXPECT_TRUE(sm.engineSmIsOperating);
	EXPECT_FALSE(sm.engineSmIsHot);
}

TEST(EngineStateMachine, tempOverlay_hot) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	getCustomPage()->smColdTempThreshold = 60;
	getCustomPage()->smHotTempThreshold  = 100;
	enterRunning();

	Sensor::setMockValue(SensorType::Clt, 110.0f);
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsCold);
	EXPECT_FALSE(sm.engineSmIsOperating);
	EXPECT_TRUE(sm.engineSmIsHot);
}

TEST(EngineStateMachine, tempOverlay_noSensor_treatsAsOperating) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	enterRunning();

	// No CLT sensor set — should not false-trigger Cold or Hot
	Sensor::resetMockValue(SensorType::Clt);
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsCold);
	EXPECT_TRUE(sm.engineSmIsOperating);
	EXPECT_FALSE(sm.engineSmIsHot);
}

TEST(EngineStateMachine, tempOverlay_exactlyAtColdThreshold_isNotCold) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	getCustomPage()->smColdTempThreshold = 60;
	getCustomPage()->smHotTempThreshold  = 100;
	enterRunning();

	// Exactly at the threshold is Operating, not Cold (strict less-than)
	Sensor::setMockValue(SensorType::Clt, 60.0f);
	engine->periodicSlowCallback();

	auto& sm = getSm();
	EXPECT_FALSE(sm.engineSmIsCold);
	EXPECT_TRUE(sm.engineSmIsOperating);
}
