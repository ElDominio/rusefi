#include "pch.h"
#include "wot_enrichment.h"

void WotEnrichment::setDefaultConfiguration() {
	engineConfiguration->wotEnrichmentEnabled = false;
	for (size_t i = 0; i < WOT_ENRICHMENT_SIZE; i++) {
		engineConfiguration->wotEnrichmentTimeBins[i] = 30.0f * i / (WOT_ENRICHMENT_SIZE - 1);
	}
	for (auto& adder : engineConfiguration->wotEnrichmentAfrAdder) {
		adder = 0;
	}
}

#if EFI_WOT_ENRICHMENT // [tag:alphax_compile_out]

void WotEnrichment::reset() {
	m_wotTimer.reset();
	isWotEnrichmentActive = false;
	wotEnrichmentTimer = 0;
	wotEnrichmentAfrAdder = 0;
}

void WotEnrichment::onEngineStop() {
	reset();
}

void WotEnrichment::onConfigurationChange(engine_configuration_s const*) {
	reset();
}

void WotEnrichment::onSlowCallback() {
	auto& sm = engine->module<EngineStateMachine>().unmock();
	if (!engineConfiguration->wotEnrichmentEnabled || !sm.isEnabled() || !sm.engineSmIsWot) {
		reset();
		return;
	}

	// Start at the first WOT sample, excluding the preceding non-WOT interval.
	if (!isWotEnrichmentActive) {
		m_wotTimer.reset();
	}
	float elapsed = clampF(0, m_wotTimer.getElapsedSeconds(), 30);
	wotEnrichmentTimer = elapsed;
	wotEnrichmentAfrAdder = interpolate2d(elapsed,
		engineConfiguration->wotEnrichmentTimeBins, engineConfiguration->wotEnrichmentAfrAdder);
	isWotEnrichmentActive = true;
}

float WotEnrichment::getAfrAdder() const {
	return isWotEnrichmentActive ? wotEnrichmentAfrAdder : 0;
}

#else // !EFI_WOT_ENRICHMENT [tag:alphax_compile_out]

// WOT enrichment compiled out (board set -DEFI_WOT_ENRICHMENT=FALSE). The module stays in the module
// list so consumers, live data and Lua lookups still link; it never applies an adder. The settings
// and their defaults above stay in the configuration layout.
void WotEnrichment::reset() {}
void WotEnrichment::onEngineStop() {}
void WotEnrichment::onConfigurationChange(engine_configuration_s const*) {}
void WotEnrichment::onSlowCallback() { isWotEnrichmentActive = false; }
float WotEnrichment::getAfrAdder() const { return 0; }

#endif // EFI_WOT_ENRICHMENT
