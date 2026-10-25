#include "pch.h"
#include "custom_page.h"

#include "engine_state_machine.h"
#include "dfco.h"

// Default cranking RPM threshold used by TEST_ENGINE
static constexpr float TEST_CRANKING_RPM = 400.0f;
static constexpr float TEST_RUNNING_RPM  = 800.0f;

[[maybe_unused]] static void setupSmConfig() {
	engineConfiguration->useEngineStateMachine = true;
	engineConfiguration->smWotTpsThreshold = 90;
	engineConfiguration->smTransientHoldTimeMs = 0; // no hold-off by default in tests
	// Lower AE threshold so tests can trigger Transient with small TPS steps on Tps1
	engineConfiguration->tpsAccelEnrichmentThreshold = 5.0f;
}

// Standard accumulator config used across direction-detection tests.
// At threshold (500 RPM/s or 5 km/h/s), gain=20 %/s → 1%/callback @50ms.
// Strong signals (4× threshold) build ±80 % in 20 callbacks; flat signal decays to 0.
[[maybe_unused]] static void setupAccumulatorConfig() {
	engineConfiguration->smAccelRateThreshold           = 500;
	engineConfiguration->smDecelRateThreshold           = 500;
	engineConfiguration->smAccumulatorGain              = 20.0f;
	engineConfiguration->smAccumulatorDecayRate         = 10.0f;
	engineConfiguration->smAccumulatorUpshiftThreshold  = 40;
	engineConfiguration->smAccumulatorDownshiftThreshold = 40;
}

[[maybe_unused]] static EngineStateMachine& getSm() {
	return engine->module<EngineStateMachine>().unmock();
}

[[maybe_unused]] static EngineStateMachineState runAndGetState() {
	engine->periodicSlowCallback();
	return getSm().getCurrentState();
}

// ---- Disabled by default ----

// AlphaX operating-mode overlays (Limp, Eco, Ghost Cam, Sport Mode, Pops & Bangs,
// torque-reduction/traction) layered on the upstream Engine State Machine core.

// ---- Helper: put the engine into a stable running state ----
[[maybe_unused]] static void enterRunning(float rpmVal = 2000.0f, float tpsVal = 50.0f) {
	engine->rpmCalculator.setRpmValue(rpmVal);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tpsVal);
	engine->periodicSlowCallback();
}

// ---- Flat-shift torque reduction ----


TEST(EngineStateMachine, tractionControlSetsTorqueReduction) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();

	// Configure a non-zero ETB drop (positive table value = throttle % removed) across the whole
	// traction-control table, then reload the controller's Map3D tables from the freshly-written config.
	for (size_t s = 0; s < efi::size(engineConfiguration->tractionControlEtbDrop); s++) {
		for (size_t v = 0; v < efi::size(engineConfiguration->tractionControlEtbDrop[0]); v++) {
			engineConfiguration->tractionControlEtbDrop[s][v] = 10;
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

// ---- Clutch-based shift detection ----

[[maybe_unused]] static void setupShiftDetectionConfig() {
	setupSmConfig();
	// Both directions mapped to Clutch Down switch, RPM Rate accumulator mode
	engineConfiguration->smUpshiftClutchSwitch   = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smDownshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smShiftDetectionMode    = sm_shift_detection_mode_e::RpmRate;
	setupAccumulatorConfig();
}

// Run N callbacks at constant RPM/TPS. With constant RPM the accumulator stays at 0
// (rate=0 → dead-band decay from 0). Used by single-direction tests where no
// accumulator build-up is required before pressing the clutch.
[[maybe_unused]] static void seedHistory(int callbacks = 10, float rpm = 2000.0f, float tps = 50.0f) {
	engine->rpmCalculator.setRpmValue(rpm);
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tps);
	for (int i = 0; i < callbacks; i++) {
		engine->periodicSlowCallback();
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	}
}

// Run N callbacks with linearly ramping RPM. Used to pre-build the shift-direction accumulator
// before pressing the clutch. Rising RPM → positive accumulator (upshift); falling → negative.
[[maybe_unused]] static void seedRampedRpm(float startRpm, float rpmPerCallback, int callbacks, float tps) {
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tps);
	for (int i = 0; i < callbacks; i++) {
		engine->rpmCalculator.setRpmValue(startRpm + rpmPerCallback * i);
		engine->periodicSlowCallback();
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	}
}


// ---- RPM Rate accumulator shift detection ----

