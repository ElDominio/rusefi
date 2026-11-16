#include "pch.h"
#include "custom_page.h"
#include "operating_modes.h"
#include "electronic_throttle.h"
#include "check_engine_light.h"
#include "exhaust_cutout.h"
#include "tinymt32.h" // basic 'random' for the P&B automatic cut duration

#if EFI_ENGINE_STATE_MACHINE

// Lower/upper bounds (ms) for the P&B spark-cut window, also used by the 'automatic' random mode.
static constexpr float PNB_CUT_MIN_MS = 20.0f;
static constexpr float PNB_CUT_MAX_MS = 500.0f;

void OperatingModes::reportLimpCondition() {
	m_limpModeLatched = true;
}

void OperatingModes::onSlowCallback() {
	auto& sm = engine->module<EngineStateMachine>().unmock();

	if (!sm.isEnabled()) {
		// Same as the state machine: nothing is classified, so no overlay is active.
		engineSmIsLimp = false;
		engineSmIsPopsAndBangs = false;
		engineSmPnbSparkCut = false;
		engineSmIsEcoMode = false;
		engineSmIsQuickWarmup = false;
		engineSmIsGhostCam = false;
		engineSmIsSportPedal = false;
		engineSmIsSportMode = false;
		return;
	}

	efitimems_t nowMs = getTimeNowMs();
	EngineStateMachineState currentState = sm.getCurrentState();

	// Overlay: Limp Mode — explicit latched protective state.
	//
	// This used to be derived from ANY LimpManager fuel/spark cut, which incorrectly
	// flagged limp for the normal rev limit, boost cut, launch cut, etc. Limp mode is now
	// a dedicated latch (m_limpModeLatched) raised only via reportLimpCondition(). Triggers:
	//   - ETB jam (LimpManager::reportEtbJammed), and
	//   - the CEL escalating to flashing (isCelBlinkingActive() -- Check Engine Triggering's
	//     points-gated celBlinkPointsThreshold, see check_engine_light.cpp). Every check on that
	//     module (TPS Circuit, TPS Intermittent, Misfire, and the battery/MAP/IAT/TPS voltage
	//     checks) contributes a flat point toward the same total, so there is a single points
	//     system rather than a separate DTC-severity threshold for Limp mode.
	// The ordinary rev limit and other transient cuts intentionally do NOT enter limp mode.
#ifdef MODULE_CHECK_ENGINE_LIGHT
	if (isCelBlinkingActive()) {
		reportLimpCondition();
	}
#endif // MODULE_CHECK_ENGINE_LIGHT

	engineSmIsLimp = m_limpModeLatched;

	// Sport Mode overlay — shared "driver wants aggressive behavior" flag consumed by Ghost Cam,
	// Sport Pedal, Exhaust Cutout, Pops & Bangs, Downshift Blip, and the Limiter Adders. Priority
	// chain: Limp out-votes Sport Mode (protective), Sport Mode out-votes Eco Mode (mutually
	// exclusive). Must run after engineSmIsLimp is set (above), and before updateEcoMode() (eco
	// reads engineSmIsSportMode), updateGhostCam() (Ghost Cam's Sport Mode activation source reads
	// it), and updatePopsAndBangs() at the end of this function.
	updateSportMode();

	// Eco mode overlay — must run before the display override below so the dash reflects it
	// this cycle. Depends on the primary state, engineSmIsLimp (limp wins), and
	// engineSmIsSportMode (sport wins — the two are mutually exclusive).
	updateEcoMode(currentState);

	// Arm a settle holdoff on an eco engage/disengage edge: the ecoThrottleMult/VVT step that
	// follows is a real RPM transient but not driver-initiated, and the state machine must not
	// misread it as Accelerating/Decelerating next tick or eco would immediately bounce itself
	// back off (see EngineStateMachine::holdRateClassification()). Skipped when the edge itself was
	// caused by a genuine, already-detected Accelerating/Decelerating this tick -- that's real
	// driver input, not eco noise, and masking further detection would just blind us to its
	// continuation.
	if (engineSmIsEcoMode != m_prevEcoModeActive
			&& currentState != EngineStateMachineState::Accelerating
			&& currentState != EngineStateMachineState::Decelerating) {
		sm.holdRateClassification();
	}
	m_prevEcoModeActive = engineSmIsEcoMode;

	// Ghost Cam overlay — idle-only manual lope effect via switch or Sport Mode.
	// Must run after engineSmIsIdle, engineSmIsLimp and engineSmIsSportMode are set.
	updateGhostCam();

	// Overlay-target transition blend — must run after both engineSmIsEcoMode and
	// engineSmIsGhostCam are finalized above (see getEcoModeBlend()/getGhostCamBlend()).
	{
		efitimems_t blendDtMs = (m_lastBlendTickMs == 0) ? 0 : (nowMs - m_lastBlendTickMs);
		m_lastBlendTickMs = nowMs;
		updateOverlayBlend(m_ecoModeBlend, engineSmIsEcoMode, blendDtMs);
		updateOverlayBlend(m_ghostCamBlend, engineSmIsGhostCam, blendDtMs);
	}

	// Sport Pedal overlay — ETB pedal-to-throttle ratio shaping via switch or Lua gauge.
	updateSportPedal();

	// Quick Warmup overlay — active when Cold AND Idle (not cranking, not after-start).
	engineSmIsQuickWarmup = getCustomPage()->quickWarmupEnabled
	                     && sm.engineSmIsCold
	                     && sm.engineSmIsIdle;

	// Override the display integer for overlay priority: Limp > Upshifting > Downshifting > LaunchControl > Eco
	// The state machine already ranks Upshifting > Downshifting > LaunchControl in
	// engineSmCurrentState; Limp goes in front of them and Eco behind them.
	// Ghost Cam is intentionally NOT in this chain: it only ever runs while engineSmIsIdle is true
	// (see updateGhostCam()), so it stays an overlay on top of Idle rather than a state you "enter" -
	// the dash keeps showing Idle and engineSmIsGhostCam conveys the overlay separately.
	if (engineSmIsLimp) {
		sm.engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::Limp);
	} else if (!sm.engineSmIsUpshifting && !sm.engineSmIsDownshifting && !sm.engineSmIsLaunchControl
			&& engineSmIsEcoMode) {
		sm.engineSmCurrentState = static_cast<uint8_t>(EngineStateMachineState::Eco);
	}

	updatePopsAndBangs(sm.engineSmIsOverrun);
}

