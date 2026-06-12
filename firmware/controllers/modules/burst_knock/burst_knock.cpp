#include "pch.h"
#include "custom_page.h"
#include "burst_knock.h"
#include "accel_enrichment.h"

#if EFI_BURST_KNOCK

void BurstKnock::onSlowCallback() {
    auto cfg = getCustomPage();

    if (!cfg->burstKnockEnabled) {
        isBurstKnockActive = false;
        burstKnockRetard = 0;
        burstKnockTpsRate = 0;
        m_peak = 0;
        return;
    }

    // Reuse the fuel accel-enrichment delta so the burst fires in lockstep with the accel pump.
    auto& ae = engine->module<TpsAccelEnrichment>().unmock();

    // deltaTps is the largest TPS jump seen across the lookback window; express it as a rate (%/s)
    // for the table Y axis. tpsAccelLookback is in seconds.
    float lookback = maxF(engineConfiguration->tpsAccelLookback, 0.001f);
    float tpsRate = ae.deltaTps / lookback;
    burstKnockTpsRate = tpsRate;

    float rpm = Sensor::getOrZero(SensorType::Rpm);

    if (ae.isAboveAccelThreshold) {
        // Transient in progress: hold the table value and keep the decay timer pinned at zero.
        float retard = interpolate3d(cfg->burstKnockRetardTable,
            cfg->burstKnockTpsRateBins, tpsRate,
            cfg->burstKnockRpmBins, rpm);
        // Retard only: never let the table advance timing.
        m_peak = retard < 0 ? 0 : retard;
        burstKnockRetard = m_peak;
        isBurstKnockActive = m_peak > 0;
        m_decayTimer.reset();
    } else if (isBurstKnockActive) {
        // Transient over: decay the latched peak linearly to zero over burstKnockDecayTime.
        float decayTime = cfg->burstKnockDecayTime;
        float elapsed = m_decayTimer.getElapsedSeconds();
        if (decayTime <= 0 || elapsed >= decayTime) {
            isBurstKnockActive = false;
            burstKnockRetard = 0;
            m_peak = 0;
        } else {
            burstKnockRetard = m_peak * (1 - elapsed / decayTime);
        }
    } else {
        burstKnockRetard = 0;
    }
}

float BurstKnock::getTimingRetard() const {
    return isBurstKnockActive ? burstKnockRetard : 0;
}

#else // !EFI_BURST_KNOCK

// Burst Knock compiled out (board set -DEFI_BURST_KNOCK=FALSE).
void BurstKnock::onSlowCallback() { isBurstKnockActive = false; }
float BurstKnock::getTimingRetard() const { return 0; }

#endif // EFI_BURST_KNOCK
