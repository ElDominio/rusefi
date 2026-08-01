/**
 * @file frequency_sensor.cpp
 */

#include "pch.h"

#include "frequency_sensor.h"

#include "digital_input_exti.h"

#if EFI_PROD_CODE
// Callback adapter since we can't pass a member function to a callback
static void freqSensorExtiCallback(void* arg, efitick_t nowNt) {
	reinterpret_cast<FrequencySensor*>(arg)->onEdge(nowNt);
}
#endif // EFI_PROD_CODE

void FrequencySensor::initIfValid(brain_pin_e pin, SensorConverter &converter, float filterParameter) {
	if (!isBrainPinValid(pin)) {
		return;
	}

	// Filter parameter greater than or equal to 0.5 impossible as it causes filter instability, clamp
	// far under that value.
	if (filterParameter > 0.35f) {
		filterParameter = 0.35f;
	}

	m_filter.configureLowpass(1, filterParameter);

	setFunction(converter);

#if EFI_PROD_CODE
	if (efiExtiEnablePin(getSensorName(), pin, PAL_EVENT_MODE_FALLING_EDGE,
			freqSensorExtiCallback, reinterpret_cast<void*>(this)) < 0) {
		return;
	}
#endif // EFI_PROD_CODE

	m_pin = pin;

	Register();
}

void FrequencySensor::initShared(SensorConverter &converter, float filterParameter) {
	// Filter parameter greater than or equal to 0.5 impossible as it causes filter instability, clamp
	// far under that value.
	if (filterParameter > 0.35f) {
		filterParameter = 0.35f;
	}

	m_filter.configureLowpass(1, filterParameter);

	setFunction(converter);

	Register();
}

void FrequencySensor::deInit() {
	m_sharedListener = nullptr;

	if (!isBrainPinValid(m_pin)) {
		return;
	}

#if EFI_PROD_CODE
	efiExtiDisablePin(m_pin);
#endif

	m_pin = Gpio::Unassigned;
}

void FrequencySensor::onEdge(efitick_t nowNt) {
    // diagnostics
	eventCounter++;
	float rawFrequency = 1 / m_edgeTimer.getElapsedSecondsAndReset(nowNt);

	if (m_sharedListener) {
		m_sharedListener->onSharedEdge(rawFrequency, nowNt);
	}

	processFrequency(rawFrequency, nowNt);
}

void FrequencySensor::onSharedEdge(float rawFrequency, efitick_t nowNt) {
	eventCounter++;
	processFrequency(rawFrequency, nowNt);
}

void FrequencySensor::processFrequency(float frequency, efitick_t nowNt) {
	if (useBiQuad) {
		frequency = m_filter.filter(frequency);
	}

	postRawValue(frequency, nowNt);
}
