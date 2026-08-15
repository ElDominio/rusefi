/*
 * @file prime_injection.h

 */

#pragma once

#include "engine_module.h"
#include "rusefi_types.h"
#include "scheduler.h"
#include "prime_injection_generated.h"

class PrimeController : public EngineModule, public prime_injection_s {
public:
	void onIgnitionStateChanged(bool ignitionOn) override;
	void onSlowCallback() override;

	floatms_t getPrimeDuration() const;

	void onPrimeStart();
	void onPrimeEnd();

	// Called by TriggerCentral for every accepted primary trigger tooth. Only does
	// anything while a tooth-counted prime is armed (see primeOnTriggerTeeth).
	void onPrimeTriggerTooth();

	bool isPriming() const {
		return m_isPriming;
	}

private:

	static void onPrimeStartAdapter(PrimeController* instance) {
		instance->onPrimeStart();
	}

	static void onPrimeEndAdapter(PrimeController* instance) {
		instance->onPrimeEnd();
	}

	uint32_t getKeyCycleCounter() const;
	void setKeyCycleCounter(uint32_t count);

	// Tooth-counted prime arming state. Not persisted: reset every ignition cycle.
	bool m_primeTriggerArmed = false;
	uint32_t m_primeTriggerTeethSeen = 0;
};
