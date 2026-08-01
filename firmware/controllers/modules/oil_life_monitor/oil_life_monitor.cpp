#include "pch.h"
#include "custom_page.h"
#include "oil_life_monitor.h"

#if EFI_OIL_LIFE_MONITOR && !EFI_MAIN_RELAY_CONTROL
#error "EFI_OIL_LIFE_MONITOR requires EFI_MAIN_RELAY_CONTROL: without it there is no reliable " \
       "signal to hold ECU power open long enough to flush the oil-life counter to flash."
#endif

#if EFI_OIL_LIFE_MONITOR

#include "storage.h"
#include "extra_flash_pages.h"

// Zone thresholds are fixed in firmware; only the multipliers below (config_page_6.txt) are tunable.
static constexpr float OIL_LIFE_ZONE_COLD_MAX = 70.0f;
static constexpr float OIL_LIFE_ZONE_OPTIMAL_MAX = 105.0f;
static constexpr float OIL_LIFE_ZONE_HIGH_HEAT_MAX = 125.0f;

// Persisted record: a single counter, wrapped in the standard CRC+version container so a blank
// or corrupted flash region (read as all-1s) is detected rather than misread as a huge bogus
// weightedRevs value. sizeof(OilLifeContainer) is guaranteed a multiple of 32 by alignas(32) on
// ExtraPageContainer, and is reserved unconditionally in extra_flash_pages.cpp regardless of
// whether this feature is compiled in, so the flash layout never shifts across builds.
struct OilLifeStorageData {
	uint32_t weightedRevs;
};
static constexpr uint32_t OIL_LIFE_DATA_VERSION = 1;
using OilLifeContainer = ExtraPageContainer<OilLifeStorageData, OIL_LIFE_DATA_VERSION>;
static_assert(sizeof(OilLifeContainer) % 32 == 0,
	"OilLifeContainer must be 32-byte aligned for STM32H7 flash writes");

void OilLifeMonitor::init() {
#if EFI_PROD_CODE
	m_loadPending = storageReqestReadID(EFI_OIL_LIFE_RECORD_ID);
#else
	m_loadPending = false;
	m_weightedRevs = 0;
	m_lastRevCounter = getRevolutionCounter();
#endif
}

void OilLifeMonitor::onSlowCallback() {
	if (!getCustomPage()->oilLifeMonitorEnabled || m_loadPending) {
		return;
	}

	uint32_t nowRevCounter = getRevolutionCounter();
	uint32_t delta = nowRevCounter - m_lastRevCounter;
	m_lastRevCounter = nowRevCounter;

	if (delta == 0) {
		// Engine not turning: nothing to weight, and no point re-reading temperature.
		return;
	}

	// Primary source is user-selectable; the *other* sensor is used whenever the selected one is
	// invalid. Either may be a real sensor or an estimator transparently publishing under the
	// same SensorType (e.g. EOT/CLT-from-CHT on a board with no physical sensor) -- this code
	// can't tell the difference and doesn't need to.
	bool preferOil = getCustomPage()->oilLifePrimarySource == oil_life_temp_source_e::OilTemp;
	SensorType primaryType = preferOil ? SensorType::OilTemperature : SensorType::Clt;
	SensorType secondaryType = preferOil ? SensorType::Clt : SensorType::OilTemperature;

	auto primary = Sensor::get(primaryType);
	float tempC;
	if (primary) {
		tempC = primary.Value;
		m_tempSource = preferOil ? TempSource::Oil : TempSource::Coolant;
	} else {
		tempC = Sensor::getOrZero(secondaryType);
		m_tempSource = preferOil ? TempSource::Coolant : TempSource::Oil;
	}

	float multiplier = getZoneMultiplier(tempC, m_tempSource == TempSource::Coolant);
	uint32_t weightedDelta = (uint32_t)(delta * multiplier);

	m_weightedRevs += weightedDelta;
	m_weightedRevsThisDrive += weightedDelta;
}

void OilLifeMonitor::onIgnitionStateChanged(bool ignitionOn) {
	if (ignitionOn) {
		// New drive cycle: start the per-drive usage counter over.
		m_weightedRevsThisDrive = 0;
	} else {
		requestFlush();
	}
}

bool OilLifeMonitor::needsDelayedShutoff() {
	return m_savePending;
}

void OilLifeMonitor::requestFlush() {
	// Marks the save as pending immediately so needsDelayedShutoff() holds the main relay open
	// from the moment a flush is requested, not just once the (EFI_PROD_CODE-only) storage
	// manager thread picks it up. store() is what actually clears this flag.
	m_savePending = true;
#if EFI_PROD_CODE
	storageRequestWriteID(EFI_OIL_LIFE_RECORD_ID, /*forced*/ true);
#endif
}

void OilLifeMonitor::reset() {
	m_weightedRevs = 0;
	requestFlush();
}