void OperatingModes::updateEcoMode(EngineStateMachineState currentState) {
	// Limp mode is protective and out-votes eco; Sport Mode is the driver's explicit request for
	// aggressive behavior and is mutually exclusive with eco, so it out-votes eco too. A disabled
	// feature is always inactive. These override even an active engage lock below -- safety and
	// explicit driver intent always win.
	if (!getCustomPage()->ecoModeEnabled || engineSmIsLimp || engineSmIsSportMode) {
		m_ecoCruiseTimer.reset();
		engineSmIsEcoMode = false;
		return;
	}

	// Inhibit also overrides the engage lock below -- the driver's explicit "no eco" request wins.
	bool inhibited = isEcoModeInhibited();

	// Post-engage lock: once eco is active, hold it forced on and skip all gate/state
	// re-evaluation for ecoModeEngageLockTime. Eco's own AFR/timing/VVT/throttle step is a real
	// change in torque delivery, and re-checking Cruising/MAP/VSS/RPM against the resulting
	// transient could misread it as "left Cruising" and immediately drop eco right back off.
	// 0 disables the lock (instant re-evaluation, same as before this existed).
	if (engineSmIsEcoMode && !inhibited
			&& !m_ecoLockTimer.hasElapsedSec(getCustomPage()->ecoModeEngageLockTime)) {
		return;
	}

	// Instant drop: Cruising and brief AE-driven Transient blips both accumulate time -- a
	// throttle flutter that immediately settles back to Cruising is AE noise, not a driver-
	// initiated tip-in/tip-out, and shouldn't cost eco its progress. Any other state (genuine
	// Accelerating/Decelerating/Overrun/etc.) pins the timer at zero, so re-entry must
	// accumulate the full ecoModeCruisingTime again before eco re-engages.
	bool cruiseElapsed = false;
	if (currentState == EngineStateMachineState::Cruising || currentState == EngineStateMachineState::Transient) {
		cruiseElapsed = m_ecoCruiseTimer.hasElapsedSec(getCustomPage()->ecoModeCruisingTime);
	} else {
		m_ecoCruiseTimer.reset();
	}

	// MAP gate: load creeping up above the limit drops eco exactly like leaving Cruising, even if
	// the state machine still reports Cruising (TPS-based) this cycle. 0 disables the gate.
	uint16_t mapLimit = getCustomPage()->ecoModeMapLimit;
	if (mapLimit > 0 && Sensor::get(SensorType::Map).value_or(0) > mapLimit) {
		m_ecoCruiseTimer.reset();
		cruiseElapsed = false;
	}

	// VSS floor: below this speed eco cannot engage regardless of state or MAP. Catches
	// low-speed parking-lot crawls that the cruise timer alone would eventually allow. 0 disables.
	uint16_t minVss = getCustomPage()->ecoModeMinVss;
	if (minVss > 0 && Sensor::getOrZero(SensorType::VehicleSpeed) < static_cast<float>(minVss)) {
		m_ecoCruiseTimer.reset();
		cruiseElapsed = false;
	}

	// RPM ceiling: eco has no business engaging (or staying engaged) above this RPM -- Cruising
	// can still be reported there (TPS-based), but it's no longer the steady-state economy cruise
	// eco targets. 0 disables the gate.
	uint16_t maxRpm = getCustomPage()->ecoModeMaxRpm;
	if (maxRpm > 0 && Sensor::getOrZero(SensorType::Rpm) > static_cast<float>(maxRpm)) {
		m_ecoCruiseTimer.reset();
		cruiseElapsed = false;
	}

	bool newEcoActive = cruiseElapsed && !inhibited;

	// Rising edge: (re-)arm the post-engage lock window.
	if (newEcoActive && !engineSmIsEcoMode) {
		m_ecoLockTimer.reset();
	}

	// Inhibit blocks the timer-based engage; there is no Force-On override of the timer/MAP/RPM gate.
	engineSmIsEcoMode = newEcoActive;
}

