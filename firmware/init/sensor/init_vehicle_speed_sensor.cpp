#include "pch.h"

#include "init.h"
#include "custom_page.h"

// Publishes SensorType::VehicleSpeed from whichever of Output Shaft Speed / Front Axle / Rear
// Axle the Main Speed Sensor "Source" dropdown (Setup -> Vehicle Information) selects. This is
// the sole source of VehicleSpeed on every board -- there is no other mechanism.
class MainSpeedSensorPassthrough : public Sensor {
public:
	MainSpeedSensorPassthrough() : Sensor(SensorType::VehicleSpeed) {}

	SensorResult get() const final override {
		switch (getCustomPage()->mainSpeedSensorSource) {
		case main_speed_sensor_source_e::None:
			return UnexpectedCode::Unknown;
		case main_speed_sensor_source_e::OutputShaftSpeed: {
			auto oss = Sensor::get(SensorType::OutputShaftSpeed);
			float revPerKm = getCustomPage()->ossRevPerKm;
			if (!oss.Valid || revPerKm <= 0) {
				return UnexpectedCode::Unknown;
			}
			// OSS is RPM; same Hz/teeth/revPerKm-derived km/h shape as AxleSpeedConverter, just
			// starting from RPM (revs/minute) instead of a raw pulse frequency.
			return oss.Value * 60.0f / revPerKm;
		}
		case main_speed_sensor_source_e::FrontAxle:
			return Sensor::get(SensorType::WheelSpeedFront);
		case main_speed_sensor_source_e::RearAxle:
			return Sensor::get(SensorType::WheelSpeedRear);
		}
		return UnexpectedCode::Unknown;
	}

	void showInfo(const char* sensorName) const override {
		auto source = getCustomPage()->mainSpeedSensorSource;
		efiPrintf("Sensor \"%s\" is Main Speed Sensor passthrough.", sensorName);
		efiPrintf("    Source = %d", (int)source);
		if (source == main_speed_sensor_source_e::OutputShaftSpeed) {
			auto oss = Sensor::get(SensorType::OutputShaftSpeed);
			efiPrintf("    OutputShaftSpeed: %s %.2f rpm, ossRevPerKm=%.1f", oss.Valid ? "valid" : "INVALID",
					oss.Valid ? oss.Value : 0.0f, getCustomPage()->ossRevPerKm);
		}
	}
};

static MainSpeedSensorPassthrough mainSpeedSensorPassthrough;

void initVehicleSpeedSensor() {
	mainSpeedSensorPassthrough.Register();
}

void deInitVehicleSpeedSensor() {
	mainSpeedSensorPassthrough.unregister();
}
