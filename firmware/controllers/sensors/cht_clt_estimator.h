#pragma once

#include <rusefi/timer.h>

/**
 * Competing-rate thermal model estimating coolant temperature (CLT) from cylinder head
 * temperature (CHT), vehicle speed (VSS), cooling fan state, and IAT/ambient. A lagged
 * thermostat valve state reproduces the real crossing/dip/recover "hunt" around the
 * thermostat's regulation point instead of a smooth monotonic approach.
 *
 * dCLT/dt = R_heat(CHT) * (CHT - CLT) - R_reject(laggedAirflow) * iatFactor(IAT) * valvePos * (CLT - thermostatTemp)
 *
 *   iatFactor(IAT) = clamp(0, 1.0 + cltEstRadiatorEffectivenessIatGain * (IAT - cltEstRadiatorReferenceTemp), 4)
 *                  -- radiator effectiveness multiplier; always 1.0 (no change) exactly at the
 *                     user-chosen reference air temp, scaled up/down from there by the gain.
 *
 *   R_heat(CHT)       = clamp01(cltEstHeadTransferRate + cltEstHeadTransferGain * (CHT - 90))
 *                     -- base rate at a 90 deg C reference CHT, plus a gain for how much faster
 *                        that transfer happens as CHT climbs above (or below) the reference.
 *   R_reject(airflow) = clamp01(cltEstRadiatorBaseRejection + cltEstRadiatorAirflowGain * airflow)
 *                     -- base rate at zero airflow, plus a gain per CFM through the radiator.
 *                        Both are a straight line rather than a curve -- simpler for tuners to
 *                        reason about, at the cost of not capturing any real nonlinearity.
 *
 *   airflow       = (fan1 on ? cltEstFan1Cfm : 0) + (fan2 on ? cltEstFan2Cfm : 0)
 *                 + interpolate2d(VSS, cltEstVssBins, cltEstVssAirflow)
 *   laggedAirflow tracks airflow with time constant cltEstCoolantLag (seconds) -- a step
 *                 change in fan/ram-air airflow doesn't instantly change how fast the bulk
 *                 coolant mass gives up heat, since that heat has to physically circulate
 *                 through the block/radiator/hoses first. Zero means airflow acts instantly.
 *
 *   valveTarget = clamp01( ((CLT - thermostatTemp) / cltEstThermostatSpread) ^ 3 )
 *   valvePos    tracks valveTarget with time constant cltEstThermostatLag (seconds)
 *
 * R_heat and R_reject are both rate constants [1/s]. CHT constantly pulls CLT up toward
 * itself; radiator airflow pulls CLT down toward the thermostat's regulation point, but only
 * once the (lagged) valve has opened. The cubic valveTarget is ~flat near thermostatTemp (the
 * valve barely moves right at the setpoint) and ramps toward fully open/closed away from it;
 * it is naturally zero (fully closed) for any CLT below thermostatTemp, since a negative value
 * cubed stays negative and clamps to zero. The valve's own response lag is what allows CLT to
 * overshoot past thermostatTemp before rejection catches up and pulls it back down again.
 *
 * cltEstThermostatLag and cltEstCoolantLag model two different physical delays and are
 * independently tunable: the former is the wax-pellet valve's own mechanical response time,
 * the latter is the coolant's thermal-mass/circulation lag in responding to airflow changes.
 *
 * VSS is a soft dependency: an invalid VSS reading degrades to zero ram-air contribution
 * rather than faulting. CHT is a hard dependency: if invalid, the estimate falls back to
 * cltEstFallbackClt.
 *
 * Radiator-fail safety override: at or above cltEstRadiatorFailTemp, the radiator is assumed
 * to provide zero effective cooling (boil-over, coolant/pump loss) and the estimate hard-locks
 * to CHT directly, bypassing the ODE so there's no lag before the true severity is reported.
 *
 * Config lives on TS page 6 (page6_s), gated by EFI_CHT_CLT_ESTIMATOR.
 */
struct ChtCltEstimator {
	void init();
	void update();

	// Debug/live state, also published to output channels.
	float airflow = 0;
	float rejectionRate = 0;
	float valvePos = 0;

private:
	float m_clt = 0;
	float m_laggedAirflow = 0;
	bool m_initialized = false;
	efitick_t m_lastUpdateNt = 0;
};

void initChtCltEstimator();
void updateChtCltEstimator();
const ChtCltEstimator& getChtCltEstimator();
