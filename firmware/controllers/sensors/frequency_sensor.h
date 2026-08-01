/**
 * @file frequency_sensor.h
 */
#include "functional_sensor.h"
#include <rusefi/timer.h>
#include "biquad.h"

class FrequencySensor : public FunctionalSensor {
public:
	FrequencySensor(SensorType type, efidur_t timeoutPeriod)
		: FunctionalSensor(type, timeoutPeriod)
	{ }

	void initIfValid(brain_pin_e pin, SensorConverter &converter, float filterParameter);
	// Initializes this sensor to derive its readings from another FrequencySensor's raw edge
	// frequency instead of its own EXTI pin (e.g. "shared with main VSS" - same physical sensor,
	// two logical readings). The STM32 EXTI layer only supports one callback per physical pin,
	// so this avoids registering a second one on a pin already owned by `source`.
	void initShared(SensorConverter &converter, float filterParameter);
	void deInit();

	// Attaches (or detaches with nullptr) a listener that will be fed this sensor's raw edge
	// frequency via onSharedEdge() whenever it is initialized with initShared().
	void setSharedListener(FrequencySensor* listener) { m_sharedListener = listener; }

	// sad workaround: we are not good at BiQuad configuring
	bool useBiQuad = true;

	void showInfo(const char* sensorName) const override;

	void onEdge(efitick_t nowNt);

	int eventCounter = 0;
private:
	void onSharedEdge(float rawFrequency, efitick_t nowNt);
	void processFrequency(float frequency, efitick_t nowNt);

	Timer m_edgeTimer;
	brain_pin_e m_pin = Gpio::Unassigned;

	Biquad m_filter;

	FrequencySensor* m_sharedListener = nullptr;
};
