#pragma once

#include "wot_enrichment_state_generated.h"
#include <rusefi/timer.h>

// Number of cells in the elapsed-WOT-time -> AFR adder curve. Must match the array sizes
// declared for wotEnrichmentTimeBins / wotEnrichmentAfrAdder in config_page_5.txt.
#define WOT_ENRICHMENT_SIZE 8

// "WOT Time Enrichment": once the engine is held at Wide Open Throttle, a timer counts the
// elapsed seconds (capped at 30 s) and an 8-cell curve maps that time to an AFR adder which
// is added to the target AFR (negative values richen). The timer and adder reset the moment
// we drop out of WOT. WOT state is read from the Engine State Machine (engineSmIsWot).
class WotEnrichment : public wot_enrichment_state_s, public EngineModule {
public:
    using interface_t = WotEnrichment;

    void onSlowCallback() override;

    // AFR adder currently applied to the target AFR (negative = richer). 0 when inactive.
    float getAfrAdder() const;

private:
    // Reset on every non-WOT callback, so the first WOT callback reads ~0 s: the timer
    // effectively starts when we enter WOT.
    Timer m_wotTimer;
};
