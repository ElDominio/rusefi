#pragma once

#include <rusefi/timer.h>

/**
 * First-order thermal model estimating engine oil temperature (EOT) from cylinder head
 * temperature (CHT), intake air temperature (IAT), and oil pressure.
 *
 * Oil pressure is a proxy for oil flow rate: more flow means more convective heat transfer
 * between the hot metal (head/bearings) and the oil, so higher pressure pulls the estimate
 * closer to CHT (shrinks the delta). CHT and oil pressure are both required inputs; if either
 * is invalid, EOT falls back to a fixed configured value.
 *
 * Steady-state delta: ΔT_target = k0 + k1*CHT + k2*IAT + k3*oilPressure
 * Integrated state  : ΔT_actual tracks ΔT_target with separate heating/cooling time factors.
 * Estimated EOT     : CHT - ΔT_actual, clamped to [IAT, 350F]. Oil also picks up heat from the
 *                      crank/rod bearings and other internals, so it's allowed to run hotter
 *                      than CHT (ΔT_actual can go negative) up to the fixed absolute ceiling.
 *
 * With no oil circulation (RPM or oil pressure at zero) the CHT-coupled steady state above
 * doesn't apply: there's no flow to carry heat into the oil, so the estimate is only allowed
 * to fall toward ambient, never rise back toward CHT, even if CHT itself ticks up (e.g. head
 * heat-soak right after shutdown).
 *
 * Config knobs (all in engineConfiguration):
 *   eotEstK0        base delta at zero CHT, IAT and oil pressure   [°C]
 *   eotEstK1        CHT coefficient                                [°C/°C]
 *   eotEstK2        IAT coefficient                                [°C/°C]
 *   eotEstK3        oil pressure coefficient (typically <= 0)      [°C/kPa]
 *   eotEstTauHeat   heating time factor                            [s]
 *   eotEstTauCool   cooling time factor (usually longer)           [s]
 *   eotEstFallbackEot  EOT to report when CHT or oil pressure is invalid  [°C]
 */
struct EotEstimator {
	void init();
	void update();

	float deltaTActual = 0;
	float deltaTTarget = 0;

private:
	efitick_t m_lastUpdateNt = 0;
	float m_lastEot = 0;
};

void initEotEstimator();
void updateEotEstimator();
const EotEstimator& getEotEstimator();
