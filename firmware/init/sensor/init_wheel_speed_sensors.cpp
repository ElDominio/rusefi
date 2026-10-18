#include "pch.h"

#if EFI_WHEEL_SPEED_SENSORS

#include "init.h"
#include "frequency_sensor.h"
#include "axle_speed_converter.h"
#include "custom_page.h"

static FrequencySensor wheelSpeedFrontSensor(SensorType::WheelSpeedFront, MS2NT(500));
static AxleSpeedConverter frontAxleConverter(/*isFront*/true);

static FrequencySensor wheelSpeedRearSensor(SensorType::WheelSpeedRear, MS2NT(500));
static AxleSpeedConverter rearAxleConverter(/*isFront*/false);

static float filterParameterFromReciprocal(int reciprocal) {
	if (reciprocal < 3 || reciprocal > 200) {
		reciprocal = 3;
	}
	return 1.0f / reciprocal;
}

// Output Shaft Speed is initialized separately (init_output_shaft_speed_sensor.cpp).
// If Mode is CAN/Lua (or None) for a given sensor, we deliberately do not register a
// FrequencySensor here, leaving that SensorType unregistered so Lua's existing generic
// Sensor.new("WheelSpeedFront"):set(value) mechanism (or a CAN RX handler) can claim it.
void initWheelSpeedSensors() {
	auto cfg = getCustomPage();

	if (cfg->wheelSpeedFrontMode == wheel_speed_sensor_mode_e::PhysicalPin) {
		wheelSpeedFrontSensor.initIfValid(
				cfg->wheelSpeedFrontPin, frontAxleConverter,
				filterParameterFromReciprocal(cfg->wheelSpeedFrontFilterReciprocal));
	}

	if (cfg->wheelSpeedRearMode == wheel_speed_sensor_mode_e::PhysicalPin) {
		wheelSpeedRearSensor.initIfValid(
				cfg->wheelSpeedRearPin, rearAxleConverter,
				filterParameterFromReciprocal(cfg->wheelSpeedRearFilterReciprocal));
	}
}

void deinitWheelSpeedSensors() {
	wheelSpeedFrontSensor.deInit();
	wheelSpeedRearSensor.deInit();
}

#else // !EFI_WHEEL_SPEED_SENSORS

void initWheelSpeedSensors() { }
void deinitWheelSpeedSensors() { }

#endif // EFI_WHEEL_SPEED_SENSORS
