#pragma once

#include "burst_knock_state_generated.h"
#include <rusefi/timer.h>

// Table dimensions for the transient timing-pull map. Must match the array sizes declared
// for burstKnockRpmBins / burstKnockTpsRateBins / burstKnockRetardTable in config_page_6.txt.
#define BURST_KNOCK_RPM_SIZE 8
#define BURST_KNOCK_TPS_RATE_SIZE 8

// "Burst Knock": on a TPS-rate transient (throttle stab) pull a chunk of ignition advance per a
// TPS-rate(Y) x RPM(X) table, then decay that retard linearly back to zero. This is the timing-side
// mirror of the TPS/TPS acceleration fuel enrichment: it reuses the TpsAccelEnrichment delta/threshold
// as its trigger, but removes timing instead of adding fuel. Modeled on LaunchPowerRamp.
class BurstKnock : public burst_knock_state_s, public EngineModule {
public:
    using interface_t = BurstKnock;

    void onSlowCallback() override;

    // Ignition timing (degrees) currently being pulled by the burst. 0 when inactive.
    float getTimingRetard() const;

private:
    Timer m_decayTimer;
    // Peak retard latched at the height of the transient; the linear decay ramps down from here.
    float m_peak = 0;
};
