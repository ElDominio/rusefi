#include "pch.h"

#include "engine_configuration.h"
#include "sensor.h"
#include "error_handling.h"
#include "efi_interpolation.h"
#include "table_helper.h"
#include "fuel_math.h"
#include "fuel_computer.h"
#include "engine_state_machine.h"
#include "custom_page.h"
#if EFI_WOT_ENRICHMENT
#include "wot_enrichment.h"
#endif // EFI_WOT_ENRICHMENT

#if EFI_ENGINE_CONTROL

mass_t FuelComputerBase::getCycleFuel(mass_t airmass, float rpm, float load) {
	load = getTargetLambdaLoadAxis(load);

	float stoich = getStoichiometricRatio();
	float lambda = getTargetLambda(rpm, load);

	// Limp Mode AFR enrichment: richen the target (lower lambda) while limp is latched.
	if (engine->module<EngineStateMachine>().unmock().engineSmIsLimp) {
		lambda *= (1.0f - getCustomPage()->limpModeAfrEnrichment * 0.01f);
	}

	float afr = stoich * lambda;

#if EFI_WOT_ENRICHMENT
	// WOT time enrichment: add an AFR adder (negative = richer) after prolonged WOT.
	afr += engine->module<WotEnrichment>().unmock().getAfrAdder();
	lambda = afr / stoich;   // keep published targetLambda consistent with the adjusted AFR
#endif // EFI_WOT_ENRICHMENT

	// Eco Mode: lean the mixture to an absolute target AFR while the economy overlay is active.
	if (engine->module<EngineStateMachine>().unmock().engineSmIsEcoMode) {
		afr = getCustomPage()->ecoTargetAfr;
		lambda = afr / stoich; // keep published targetLambda consistent with the eco target
	}

	// Quick Warmup: override lambda target when cold + idle to aid catalyst light-off.
	if (engine->module<EngineStateMachine>().unmock().engineSmIsQuickWarmup) {
		lambda = getCustomPage()->quickWarmupLambdaTarget;
		afr = lambda * stoich;
	}

	afrTableYAxis = load;
	targetLambda = lambda;
	targetAFR = afr;
	stoichiometricRatio = stoich;

	return airmass / afr;
}

float FuelComputer::getStoichiometricRatio() const {
	float primary = engineConfiguration->stoichRatioPrimary;


	if (primary < 5) {
   	// Config compatibility: this field may be zero on ECUs with very old defaults
		criticalError("Please set stoichRatioPrimary");
		return 0;
	}

	// Without an ethanol/flex sensor, return primary configured stoich ratio
	if (!Sensor::hasSensor(SensorType::FuelEthanolPercent)) {
		return primary;
	}

	float secondary = engineConfiguration->stoichRatioSecondary;

	if (secondary < 5) {
	  // Config compatibility: this field may be zero on ECUs with very old defaults
		criticalError("Please set stoichRatioSecondary");
		return 0;
	}

	auto flex = Sensor::get(SensorType::FuelEthanolPercent);

	// TODO: what do do if flex sensor fails?

	// Linear interpolate between primary and secondary stoich ratios
	return interpolateClamped(0, primary, 100, secondary, flex.Value);
}


float FuelComputer::getTargetLambda(float rpm, float load) const {
	float target = interpolate3d(
		config->lambdaTable,
		config->lambdaLoadBins, load,
		config->lambdaRpmBins, rpm
	);

	// Add any blends if configured
	for (size_t i = 0; i < efi::size(config->targetAfrBlends); i++) {
		auto result = calculateBlend(config->targetAfrBlends[i], rpm, load);

		engine->outputChannels.targetAfrBlendParameter[i] = result.BlendParameter;
		engine->outputChannels.targetAfrBlendBias[i] = result.Bias;
		engine->outputChannels.targetAfrBlendOutput[i] = result.Value;
		engine->outputChannels.targetAfrBlendYAxis[i] = result.TableYAxis;

		target += result.Value;
	}

    return target;
}

float FuelComputer::getTargetLambdaLoadAxis(float defaultLoad) const {
	return getLoadOverride(defaultLoad, engineConfiguration->afrOverrideMode);
}

float IFuelComputer::getLoadOverride(float defaultLoad, load_override_e overrideMode) const {
	switch(overrideMode) {
		case AFR_None: return defaultLoad;
		// MAP default to 200kpa - failed MAP goes rich
		case AFR_MAP: return Sensor::get(SensorType::Map).value_or(200);
		// TPS/pedal default to 100% - failed TPS goes rich
		case AFR_Tps: return Sensor::get(SensorType::Tps1).value_or(100);
		case AFR_AccPedal: return Sensor::get(SensorType::AcceleratorPedal).value_or(100);
		case AFR_CylFilling: return normalizedCylinderFilling;
		default: return 0;
	}
}

#endif // EFI_ENGINE_CONTROL