// counts as asserted; mirrors the pin + Lua-gauge reading pattern in isPopsAndBangsBlocked().
bool OperatingModes::isEcoModeSwitchAsserted() const {
	bool pinAsserted = false;

#if !EFI_UNIT_TEST
	const switch_input_pin_e pin = getCustomPage()->ecoModeSwitchPin;
	if (isBrainPinValid(pin)) {
		pinAsserted = efiReadPin(pin, getCustomPage()->ecoModeSwitchPinMode);
	}
#endif

	bool gaugeAsserted = false;
	SensorType gaugeType = SensorType::LuaGauge1;
	switch (getCustomPage()->ecoModeLuaGauge) {
		case LUA_GAUGE_2: gaugeType = SensorType::LuaGauge2; break;
		case LUA_GAUGE_3: gaugeType = SensorType::LuaGauge3; break;
		case LUA_GAUGE_4: gaugeType = SensorType::LuaGauge4; break;
		case LUA_GAUGE_5: gaugeType = SensorType::LuaGauge5; break;
		case LUA_GAUGE_6: gaugeType = SensorType::LuaGauge6; break;
		case LUA_GAUGE_7: gaugeType = SensorType::LuaGauge7; break;
		case LUA_GAUGE_8: gaugeType = SensorType::LuaGauge8; break;
		default: break;
	}
	const auto gaugeResult = Sensor::get(gaugeType);
	if (gaugeResult.Valid) {
		const float value = gaugeResult.Value;
		const float threshold = getCustomPage()->ecoModeLuaGaugeValue;
		switch (getCustomPage()->ecoModeLuaGaugeMeaning) {
			case LUA_GAUGE_LOWER_BOUND: gaugeAsserted = (value >= threshold); break;
			case LUA_GAUGE_UPPER_BOUND: gaugeAsserted = (value <= threshold); break;
		}
	}

	return pinAsserted || gaugeAsserted;
}

