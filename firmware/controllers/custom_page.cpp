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