[[maybe_unused]] static void setupRpmRateModeConfig() {
	setupSmConfig();
	engineConfiguration->smUpshiftClutchSwitch   = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smDownshiftClutchSwitch = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smShiftDetectionMode    = sm_shift_detection_mode_e::RpmRate;
	setupAccumulatorConfig(); // sets rate thresholds and accumulator params
}

// Rising RPM builds a positive accumulator → upshift confirmed.
// 2000 RPM/s at 500 RPM/s threshold → 4× → 4 %/callback → 80 % after 20 callbacks (threshold 40 %).

TEST(EngineStateMachine, ecoEngageSuppressesFalseAcceleratingDuringSettleHoldoff) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	// m_lastRpmRate is only ever computed inside updateShiftAccumulator(), which
	// updateShiftDetection() skips entirely unless a clutch switch is configured.
	engineConfiguration->smUpshiftClutchSwitch       = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smAccelRateThreshold        = 500; // RPM/s
	engineConfiguration->smRpmRateWindowMs           = 50;  // one tick
	engineConfiguration->smTransientHoldTimeMs       = 150;  // settle window after the eco edge (3 ticks @ 50 ms)

	getCustomPage()->ecoModeEnabled      = true;
	getCustomPage()->ecoModeCruisingTime = 0; // engage as soon as Cruising is seen
	getCustomPage()->ecoModeMapLimit     = 0;
	getCustomPage()->ecoModeMinVss       = 0;
	getCustomPage()->ecoModeSwitchMode   = eco_mode_switch_mode_e::Off;

	MockIdleController mockIdle;
	engine->module<IdleController>().set(&mockIdle);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Running));

	enterRunning(2000.0f, 50.0f);
	auto& sm = getSm();
	ASSERT_EQ(EngineStateMachineState::Cruising, sm.getCurrentState());
	ASSERT_TRUE(sm.engineSmIsEcoMode); // engaged this tick -> settle holdoff armed

	// Eco's own actuation now drives an RPM ramp comfortably above smAccelRateThreshold, standing
	// in for the ecoThrottleMult/VVT step. Despite the ramp, the settle-holdoff ticks must not
	// read it as Accelerating, so eco must not drop.
	float rpm = 2000.0f;
	for (int i = 0; i < 3; i++) {
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
		rpm += 200.0f;
		engine->rpmCalculator.setRpmValue(rpm);
		engine->periodicSlowCallback();
		EXPECT_NE(EngineStateMachineState::Accelerating, sm.getCurrentState());
		EXPECT_TRUE(sm.engineSmIsEcoMode);
	}

	// Once the holdoff is exhausted, a persisting high rate is (correctly) detected as real
	// Accelerating, and eco drops -- the suppression is a brief settle window, not a permanent
	// blind spot.
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	rpm += 200.0f;
	engine->rpmCalculator.setRpmValue(rpm);
	engine->periodicSlowCallback();
	EXPECT_EQ(EngineStateMachineState::Accelerating, sm.getCurrentState());
	EXPECT_FALSE(sm.engineSmIsEcoMode);
}

TEST(EngineStateMachine, genuineAccelerationDropsEcoWithoutArmingHoldoff) {
	EngineTestHelper eth(engine_type_e::TEST_ENGINE);
	setupSmConfig();
	engineConfiguration->smUpshiftClutchSwitch       = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smAccelRateThreshold        = 500; // RPM/s
	engineConfiguration->smRpmRateWindowMs           = 50;  // one tick
	engineConfiguration->smTransientHoldTimeMs       = 150;

	getCustomPage()->ecoModeEnabled      = true;
	getCustomPage()->ecoModeCruisingTime = 0;
	getCustomPage()->ecoModeMapLimit     = 0;
	getCustomPage()->ecoModeMinVss       = 0;
	getCustomPage()->ecoModeSwitchMode   = eco_mode_switch_mode_e::Off;

	MockIdleController mockIdle;
	engine->module<IdleController>().set(&mockIdle);
	ON_CALL(mockIdle, getCurrentPhase()).WillByDefault(Return(IIdleController::Phase::Running));

	enterRunning(2000.0f, 50.0f);
	auto& sm = getSm();
	ASSERT_TRUE(sm.engineSmIsEcoMode); // engaging itself arms a 3-tick settle holdoff

	// Burn through the initial engage-edge settle holdoff with a hard, sustained ramp -- once it
	// expires (4 ticks: 3 suppressed + 1 that sees the now-stale-but-still-high rate), the ramp is
	// (correctly) detected as real Accelerating and eco drops.
	float rpm = 2000.0f;
	for (int i = 0; i < 3; i++) {
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
		rpm += 400.0f;
		engine->rpmCalculator.setRpmValue(rpm);
		engine->periodicSlowCallback();
	}
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	rpm += 400.0f;
	engine->rpmCalculator.setRpmValue(rpm);
	engine->periodicSlowCallback();
	EXPECT_EQ(EngineStateMachineState::Accelerating, sm.getCurrentState());
	EXPECT_FALSE(sm.engineSmIsEcoMode);

	// The very next tick, RPM keeps climbing at the same hard rate -- if this disengage had
	// (incorrectly) armed a new settle holdoff, this tick would be masked back to Cruising/Idle
	// instead of staying Accelerating.
	advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	rpm += 400.0f;
	engine->rpmCalculator.setRpmValue(rpm);
	engine->periodicSlowCallback();
	EXPECT_EQ(EngineStateMachineState::Accelerating, sm.getCurrentState());
}