bool OperatingModes::isEcoModeInhibited() const {
	return getCustomPage()->ecoModeSwitchMode == eco_mode_switch_mode_e::Inhibit
		&& isEcoModeSwitchAsserted();
}

// Sport Mode: shared "driver wants aggressive behavior" activation source, either a hardware
// switch or a Lua gauge compared against a threshold. Mirrors the pin + Lua-gauge reading
// pattern used by Sport Pedal / Exhaust Cutout (see isSportPedalActive() /
// ExhaustCutoutController::getInputHigh()). Limp mode is protective and out-votes Sport Mode,
// same as it out-votes Eco Mode.
void OperatingModes::updateSportMode() {
	if (engineSmIsLimp) {
		engineSmIsSportMode = false;
		return;
	}

	auto mode = getCustomPage()->smSportModeActivationMode;

	if (mode == SPORT_MODE_SWITCH) {
		bool switchAsserted = false;
#if !EFI_UNIT_TEST
		const switch_input_pin_e pin = getCustomPage()->smSportModeSwitchPin;
		if (isBrainPinValid(pin)) {
			switchAsserted = efiReadPin(pin, getCustomPage()->smSportModeSwitchPinMode);
		}
#endif
		engineSmIsSportMode = switchAsserted;
		return;
	}

	if (mode == SPORT_MODE_LUA_GAUGE) {
		SensorType gaugeType = SensorType::LuaGauge1;
		switch (getCustomPage()->smSportModeLuaGauge) {
			case LUA_GAUGE_2: gaugeType = SensorType::LuaGauge2; break;
			case LUA_GAUGE_3: gaugeType = SensorType::LuaGauge3; break;
			case LUA_GAUGE_4: gaugeType = SensorType::LuaGauge4; break;
			case LUA_GAUGE_5: gaugeType = SensorType::LuaGauge5; break;
			case LUA_GAUGE_6: gaugeType = SensorType::LuaGauge6; break;
			case LUA_GAUGE_7: gaugeType = SensorType::LuaGauge7; break;
			case LUA_GAUGE_8: gaugeType = SensorType::LuaGauge8; break;
			default: break;
		}
		const auto gaugeResult = Sensor::get(gaugeType);
		bool gaugeAsserted = false;
		if (gaugeResult.Valid) {
			const float value = gaugeResult.Value;
			const float threshold = getCustomPage()->smSportModeLuaGaugeThreshold;
			switch (getCustomPage()->smSportModeLuaGaugeMeaning) {
				case LUA_GAUGE_LOWER_BOUND: gaugeAsserted = (value >= threshold); break;
				case LUA_GAUGE_UPPER_BOUND: gaugeAsserted = (value <= threshold); break;
			}
		}
		engineSmIsSportMode = gaugeAsserted;
		return;
	}

	engineSmIsSportMode = false; // SPORT_MODE_OFF
}

void OperatingModes::updateGhostCam() {
#if EFI_GHOST_CAM
	// Requires idle state and non-limp; feature must be enabled.
	if (!getCustomPage()->ghostCamEnabled || !engine->module<EngineStateMachine>().unmock().engineSmIsIdle || engineSmIsLimp) {
		engineSmIsGhostCam = false;
		return;
	}

	// CLT gate: don't engage below minimum coolant temperature.
	auto clt = Sensor::get(SensorType::Clt);
	if (clt && clt.Value < static_cast<float>(getCustomPage()->ghostCamCltMin)) {
		engineSmIsGhostCam = false;
		return;
	}

	// Activation: either the dedicated switch pin or Sport Mode, selected by
	// ghostCamActivationSource (0=Switch, 1=Sport Mode).
	if (getCustomPage()->ghostCamActivationSource == 0) {
		bool switchAsserted = false;
#if !EFI_UNIT_TEST
		const switch_input_pin_e pin = getCustomPage()->ghostCamActivatePin;
		if (isBrainPinValid(pin)) {
			switchAsserted = efiReadPin(pin, getCustomPage()->ghostCamActivatePinMode);
		}
#endif
		engineSmIsGhostCam = switchAsserted;
	} else {
		engineSmIsGhostCam = engineSmIsSportMode;
	}
#else // !EFI_GHOST_CAM
	engineSmIsGhostCam = false;
#endif // EFI_GHOST_CAM
}