// Called from storage manager thread when the requested read completes.
void OilLifeMonitor::load() {
#if EFI_PROD_CODE
	OilLifeContainer container;
	if (storageRead(EFI_OIL_LIFE_RECORD_ID, (uint8_t *)&container, sizeof(container)) == StorageStatus::Ok
			&& container.isValid()) {
		m_weightedRevs = container.data.weightedRevs;
	} else {
		// Blank/erased flash (first boot with this feature) or a corrupted/version-mismatched
		// record -- fail open at 100% life rather than trust garbage bytes.
		m_weightedRevs = 0;
	}
#endif // EFI_PROD_CODE

	m_loadPending = false;
	// Re-seed so we don't apply one huge bogus delta from the boot-time counter value.
	m_lastRevCounter = getRevolutionCounter();
}

// Called on the storage manager thread (directly for MFS/SD, or via the INT_FLASH shared-sector
// burn sequence -- see oilLifeMonitorBurnToFlash() / extra_flash_pages.cpp) when a write completes.
void OilLifeMonitor::store() {
#if EFI_PROD_CODE
	OilLifeContainer container;
	container.data.weightedRevs = m_weightedRevs;
	container.prepareForStorage();
	storageWrite(EFI_OIL_LIFE_RECORD_ID, (const uint8_t *)&container, sizeof(container));
#endif // EFI_PROD_CODE

	m_savePending = false;
}

float OilLifeMonitor::getOilLifePercent() const {
	return computeOilLifePercent(m_weightedRevs, getCustomPage()->oilLifeRevsScaleMillions);
}

uint8_t OilLifeMonitor::getTempSourceForOutput() const {
	return (uint8_t)m_tempSource;
}

uint32_t OilLifeMonitor::getWeightedRevsThisDrive() const {
	return m_weightedRevsThisDrive;
}

float OilLifeMonitor::computeOilLifePercent(uint32_t weightedRevs, uint8_t scaleMillions) {
	if (scaleMillions == 0) {
		// Fail open: unreachable via the TS UI (field range is [1,100]), but a corrupt or
		// freshly-erased page-6 blob could read zero -- don't report a divide-by-zero result.
		return 100.0f;
	}

	float maxRevs = scaleMillions * 1'000'000.0f;
	return clampF(0.0f, (1.0f - weightedRevs / maxRevs) * 100.0f, 100.0f);
}

float OilLifeMonitor::getZoneMultiplier(float tempC, bool isCoolant) {
	auto cfg = getCustomPage();

	if (tempC < OIL_LIFE_ZONE_COLD_MAX) {
		return isCoolant ? cfg->oilLifeMultCoolantCold : cfg->oilLifeMultOilCold;
	} else if (tempC < OIL_LIFE_ZONE_OPTIMAL_MAX) {
		return isCoolant ? cfg->oilLifeMultCoolantOptimal : cfg->oilLifeMultOilOptimal;
	} else if (tempC < OIL_LIFE_ZONE_HIGH_HEAT_MAX) {
		return isCoolant ? cfg->oilLifeMultCoolantHighHeat : cfg->oilLifeMultOilHighHeat;
	} else {
		return isCoolant ? cfg->oilLifeMultCoolantExtreme : cfg->oilLifeMultOilExtreme;
	}
}

void initOilLifeMonitor() {
	engine->module<OilLifeMonitor>()->init();
}

void resetOilLifeMonitor() {
	engine->module<OilLifeMonitor>()->reset();
}

void oilLifeMonitorBurnToFlash() {
	engine->module<OilLifeMonitor>()->store();
}

#else // !EFI_OIL_LIFE_MONITOR

// Oil Life Monitor compiled out (board set -DEFI_OIL_LIFE_MONITOR=FALSE).
void OilLifeMonitor::init() { }
void OilLifeMonitor::onSlowCallback() { }
void OilLifeMonitor::onIgnitionStateChanged(bool) { }
bool OilLifeMonitor::needsDelayedShutoff() { return false; }
void OilLifeMonitor::requestFlush() { }
void OilLifeMonitor::reset() { }
void OilLifeMonitor::load() { }
void OilLifeMonitor::store() { }
float OilLifeMonitor::getOilLifePercent() const { return 100.0f; }
uint8_t OilLifeMonitor::getTempSourceForOutput() const { return 0; }
uint32_t OilLifeMonitor::getWeightedRevsThisDrive() const { return 0; }
float OilLifeMonitor::computeOilLifePercent(uint32_t, uint8_t) { return 100.0f; }
float OilLifeMonitor::getZoneMultiplier(float, bool) { return 1.0f; }

void initOilLifeMonitor() { }
void resetOilLifeMonitor() { }
void oilLifeMonitorBurnToFlash() { }

#endif // EFI_OIL_LIFE_MONITOR
