#include "pch.h"

#include "wheel_slip_ratio_source.h"

#if EFI_WHEEL_SPEED_SENSORS

#include "custom_page.h"

expected<float> readWheelSpeedSource(wheel_speed_source_e source) {
	switch (source) {
	case wheel_speed_source_e::None:
		return unexpected;
	case wheel_speed_source_e::VehicleSpeed:
		return Sensor::get(SensorType::VehicleSpeed);
	case wheel_speed_source_e::FrontAxle:
		return Sensor::get(SensorType::WheelSpeedFront);
	case wheel_speed_source_e::RearAxle:
		return Sensor::get(SensorType::WheelSpeedRear);
	case wheel_speed_source_e::OutputShaftSpeed:
		return Sensor::get(SensorType::OutputShaftSpeed);
	case wheel_speed_source_e::AuxSpeed1:
		return Sensor::get(SensorType::AuxSpeed1);
	case wheel_speed_source_e::AuxSpeed2:
		return Sensor::get(SensorType::AuxSpeed2);
	}
	return unexpected;
}

bool isConfigurableWheelSlipRatioActive() {
	auto cfg = getCustomPage();
	return cfg->wheelSlipRatioSource1 != wheel_speed_source_e::None
			&& cfg->wheelSlipRatioSource2 != wheel_speed_source_e::None;
}

class ConfigurableWheelSlipRatio : public Sensor {
public:
	ConfigurableWheelSlipRatio() : Sensor(SensorType::WheelSlipRatio) {}

	SensorResult get() const final override {
		auto cfg = getCustomPage();
		auto source1 = readWheelSpeedSource(cfg->wheelSlipRatioSource1);
		auto source2 = readWheelSpeedSource(cfg->wheelSlipRatioSource2);
		if (!source1 || !source2 || source2.Value == 0) {
			return UnexpectedCode::Unknown;
		}
		return source1.Value / source2.Value;
	}

	void showInfo(const char* sensorName) const override {
		auto cfg = getCustomPage();
		auto source1 = readWheelSpeedSource(cfg->wheelSlipRatioSource1);
		auto source2 = readWheelSpeedSource(cfg->wheelSlipRatioSource2);
		efiPrintf("Sensor \"%s\" is configurable Wheel Slip Ratio.", sensorName);
		efiPrintf("    Source1 = %d: %s %.2f", (int)cfg->wheelSlipRatioSource1,
				source1.Valid ? "valid" : "INVALID", source1.Valid ? source1.Value : 0.0f);
		efiPrintf("    Source2 = %d: %s %.2f", (int)cfg->wheelSlipRatioSource2,
				source2.Valid ? "valid" : "INVALID", source2.Valid ? source2.Value : 0.0f);
	}
};

static ConfigurableWheelSlipRatio configurableWheelSlipRatio;

void initConfigurableWheelSlipRatio() {
	if (isConfigurableWheelSlipRatioActive()) {
		configurableWheelSlipRatio.Register();
	}
}

void deinitConfigurableWheelSlipRatio() {
	configurableWheelSlipRatio.unregister();
}

#else // !EFI_WHEEL_SPEED_SENSORS

bool isConfigurableWheelSlipRatioActive() { return false; }
void initConfigurableWheelSlipRatio() { }
void deinitConfigurableWheelSlipRatio() { }

#endif // EFI_WHEEL_SPEED_SENSORS
