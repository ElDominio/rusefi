#include "pch.h"

/**
 * We were NOT able to get this code working reliable
 * it could be that biquad parameter or biquad overall is part of the problem
 */

#include "init.h"
#include "frequency_sensor.h"
#include "input_shaft_speed_converter.h"

extern FrequencySensor vehicleSpeedSensor;

FrequencySensor inputShaftSpeedSensor(SensorType::InputShaftSpeed, MS2NT(500));
static InputShaftSpeedConverter inputSpeedConverter;

void initInputShaftSpeedSensor() {
	int parameter = engineConfiguration->issFilterReciprocal;

	if (parameter < 3 || parameter > 200) {
		parameter = 3;
	}

	float filterParameter = 1.0f / parameter;

	if (engineConfiguration->tcuInputSpeedSensorSharedWithVss) {
		// Same physical sensor as the main VSS: derive our reading from its raw edge frequency
		// instead of registering a second EXTI callback on the same pin.
		inputShaftSpeedSensor.initShared(inputSpeedConverter, filterParameter);
		vehicleSpeedSensor.setSharedListener(&inputShaftSpeedSensor);
	} else {
		vehicleSpeedSensor.setSharedListener(nullptr);
		inputShaftSpeedSensor.initIfValid(
				engineConfiguration->tcuInputSpeedSensorPin, inputSpeedConverter, filterParameter);
	}
}

void deinitInputShaftSpeedSensor() {
	vehicleSpeedSensor.setSharedListener(nullptr);
	inputShaftSpeedSensor.deInit();
}
