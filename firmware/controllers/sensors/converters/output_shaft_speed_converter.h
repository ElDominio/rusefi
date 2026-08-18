#include "pch.h"
#include "sensor_converter_func.h"
#include "custom_page.h"
#include "wheel_speed_plausibility_filter.h"

class OutputShaftSpeedConverter : public SensorConverter {
public:
	SensorResult convert(float frequency) const override {
		float rpm = frequency * 60 / getCustomPage()->outputShaftSpeedSensorTeeth;

		float dt = frequency > 0 ? clampF(0.001f, 1 / frequency, 1) : 1;
		return m_filter.apply(rpm, dt);
	}

private:
	WheelSpeedPlausibilityFilter m_filter;
};