// ---- Clutch-Up-only shift detection (single switch, no Clutch-Down wired) ----

[[maybe_unused]] static void setupClutchUpOnlyConfig() {
	setupSmConfig();
	engineConfiguration->smUpshiftClutchSwitch         = sm_clutch_switch_e::ClutchUp;
	engineConfiguration->smDownshiftClutchSwitch       = sm_clutch_switch_e::None;
	engineConfiguration->smShiftDetectionMode          = sm_shift_detection_mode_e::RpmRate;
	engineConfiguration->smSecondClutchSwitchAvailable = false;
	setupAccumulatorConfig();
}

// The pedal leaving rest (falling edge) is the moment the drivetrain is still locked, which is
// when the window must open. This is the edge that was previously missed entirely.

[[maybe_unused]] static void setupSecondSwitchUpPrimaryConfig() {
	setupSmConfig();
	engineConfiguration->smUpshiftClutchSwitch   = sm_clutch_switch_e::ClutchUp;
	engineConfiguration->smDownshiftClutchSwitch = sm_clutch_switch_e::None;
	engineConfiguration->smShiftDetectionMode    = sm_shift_detection_mode_e::RpmRate;
	setupAccumulatorConfig();
}

// With a Clutch-Down switch also wired, its falling edge (release starting) should close the
// window earlier than waiting for Clutch-Up to fully return to rest.

// ---- VSS Rate mode: minimum-speed gate (smShiftMinVss) ----

[[maybe_unused]] static void setupVssRateModeConfig() {
	setupSmConfig();
	engineConfiguration->smUpshiftClutchSwitch    = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smDownshiftClutchSwitch  = sm_clutch_switch_e::ClutchDown;
	engineConfiguration->smShiftDetectionMode     = sm_shift_detection_mode_e::VssRate;
	engineConfiguration->smAccelRateThreshold     = 5;  // km/h per second
	engineConfiguration->smDecelRateThreshold     = 5;
	engineConfiguration->smAccumulatorGain              = 20.0f;
	engineConfiguration->smAccumulatorDecayRate         = 10.0f;
	engineConfiguration->smAccumulatorUpshiftThreshold  = 40;
	engineConfiguration->smAccumulatorDownshiftThreshold = 40;
}

// Seed the history buffer with linearly ramping VSS, mirroring seedRampedRpm.
[[maybe_unused]] static void seedRampedVss(float startVss, float vssPerCallback, int callbacks, float tps) {
	Sensor::setMockValue(SensorType::DriverThrottleIntent, tps);
	for (int i = 0; i < callbacks; i++) {
		Sensor::setMockValue(SensorType::VehicleSpeed, startVss + vssPerCallback * i);
		engine->periodicSlowCallback();
		advanceTimeUs(SLOW_CALLBACK_PERIOD_MS * 1000);
	}
}

// Below smShiftMinVss the accumulator decays instead of accumulating, so the confirmation
// threshold is never reached even when the VSS rate would otherwise qualify.

// ---- smRelatchOnClutchDown: a fresh Clutch-Down press refreshes an active latch ----

[[maybe_unused]] static void setupRelatchConfig() {
	setupClutchUpOnlyConfig();
	engineConfiguration->smSecondClutchSwitchAvailable = true;
	engineConfiguration->smShiftLatchTimeMs = 100;
}


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

// ---- Limp Mode overlay ----

// Sync LimpManager transient state by running its fast-callback logic.
[[maybe_unused]] static void updateLimpManager() {
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
	engine->module<IdleController>().set(&mockIdle);
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
