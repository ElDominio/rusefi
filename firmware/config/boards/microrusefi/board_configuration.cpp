/**
 * @file boards/microrusefi/board_configuration.cpp
 *
 *
 * @brief Configuration defaults for the microRusefi board
 *
 * MICRO_RUS_EFI
 * set engine_type 60
 *
 * MRE_BOARD_OLD_TEST
 * set engine_type 30
 *
 * MRE_BOARD_NEW_TEST
 * set engine_type 31
 *
 * See https://github.com/rusefi/rusefi/wiki/Hardware-microRusEfi-wiring
 *
 * @author Matthew Kennedy, (c) 2019
 */

#include "pch.h"
#include "mre_meta.h"
#include "defaults.h"

static void setInjectorPins() {
	engineConfiguration->injectionPins[0] = MRE_INJ_1;
	engineConfiguration->injectionPins[1] = MRE_INJ_2;
	engineConfiguration->injectionPins[2] = MRE_INJ_3;
	engineConfiguration->injectionPins[3] = MRE_INJ_4;
}

static void setIgnitionPins() {
	engineConfiguration->ignitionPins[0] = Gpio::D4;
	engineConfiguration->ignitionPins[1] = Gpio::D3;
	engineConfiguration->ignitionPins[2] = Gpio::D2;
	engineConfiguration->ignitionPins[3] = Gpio::D1;
}

Gpio getCommsLedPin() {
	return Gpio::E2; // d23 = blue
}

Gpio getRunningLedPin() {
	// D22 = green
	return Gpio::E4;
}

Gpio getWarningLedPin() {
	// D27 = orange or yellow
	return Gpio::E1;
}


static void setupDefaultSensorInputs() {
	// trigger inputs
	// tle8888 VR conditioner
	engineConfiguration->triggerInputPins[0] = Gpio::C6;
	engineConfiguration->triggerInputPins[1] = Gpio::Unassigned;
	// Direct hall-only cam input
	engineConfiguration->camInputs[0] = Gpio::A5;

	// open question if it's great to have TPS in default TPS - the down-side is for
	// vehicles without TPS or for first start without TPS one would have to turn in off
	// to avoid cranking corrections based on wrong TPS data
	engineConfiguration->tps1_1AdcChannel = MRE_IN_TPS;

	engineConfiguration->map.sensor.hwChannel = MRE_IN_MAP;

	// EFI_ADC_14: "32 - AN volt 6"
	engineConfiguration->afr.hwChannel = EFI_ADC_14;

	engineConfiguration->clt.adcChannel = MRE_IN_CLT;

	engineConfiguration->iat.adcChannel = MRE_IN_IAT;

#ifndef EFI_BOOTLOADER
	setCommonNTCSensor(&engineConfiguration->auxTempSensor1, MRE_DEFAULT_AT_PULLUP);
	setCommonNTCSensor(&engineConfiguration->auxTempSensor2, MRE_DEFAULT_AT_PULLUP);
#endif // EFI_BOOTLOADER
}


/**
 * @brief   Board-specific configuration defaults.
 *
 * See also setDefaultEngineConfiguration
 *

 */
void setBoardDefaultConfiguration() {
	setInjectorPins();
	setIgnitionPins();

	// MRE has a special main relay control low side pin
	// rusEfi firmware is totally not involved with main relay control on microRusEfi board
	// todo: maybe even set EFI_MAIN_RELAY_CONTROL to FALSE for MRE configuration
	// TLE8888 half bridges (pushpull, lowside, or high-low)  TLE8888_IN11 / TLE8888_OUT21
	// Gpio::TLE8888_PIN_21: "35 - GP Out 1"
	engineConfiguration->fuelPumpPin = MRE_GPOUT_1;

//	engineConfiguration->isSdCardEnabled = true;

	// TLE8888 high current low side: VVT2 IN9 / OUT5
	// Gpio::E10: "3 - Lowside 2"
	engineConfiguration->idle.solenoidPin = MRE_LS_2;


	// Gpio::TLE8888_PIN_22: "34 - GP Out 2"
	engineConfiguration->fanPin = MRE_GPOUT_2;

	// "required" hardware is done - set some reasonable defaults
	setupDefaultSensorInputs();

	// Enable onboard SD card on v0.6.0
	engineConfiguration->sdCardSpiDevice = SPI_DEVICE_2;
	engineConfiguration->isSdCardEnabled = true;
	engineConfiguration->sdCardCsPin = Gpio::E15;

	// Don't enable expansion header SPI by default
	engineConfiguration->is_enabled_spi_3 = false;

	engineConfiguration->cylindersCount = 4;
	engineConfiguration->firingOrder = FO_1_3_4_2;

	engineConfiguration->ignitionMode = IM_INDIVIDUAL_COILS; // IM_WASTED_SPARK
	engineConfiguration->crankingInjectionMode = IM_SIMULTANEOUS;
	engineConfiguration->injectionMode = IM_SIMULTANEOUS;//IM_BATCH;// IM_SEQUENTIAL;
}

