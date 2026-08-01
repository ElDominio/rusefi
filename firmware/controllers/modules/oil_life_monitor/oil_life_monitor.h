#pragma once

// "Weighted Engine Oil Life Monitor": tracks a temperature-weighted cumulative engine-revolution
// counter and reports remaining oil life as a percentage. The primary temperature source
// (SensorType::OilTemperature or SensorType::Clt) is user-selectable via oilLifePrimarySource;
// the *other* one is used as a fallback whenever the selected source is invalid. Either source
// may itself be a real sensor or an estimated value transparently published under the same
// SensorType (e.g. the EOT/CLT estimators on boards with no physical sensor) -- this module has
// no way to tell the difference and doesn't need to. Zone thresholds (70/105/125 C) are fixed in
// firmware; only the per-zone multipliers are TS-tunable.
//
// Accumulates in RAM only for the whole time the ECU is powered -- there is no periodic flash
// write. The counter is written to flash exactly once, on ignition-off, via needsDelayedShutoff()
// holding the main relay open until the write completes. This is why the feature requires
// EFI_MAIN_RELAY_CONTROL: without it there is no reliable shutdown signal to hold power open for.
class OilLifeMonitor : public EngineModule {
public:
	using interface_t = OilLifeMonitor;

	void init();
	void onSlowCallback() override;
	void onIgnitionStateChanged(bool ignitionOn) override;
	bool needsDelayedShutoff() override;

	// Resets the accumulated weighted-revolution counter back to zero (100% oil life) and
	// immediately requests a flash flush so the reset survives a power cycle right away.
	void reset();

	// Called by the storage manager thread once the requested read/write completes.
	void load();
	void store();

	float getOilLifePercent() const;
	uint8_t getTempSourceForOutput() const;

	// Weighted revs consumed since this drive cycle began (ignition-on). Not persisted, not
	// affected by reset() -- purely a per-drive-cycle usage indicator.
	uint32_t getWeightedRevsThisDrive() const;

	// Pure, unit-testable helpers.
	static float computeOilLifePercent(uint32_t weightedRevs, uint8_t scaleMillions);
	static float getZoneMultiplier(float tempC, bool isCoolantFallback);

private:
	enum class TempSource : uint8_t { Oil = 0, Coolant = 1 };

	void requestFlush();

	uint32_t m_weightedRevs = 0;
	uint32_t m_weightedRevsThisDrive = 0;
	uint32_t m_lastRevCounter = 0;
	bool m_savePending = false;
	bool m_loadPending = false;
	TempSource m_tempSource = TempSource::Oil;
};

void initOilLifeMonitor();
void resetOilLifeMonitor();

// Writes the accumulated counter to flash. Free function (rather than requiring callers to know
// about the OilLifeMonitor class) so extra_flash_pages.cpp -- which is compiled unconditionally,
// regardless of EFI_OIL_LIFE_MONITOR -- can piggyback the write on its INT_FLASH shared-sector
// burn sequence (see burnExtraFlashPage()/burnExtraFlashPages()) without needing to reference
// engine->module<OilLifeMonitor>() directly. No-op when the feature is compiled out.
void oilLifeMonitorBurnToFlash();
