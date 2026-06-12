// file custom_page.cpp

#include "pch.h"
#include "custom_page.h"
#include "extra_flash_pages.h"

static constexpr uint32_t PAGE5_DATA_VERSION = 1;

using page5_container_s = ExtraPageContainer<page5_s, PAGE5_DATA_VERSION>;

static_assert(sizeof(page5_container_s) % 32 == 0,
	"page5_container_s must be 32-byte aligned for STM32H7 flash writes");

static page5_container_s customPageContainer;

void customPageSetDefaults() {
	customPageContainer.data = {};

	auto& d = customPageContainer.data;

	// Downshift Blipper — disabled by default; sane starting calibration.
	d.downshiftBlipperEnabled = false;
	d.downshiftBlipperRequireBrake = false;
	d.downshiftBlipperUseLuaGauge = false;
	d.downshiftBlipperLuaGauge = 0;

	d.downshiftBlipperDriverTpsThreshold = 15;
	d.downshiftBlipperMaxTpsLimit = 40;

	d.downshiftBlipperMaxTimeMs = 250;
	d.downshiftBlipperLockoutTimeMs = 500;
	d.downshiftBlipperMinRpm = 1500;
	d.downshiftBlipperMaxRpm = 6200;
	d.downshiftBlipRampOpenMs = 15;
	d.downshiftBlipRampCloseMs = 30;
	d.downshiftBlipRpmOffset = 100;
	d.downshiftBlipperMinVss = 15;

	d.downshiftBlipperKp = 2;
	d.downshiftBlipperKi = 0;
	d.downshiftBlipperKd = 0;

	// Default multiplier curve: pass-through (always 1.0).
	for (size_t i = 0; i < efi::size(d.downshiftBlipperLuaMultBins); i++) {
		d.downshiftBlipperLuaMultBins[i] = i;
		d.downshiftBlipperLuaMultValues[i] = 1.0f;
	}

	// Engine State Machine thresholds + shift detection (enable bit lives in page 1).
	d.smShiftTpsThreshold = 5;          // 5% TPS — matches idlePidDeactivationTpsThreshold default
	d.smWotTpsThreshold = 90;           // 90% TPS
	d.smTransientHoldoffCallbacks = 4;  // 200 ms at 20 Hz
	d.smUpshiftClutchSwitch   = sm_clutch_switch_e::None;
	d.smDownshiftClutchSwitch = sm_clutch_switch_e::None;
	d.smShiftDetectionMode    = sm_shift_detection_mode_e::SimpleThrottle;
	d.smShiftLookbackMs       = 300;
	d.smClutchUpDisengagementDelayMs = 0;
	d.smUpshiftRateThreshold   = 0;
	d.smDownshiftRateThreshold = 0;

	// Limp Mode (Engine State Machine sub-feature) — conservative "get-home" defaults.
	d.limpSeverityThreshold  = 5;    // one latched misfire (5 severity pts) latches limp
	d.limpModeEtbLimit       = 30;   // cap throttle at 30%
	d.limpModeTimingReduction = 5;   // pull 5 deg of timing
	d.limpModeAfrEnrichment  = 5;    // 5% richer than target
	d.limpModeRevLimit       = 3000; // 3000 RPM hard limit
	d.limpModeBoostLimit     = 0;    // 0 = no boost ceiling

	// Misfire Detection (Engine State Machine sub-feature) — disabled by default.
	d.misfireDetectionEnabled = false;
	d.misfireConsecutiveCount = 3;     // 3 consecutive bad firings to arm
	d.misfireCountThreshold   = 50;    // 50 total events latch the MIL
	d.misfireThresholdRatio   = 1.15f; // 15% slower than the cylinder's own baseline
	d.misfireWindowStart      = 20;    // expansion stroke: TDC + 20 deg
	d.misfireWindowEnd        = 120;   //                   TDC + 120 deg

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
	d.popsAndBangsDisableMode = POPS_AND_BANGS_DISABLE_MODE_NONE;
	d.popsAndBangsDisablePin = Gpio::Unassigned;
	d.popsAndBangsDisablePinMode = PI_DEFAULT;
	d.popsAndBangsLuaGauge = LUA_GAUGE_1;
	d.popsAndBangsLuaGaugeMeaning = LUA_GAUGE_LOWER_BOUND;
	d.popsAndBangsLuaGaugeValue = 0.0f;

	// Pops and Bangs spark cut (overlay on top of P&B).
	d.popsAndBangsSparkCutEnabled = false;
	d.popsAndBangsCutDurationAuto = false;
	d.popsAndBangsCutEveryRevs = 4;
	d.popsAndBangsCutPercent = 60;
	d.popsAndBangsCutDurationMs = 100;
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

page5_s* getCustomPage() {
	return &customPageContainer.data;
}

void* customPageGetTsPage() {
	return (void*)&customPageContainer.data;
}

size_t customPageGetTsPageSize() {
	return sizeof(page5_s);
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