// Steps `blend` toward 1.0 (active) or 0.0 (!active) by dtMs/SM_SLOW_TRANSITION_MS, or snaps
// directly to the target when smSlowStateTransitionEnabled is off (legacy instant behavior) or on
// the very first tick (dtMs == 0, no elapsed-time reference yet).
void OperatingModes::updateOverlayBlend(float& blend, bool active, efitimems_t dtMs) {
	float targetBlend = active ? 1.0f : 0.0f;

	if (!getCustomPage()->smSlowStateTransitionEnabled || dtMs <= 0) {
		blend = targetBlend;
		return;
	}

	float step = dtMs / SM_SLOW_TRANSITION_MS;
	if (blend < targetBlend) {
		blend = std::min(targetBlend, blend + step);
	} else if (blend > targetBlend) {
		blend = std::max(targetBlend, blend - step);
	}
}

void OperatingModes::updateSportPedal() {
#if EFI_SPORT_PEDAL
	engineSmIsSportPedal = isSportPedalActive();
#else
	engineSmIsSportPedal = false;
#endif
}

void OperatingModes::updatePopsAndBangs(bool isOverrun) {
	if (!engineConfiguration->popsAndBangsEnabled) {
		m_pnbState = PopsAndBangsState::Inactive;
		engineSmIsPopsAndBangs = false;
		return;
	}

	float rpm = Sensor::getOrZero(SensorType::Rpm);

	if (isOverrun) {
		if (!m_wasPnbOverrun) {
			m_pnbOverrunTimer.reset();
			m_pnbState = PopsAndBangsState::Inactive;
		}

		if (m_pnbState == PopsAndBangsState::Inactive) {
			if (rpm > getCustomPage()->popsAndBangsRpmHigh &&
			    rpm < getCustomPage()->popsAndBangsRpmMax &&
			    m_pnbOverrunTimer.hasElapsedSec(getCustomPage()->popsAndBangsDelay)) {
				const auto clt = Sensor::get(SensorType::Clt);
				bool cltOk = clt &&
					clt.Value >= getCustomPage()->popsAndBangsCltMin &&
					clt.Value <= getCustomPage()->popsAndBangsCltMax;
				if (cltOk && !isPopsAndBangsBlocked()) {
					m_pnbState = PopsAndBangsState::Active;
					m_pnbActiveTimer.reset();
					// Start the spark-cut cycle from a clean firing phase.
					m_pnbCutActive = false;
					m_pnbLastFireRev = getRevolutionCounter();
				} else {
					m_pnbState = PopsAndBangsState::Expired;
				}
			}
		} else if (m_pnbState == PopsAndBangsState::Active) {
			if (rpm < getCustomPage()->popsAndBangsRpmLow ||
			    rpm > getCustomPage()->popsAndBangsRpmMax) {
				m_pnbState = PopsAndBangsState::Expired;
			}

			float duration = getCustomPage()->popsAndBangsDuration;
			if (duration > 0 && m_pnbActiveTimer.hasElapsedSec(duration)) {
				m_pnbState = PopsAndBangsState::Expired;
			}

			if (isPopsAndBangsBlocked()) {
				m_pnbState = PopsAndBangsState::Expired;
			}
		}
	} else {
		m_pnbState = PopsAndBangsState::Inactive;
	}

	m_wasPnbOverrun = isOverrun;
	engineSmIsPopsAndBangs = (m_pnbState == PopsAndBangsState::Active);

	if (!engineSmIsPopsAndBangs) {
		// P&B no longer active — make sure no stale cut window is left open.
		m_pnbCutActive = false;
		engineSmPnbSparkCut = false;
	}
}

static tinymt32_t pnbRandom;

