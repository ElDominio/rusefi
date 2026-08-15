// file custom_page.cpp

#include "pch.h"
#include "custom_page.h"
#include "extra_flash_pages.h"

static constexpr uint32_t PAGE6_DATA_VERSION = 22;

using page6_container_s = ExtraPageContainer<page6_s, PAGE6_DATA_VERSION>;

static_assert(sizeof(page6_container_s) % 32 == 0,
	"page6_container_s must be 32-byte aligned for STM32H7 flash writes");

static page6_container_s customPageContainer;

void customPageSetDefaults() {
	customPageContainer.data = {};

	auto& d = customPageContainer.data;

	// Downshift Blipper — disabled by default; sane starting calibration.
	d.downshiftBlipperEnabled = false;
	d.downshiftBlipperRequireBrake = false;
	d.downshiftBlipperRequireSportMode = false;

	d.downshiftBlipperDriverTpsThreshold = 15;
	d.downshiftBlipperMaxTpsLimit = 40;

	d.downshiftBlipperMaxTimeMs = 250;
	d.downshiftBlipperLockoutTimeMs = 500;
	d.downshiftBlipperMinRpm = 1500;
	d.downshiftBlipperMaxRpm = 6200;
	d.downshiftBlipRampOpenMs = 15;
	d.downshiftBlipRampCloseMs = 30;
	d.downshiftBlipOpenLoopWindowRpm = 100;
	d.downshiftBlipperMinVss = 15;

	d.downshiftBlipperKp = 2;
	d.downshiftBlipperKi = 0;
	d.downshiftBlipperKd = 0;

	// Engine State Machine thresholds + shift detection (enable bit lives in page 1).
	d.smWotTpsThreshold = 90;           // 90% TPS
	d.smTransientHoldoffCallbacks = 4;  // 200 ms at 20 Hz
	d.smUpshiftClutchSwitch   = sm_clutch_switch_e::None;
	d.smDownshiftClutchSwitch = sm_clutch_switch_e::None;
	d.smShiftDetectionMode    = sm_shift_detection_mode_e::RpmRate;
	d.smAccelRateThreshold = 0;
	d.smDecelRateThreshold = 0;
	d.smRpmRateWindowMs    = 100; // 100 ms (2 slow-callback ticks) -- recompute cadence for RPM/t vs. the old raw single-tick behavior
	d.smRelatchOnClutchDown    = false;
	d.smShiftLatchTimeMs       = 0;
	d.smShiftMinVss            = 0;
	d.smAccumulatorUpshiftThreshold  = 40;
	d.smAccumulatorDownshiftThreshold = 40;
	d.smAccumulatorGain      = 20.0f;
	d.smAccumulatorDecayRate = 10.0f;

	// Sport Mode (Engine State Machine) — disabled by default.
	d.smSportModeActivationMode  = SPORT_MODE_OFF;
	d.smSportModeSwitchPin       = Gpio::Unassigned;
	d.smSportModeSwitchPinMode   = PI_DEFAULT;
	d.smSportModeLuaGauge        = LUA_GAUGE_1;
	d.smSportModeLuaGaugeMeaning = LUA_GAUGE_LOWER_BOUND;
	d.smSportModeLuaGaugeThreshold = 0.0f;

	// Limp Mode (Engine State Machine sub-feature) — conservative "get-home" defaults.
	d.limpSeverityThreshold  = 5;    // one latched misfire (5 severity pts) latches limp
	d.limpModeEtbLimit       = 30;   // cap throttle at 30%
	d.limpModeTimingReduction = 5;   // pull 5 deg of timing
	d.limpModeAfrEnrichment  = 5;    // 5% richer than target
	d.limpModeRevLimit       = 3000; // 3000 RPM hard limit
	d.limpModeBoostLimit     = 0;    // 0 = no boost ceiling

	// Misfire Detection (Engine State Machine sub-feature) — disabled by default.
	d.misfireDetectionEnabled = false;
	d.misfireConsecutiveCount  = 2;     // need >=2 flagged firings within the window to count one
	d.misfireWindowFirings     = 12;   // sliding window: last 12 firings, any cylinder (~3 cycles on a 4-cyl)
	d.misfireCountThreshold    = 50;   // 50 counted events throws the misfire DTC (P0300)
	d.misfireK                 = 3.0f; // threshold = baseline + 3 * wobble
	d.misfireEmaAlphaDecel     = 0.05f;  // positive delta (engine slowing): track RPM drops quickly
	d.misfireEmaAlphaAccel     = 0.005f; // negative delta (engine faster): resist upward drift during misfires
	d.misfireWobbleAlphaRise   = 0.1f;   // wobble rising (engine rougher): track fast to avoid false positives
	d.misfireWobbleAlphaFall   = 0.01f;  // wobble falling (engine smoother): relax slowly, stay conservative
	d.misfireSettleCycles      = 200;    // ~2 s on a 4-cyl at 800 RPM before detection goes live
	d.misfireWindowStart       = 20;   // expansion stroke: TDC + 20 deg
	d.misfireWindowEnd         = 120;  //                   TDC + 120 deg

	// Burst Knock (transient ignition timing pull) — disabled by default; seed usable axes.
	d.burstKnockEnabled = false;
	d.burstKnockDecayTime = 1.0f; // pull decays back to zero over 1 s
	for (size_t i = 0; i < efi::size(d.burstKnockRpmBins); i++) {
		d.burstKnockRpmBins[i] = 800 + i * 800;      // 800 .. 6400 rpm
	}
	for (size_t i = 0; i < efi::size(d.burstKnockTpsRateBins); i++) {
		d.burstKnockTpsRateBins[i] = i * 150;        // 0 .. 1050 %/s
	}
	// burstKnockRetardTable left at zero (no pull) until the user tunes it.

	// WOT Time Enrichment — disabled by default; seed a 0..30 s time axis, no enrichment yet.
	d.wotEnrichmentEnabled = false;
	for (size_t i = 0; i < efi::size(d.wotEnrichmentTimeBins); i++) {
		d.wotEnrichmentTimeBins[i] = i * (30.0f / 7);  // 0 .. 30 s
	}
	// wotEnrichmentAfrAdder left at zero (no enrichment) until the user tunes it.

	// Sport Pedal (ETB pedal-to-throttle ratio shaping) — disabled by default; pass-through curve.
	d.sportPedalActivationMode   = SPORT_PEDAL_OFF;
	d.sportPedalSwitchPin        = Gpio::Unassigned;
	d.sportPedalSwitchPinMode    = PI_DEFAULT;
	for (size_t i = 0; i < efi::size(d.sportPedalPedalBins); i++) {
		d.sportPedalPedalBins[i] = i * (100.0f / 7); // 0 .. 100 % pedal
		d.sportPedalMultValues[i] = 1.0f;            // 1.0 = pass-through until the user tunes it
	}

	// Engine Temperature Overlay (Engine State Machine sub-feature).
	d.smColdTempThreshold = 60;   // below 60°C = Cold (not yet at operating temp)
	d.smHotTempThreshold  = 100;  // above 100°C = Hot (typical thermostat-open range)

	// Quick Warmup (Engine State Machine sub-feature) — disabled by default; safe inert calibration.
	d.quickWarmupEnabled      = false;
	d.quickWarmupTimingRetard = -8.0f;   // 8 deg retard — heats exhaust for catalyst light-off
	d.quickWarmupLambdaTarget = 0.95f;   // 5% rich — improves combustion stability when cold
	d.quickWarmupEtbOffset    = 3;       // 3% extra throttle — compensates for reduced torque

	// Eco Mode (Engine State Machine sub-feature) — disabled by default; inert calibration.
	d.ecoModeEnabled       = false;
	d.ecoModeVvtOverride   = false;
	d.ecoModeSwitchMode    = eco_mode_switch_mode_e::Off;
	d.ecoModeSwitchPin     = Gpio::Unassigned;
	d.ecoModeSwitchPinMode = PI_DEFAULT;
	d.ecoModeLuaGauge      = LUA_GAUGE_1;
	d.ecoModeLuaGaugeMeaning = LUA_GAUGE_LOWER_BOUND;
	d.ecoModeLuaGaugeValue = 0.0f;
	d.ecoModeCruisingTime  = 10;     // 10 s of steady cruise before eco engages
	d.ecoModeMapLimit      = 0;      // disabled by default — no MAP gate
	d.ecoTargetAfr         = 15.5f;  // slightly leaner than stoich for economy
	d.ecoTimingAdder       = 0.0f;   // no timing change until the user tunes it
	d.ecoVvtIntakeTarget   = 0.0f;
	d.ecoVvtExhaustTarget  = 0.0f;
	d.ecoThrottleMult      = 1.0f;   // pass-through until the user tunes it

	// Pops and Bangs (enable bit lives in page 1).
	d.popsAndBangsDelay = 1.0f;
	d.popsAndBangsDuration = 2.0f;
	d.popsAndBangsAirAdd = 0;
	d.popsAndBangsRpmHigh = 2500;
	d.popsAndBangsRpmLow = 1800;
	d.popsAndBangsRpmMax = 5500;
	d.popsAndBangsCltMin = 40;
	d.popsAndBangsCltMax = 105;
	d.popsAndBangsTimingOverride = -10.0f;
	d.popsAndBangsVeOverride = 30.0f;
	d.popsAndBangsRequireSportMode = false;

	// Pops and Bangs spark cut (overlay on top of P&B).
	d.popsAndBangsSparkCutEnabled = false;
	d.popsAndBangsCutDurationAuto = false;
	d.popsAndBangsCutEveryRevs = 4;
	d.popsAndBangsCutPercent = 60;
	d.popsAndBangsCutDurationMs = 100;

	// Advanced / PWM Fuel Pump — default to single relay so initFuelPumpPwm() returns
	// early and never hands the pin to the SimplePwm scheduler.
	d.fuelPumpMode = FP_MODE_SINGLE;
	d.fuelPumpMinDuty = 0;
	d.fuelPumpMaxDuty = 100;
	d.fuelPumpPwmFrequency = 100;
	d.fuelPump_iTermMin = -30;
	d.fuelPump_iTermMax = 30;

	// Dwell Duty Mode — disabled by default; 50% is the standard TFI module target.
	d.dwellDutyModeEnabled = false;
	d.dwellDutyPercent = 50;

	// Check Engine Triggering — all checks disabled by default; each check is worth 1 point.
	d.tpsCircuitCelEnable = false;
	d.tpsIntermittentCelEnable = false;
	d.oilPressureLowCelEnable = false;
	d.cltHighCelEnable = false;
	d.afrCelEnable = false;
	d.voltageCelEnable = false;
	d.celPointsThreshold = 1;       // one tripped, enabled check raises the DTC, CEL solid
	d.celBlinkPointsThreshold = 2;  // two simultaneously tripped checks escalate to a flashing CEL
	d.celDebounceTimeSec = 5.0f;    // every check's condition must hold for 5s before tripping/clearing
	d.tpsIntermittentFlipCount = 3; // 3+ ok/fault flips within celDebounceTimeSec trips Intermittent

	// Malfunction Indicator KOEO bulb check — disabled by default, independent of Check Engine
	// Triggering above.
	d.celOnKoeo = false;

	// Injector Small Pulse % Correction Curve — evenly-spaced bins 0..2 ms, all corrections zero.
	// User fills in vendor data (e.g. Injector Dynamics NFC) before enabling Curve (%) mode.
	for (size_t i = 0; i < efi::size(d.injectorSmallPulseCurveBins); i++) {
		d.injectorSmallPulseCurveBins[i] = i * (2.0f / (efi::size(d.injectorSmallPulseCurveBins) - 1));
		d.injectorSmallPulseCurveValues[i] = 0.0f;
	}

	// Intake Manifold Runner Control — disabled by default; solenoid defaults at 100% open, 0% closed.
	d.imrcMode                = IMRC_DISABLED;
	d.imrcInvert              = false;
	d.imrcRpmThreshold        = 0;
	d.imrcTpsThreshold        = 0;
	d.imrcMapKpaThreshold     = 0.0f;
	d.imrcSolenoid1Pin        = Gpio::Unassigned;
	d.imrcSolenoid1PinMode    = OM_DEFAULT;
	d.imrcSolenoid2Pin        = Gpio::Unassigned;
	d.imrcSolenoid2PinMode    = OM_DEFAULT;
	d.imrcSolenoidFrequency   = 100;
	d.imrcSolenoidOpenDuty    = 100;
	d.imrcSolenoidClosedDuty  = 0;
	d.imrcHBridgePin1         = Gpio::Unassigned;
	d.imrcHBridgePin1Mode     = OM_DEFAULT;
	d.imrcHBridgePin2         = Gpio::Unassigned;
	d.imrcHBridgePin2Mode     = OM_DEFAULT;
	d.imrcHBridgeFrequency    = 1000;
	d.imrcHBridgeDutyCycle    = 100;
	d.imrcHBridgeMoveDurationS = 1.0f;

	// Ghost Cam Mode — disabled by default; inert calibration (stoich AFR, zero cam angle).
	d.ghostCamEnabled = false;
	d.ghostCamActivationSource = false; // 0 = Switch
	d.ghostCamActivatePin = Gpio::Unassigned;
	d.ghostCamActivatePinMode = PI_DEFAULT;
	d.ghostCamCltMin = 60;           // require operating temp
	d.ghostCamIdleRpm = 800;         // match a typical warm idle target
	d.ghostCamTargetAfr = 14.7f;     // stoich — user tunes toward 16+ for lope effect
	d.ghostCamIdleBaseDuty = 30;     // reasonable open-loop starting point
	d.ghostCamIntakeCamAngle = 0.0f; // no overlap until user tunes
	d.ghostCamExhaustCamAngle = 0.0f;
	d.ghostCamTimingPid_pFactor  = 0.1f;
	d.ghostCamTimingPid_iFactor  = 0;
	d.ghostCamTimingPid_dFactor  = 0;
	d.ghostCamTimingPid_minValue = -10; // max retard
	d.ghostCamTimingPid_maxValue = 10;  // max advance

	// CLT Estimator (competing-rate radiator model estimating CLT from CHT) — disabled by
	// default. All curve/valve values below are PLACEHOLDER physically-motivated calibration,
	// NOT bench-validated. Re-validate on a bench/real vehicle before treating these as real
	// defaults. See cht_clt_estimator.h.
	d.cltFromCht = false;
	d.cltEstThermostatTemp = 88;    // typical ~190F thermostat
	d.cltEstFallbackClt = 20;       // reported CLT if CHT itself is invalid
	d.cltEstRadiatorFailTemp = 126; // above this CHT, assume zero radiator airflow and lock CLT=CHT
	d.cltEstThermostatLag = 15;     // wax-pellet valve response lag
	d.cltEstCoolantLag = 5;         // coolant thermal-mass/circulation lag on airflow's effect
	d.cltEstFan1Cfm = 400;
	d.cltEstFan2Cfm = 400;
	d.cltEstThermostatSpread = 8;   // deg C from closed to fully open

	// Head-to-coolant heat transfer: rate at a 90 deg C reference CHT, plus a gain for how much
	// faster that transfer happens as CHT climbs above (or below) the reference. Approximates
	// the old 40-140 deg C curve (0.020..0.045 1/s) to within a few percent across its range.
	d.cltEstHeadTransferRate = 0.030f;  // 1/s at 90 deg C
	d.cltEstHeadTransferGain = 0.00025f; // 1/s per deg C above the reference

	// Radiator rejection vs airflow: near-zero rejection at zero airflow (fans off, stopped),
	// increasing with real airflow (applied once the thermostat valve has opened). Approximates
	// the old 0-900 CFM curve (0.0025..0.030 1/s) to within a few percent across its range.
	d.cltEstRadiatorBaseRejection = 0.0035f; // 1/s at zero airflow
	d.cltEstRadiatorAirflowGain = 0.00003f;  // 1/s per CFM

	// VSS -> ram-air CFM: saturating curve, non-degenerate out of the box so the model is usable
	// without retuning. Approximates the old 0-180 kph curve (0..700 CFM) to within ~10% across
	// its range -- worst case is the low-speed knee, where the old curve rose slightly faster.
	d.cltEstVssMaxAirflow = 800.0f;    // CFM approached at high speed
	d.cltEstVssHalfFlowSpeed = 70.0f;  // kph to reach half of max ram-air flow

	// Radiator effectiveness vs IAT/AAT: default zero gain (no-op, effectiveness always 1.0
	// regardless of reference temp) — leave the shape/direction of hot-underhood-air degradation
	// to user tuning rather than guessing it.
	d.cltEstRadiatorReferenceTemp = 25;         // deg C -- "typical" calibration air temp
	d.cltEstRadiatorEffectivenessIatGain = 0.0f;

	// Rolling Launch Control — disabled by default; sane gates so it is usable once enabled.
	d.rollingLaunchEnabled        = false;
	d.rollingLaunchActivatePin    = Gpio::Unassigned;
	d.rollingLaunchActivatePinMode = PI_DEFAULT;
	d.rollingLaunchRpmWindow      = 500;   // hold target = captured RPM + 500
	d.rollingLaunchMinArmRpm      = 2500;  // do not arm while lugging below 2500 RPM
	d.rollingLaunchMaxRpm         = 7000;  // absolute over-rev ceiling for the captured target
	d.rollingLaunchMinVss         = 10;    // must be rolling (>= 10 km/h) to arm
	d.rollingLaunchTimingRetard   = 10;    // 10 deg pulled while held to build boost
	d.rollingLaunchFuelAdderPercent = 5;   // 5% richer while held
	d.rollingLaunchRampOutTime    = 1.0f;  // pulled timing decays to zero over 1 s on release

	// Weighted Engine Oil Life Monitor — disabled by default; conservative starting multipliers.
	d.oilLifeMonitorEnabled = false;
	d.oilLifePrimarySource = oil_life_temp_source_e::OilTemp;
	d.oilLifeRevsScaleMillions = 6;      // 6,000,000 weighted revs = 0% oil life
	d.oilLifeMultOilCold      = 3.0f;
	d.oilLifeMultOilOptimal   = 1.0f;
	d.oilLifeMultOilHighHeat  = 1.8f;
	d.oilLifeMultOilExtreme   = 4.0f;
	d.oilLifeMultCoolantCold     = 3.5f;
	d.oilLifeMultCoolantOptimal  = 1.1f;
	d.oilLifeMultCoolantHighHeat = 2.0f;
	d.oilLifeMultCoolantExtreme  = 4.5f;

	// VVT Advanced Mode — disabled by default; seed a symmetric -40..40 deg distance axis (zero at
	// the center point, for smooth interpolation through the target) and a pass-through (1.0) oil
	// pressure multiplier curve.
	d.vvtAdvancedModeEnabled = false;
	d.vvtAdvancedPidPauseEnabled = true;
	d.vvtAdvancedPidPauseDeg = 1.0f; // within 1 deg of target, duty curve alone holds position
	for (size_t i = 0; i < efi::size(d.vvtAdvDistanceBinsIntake); i++) {
		d.vvtAdvDistanceBinsIntake[i] = -40.0f + i * 10.0f; // -40 .. 40 deg, zero at center
		d.vvtAdvDistanceBinsExhaust[i] = -40.0f + i * 10.0f;
	}
	// vvtAdvDutyIntake/Exhaust left at zero (no base duty) until the user tunes it.
	for (size_t i = 0; i < efi::size(d.vvtAdvOilPressureBinsIntake); i++) {
		d.vvtAdvOilPressureBinsIntake[i] = i * (1000.0f / 5);  // 0 .. 1000 kPa
		d.vvtAdvOilPressureMultIntake[i] = 1.0f;               // 1.0 = pass-through until the user tunes it
		d.vvtAdvOilPressureBinsExhaust[i] = i * (1000.0f / 5);
		d.vvtAdvOilPressureMultExhaust[i] = 1.0f;
	}
}

void loadCustomPage() {
#if EFI_CONFIGURATION_STORAGE
	if (storageRead(EFI_CUSTOM_PAGE_RECORD_ID,
			customPageGetStoragePtr(),
			customPageGetStorageSize()) == StorageStatus::Ok
		&& customPageIsValid()) {
		return;
	}
#endif
	customPageSetDefaults();
}

bool customPageIsValid() {
	return customPageContainer.isValid();
}

page6_s* getCustomPage() {
	return &customPageContainer.data;
}

void* customPageGetTsPage() {
	return (void*)&customPageContainer.data;
}

size_t customPageGetTsPageSize() {
	return sizeof(page6_s);
}

void customPagePrepareForStorage() {
	customPageContainer.prepareForStorage();
}

uint8_t* customPageGetStoragePtr() {
	return (uint8_t*)&customPageContainer;
}

size_t customPageGetStorageSize() {
	return sizeof(customPageContainer);
}
