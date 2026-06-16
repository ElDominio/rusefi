#include "pch.h"
#include "custom_page.h"
#include "wot_enrichment.h"
#include "engine_state_machine.h"

#if EFI_WOT_ENRICHMENT

void WotEnrichment::onSlowCallback() {
    auto cfg = getCustomPage();

    bool wot = engine->module<EngineStateMachine>().unmock().engineSmIsWot;

    if (!cfg->wotEnrichmentEnabled || !wot) {
        // Not at WOT (or disabled): keep the timer pinned at zero so it restarts on WOT entry.
        m_wotTimer.reset();
        isWotEnrichmentActive = false;
        wotEnrichmentTimer = 0;
        wotEnrichmentAfrAdder = 0;
        return;
    }

    // Held at WOT: advance the timer (capped at 30 s) and look up the AFR adder.
    float elapsed = clampF(0, m_wotTimer.getElapsedSeconds(), 30);
    wotEnrichmentTimer = elapsed;
    wotEnrichmentAfrAdder = interpolate2d(elapsed,
        cfg->wotEnrichmentTimeBins, cfg->wotEnrichmentAfrAdder);
    isWotEnrichmentActive = true;
}

float WotEnrichment::getAfrAdder() const {
    return isWotEnrichmentActive ? wotEnrichmentAfrAdder : 0;
}

#else // !EFI_WOT_ENRICHMENT

// WOT enrichment compiled out (board set -DEFI_WOT_ENRICHMENT=FALSE).
void WotEnrichment::onSlowCallback() { isWotEnrichmentActive = false; }
float WotEnrichment::getAfrAdder() const { return 0; }

#endif // EFI_WOT_ENRICHMENT