// Returns a fresh random cut-window duration within [PNB_CUT_MIN_MS, PNB_CUT_MAX_MS].
static float pnbRandomCutDurationMs() {
	static bool inited = false;
	if (!inited) {
		tinymt32_init(&pnbRandom, 0x50616E42 /* 'PanB' */);
		inited = true;
	}
	// tinymt32_generate_float returns [0, 1)
	float r = tinymt32_generate_float(&pnbRandom);
	return PNB_CUT_MIN_MS + r * (PNB_CUT_MAX_MS - PNB_CUT_MIN_MS);
}

// Skip ratio (0..1) applied while a cut window is open.
static float pnbCutRatio() {
	return clampF(0.0f, getCustomPage()->popsAndBangsCutPercent, 100.0f) / 100.0f;
}

float OperatingModes::getPopsAndBangsSparkSkipRatio() {
	if (!engineSmIsPopsAndBangs || !getCustomPage()->popsAndBangsSparkCutEnabled) {
		m_pnbCutActive = false;
		engineSmPnbSparkCut = false;
		return 0.0f;
	}

	const uint32_t rev = getRevolutionCounter();

	if (m_pnbCutActive) {
		// Still inside the current cut window? Keep cutting spark so fuel charges the exhaust.
		if (m_pnbCutWindowTimer.getElapsedSeconds() * 1000.0f < m_pnbCutDurationMs) {
			engineSmPnbSparkCut = true;
			return pnbCutRatio();
		}
		// Window elapsed — resume firing (re-lights the charge) and restart the rev count.
		m_pnbCutActive = false;
		m_pnbLastFireRev = rev;
	}

	// Firing normally until the configured number of revolutions have elapsed.
	uint8_t everyRevs = getCustomPage()->popsAndBangsCutEveryRevs;
	if (everyRevs < 1) {
		everyRevs = 1;
	}

	if (rev - m_pnbLastFireRev >= everyRevs) {
		// Open a new spark-cut window.
		m_pnbCutActive = true;
		m_pnbCutWindowTimer.reset();
		if (getCustomPage()->popsAndBangsCutDurationAuto) {
			m_pnbCutDurationMs = pnbRandomCutDurationMs();
		} else {
			m_pnbCutDurationMs = clampF(PNB_CUT_MIN_MS, getCustomPage()->popsAndBangsCutDurationMs, PNB_CUT_MAX_MS);
		}
		engineSmPnbSparkCut = true;
		return pnbCutRatio();
	}

	engineSmPnbSparkCut = false;
	return 0.0f;
}

bool OperatingModes::isPopsAndBangsBlocked() const {
	// Sport Mode gate: when required, pops and bangs cannot activate unless Sport Mode is on.
	bool sportModeBlocked = getCustomPage()->popsAndBangsRequireSportMode && !engineSmIsSportMode;

	bool cutoutBlocked = false;
	if (getCustomPage()->popsAndBangsCutoutInhibitMode == pops_and_bangs_cutout_inhibit_e::Inhibit) {
		// Gate (not a trigger): blocked unless the cutout is actually open.
		cutoutBlocked = !engine->module<ExhaustCutoutController>()->isCutoutOpen;
	}

	return sportModeBlocked || cutoutBlocked;
}

#else // !EFI_ENGINE_STATE_MACHINE

// Operating modes compiled out together with the state machine (board set
// -DEFI_ENGINE_STATE_MACHINE=FALSE). The module stays in the module list so consumers, live data
// and Lua lookups still link; it does nothing and all overlay flags stay false.
void OperatingModes::onSlowCallback() {}
void OperatingModes::reportLimpCondition() {}
void OperatingModes::updatePopsAndBangs(bool /*isOverrun*/) { engineSmIsPopsAndBangs = false; }
void OperatingModes::updateEcoMode(EngineStateMachineState /*currentState*/) { engineSmIsEcoMode = false; }
void OperatingModes::updateSportMode() { engineSmIsSportMode = false; }
void OperatingModes::updateGhostCam() { engineSmIsGhostCam = false; }
void OperatingModes::updateSportPedal() { engineSmIsSportPedal = false; }
float OperatingModes::getPopsAndBangsSparkSkipRatio() { engineSmPnbSparkCut = false; return 0.0f; }

#endif // EFI_ENGINE_STATE_MACHINE
