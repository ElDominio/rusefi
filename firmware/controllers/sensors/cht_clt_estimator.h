#pragma once

#include <rusefi/timer.h>

/**
 * First-order thermal model estimating coolant temperature from cylinder head temperature.
 *
 * Steady-state delta: ΔT_target = k0 + k1*CHT + k2*load
 * Integrated state  : ΔT_actual tracks ΔT_target with separate heating/cooling time factors.
 * Estimated CLT     : CHT - ΔT_actual, clamped to [IAT, CHT].
 *
 * Config knobs (all in engineConfiguration):
 *   chtEstK0        base delta at zero load and zero CHT  [°C]
 *   chtEstK1        CHT coefficient                       [°C/°C]
 *   chtEstK2        engine-load coefficient               [°C/%]
 *   chtEstTauHeat   heating time factor                   [s]
 *   chtEstTauCool   cooling time factor (usually longer)  [s]
 *   chtEstFallbackClt  CLT to report when CHT is invalid  [°C]
 */
struct ChtCltEstimator {
	void init();
	void update();

	float deltaTActual = 0;
	float deltaTTarget = 0;

private:
	efitick_t m_lastUpdateNt = 0;
};

void initChtCltEstimator();
void updateChtCltEstimator();
const ChtCltEstimator& getChtCltEstimator();
