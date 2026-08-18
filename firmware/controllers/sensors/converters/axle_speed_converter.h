#include "pch.h"
#include "sensor_converter_func.h"
#include "custom_page.h"
#include "wheel_speed_plausibility_filter.h"

// Serves both Front and Rear Axle speed sensors (isFront picks which page-6 fields to read),
// converting the raw pulse frequency to km/h via that axle's own tooth count and wheel
// revolutions-per-km (tire size), same formula shape as vehicle_speed_converter.h. Physical Pin
// mode only -- CAN/Lua mode never instantiates a FrequencySensor, so this converter isn't used.
class AxleSpeedConverter : public SensorConverter {
public:
	explicit AxleSpeedConverter(bool isFront) : m_isFront(isFront) {}

	SensorResult convert(float frequency) const override {
		auto cfg = getCustomPage();
		uint8_t teeth = m_isFront ? cfg->wheelSpeedFrontTeeth : cfg->wheelSpeedRearTeeth;
		float revPerKm = m_isFront ? cfg->wheelSpeedFrontRevPerKm : cfg->wheelSpeedRearRevPerKm;

		auto pulsePerKm = revPerKm * teeth;

		if (pulsePerKm == 0) {
			// avoid div by 0
			return 0;
		}

		auto kmPerPulse = 1 / pulsePerKm;

		//     1 pulse       3600 sec      1 km       km
		//    ---------  *  ---------- * --------- = ----
		//       sec           1 hr       1 pulse     hr
		float speedKmh = frequency * 3600 * kmPerPulse;

		float dt = frequency > 0 ? clampF(0.001f, 1 / frequency, 1) : 1;
		return m_filter.apply(speedKmh, dt);
	}

private:
	bool m_isFront;
	WheelSpeedPlausibilityFilter m_filter;
};
