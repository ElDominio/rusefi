/**
 * @file boards/protorico-grummann/board_configuration.cpp
 *
 * Protorico-Grummann custom board (Grumman step van conversion).
 *
 * Same physical Hellen 100-pin module and power management as protorico-econoline, but a
 * different harness: single-coil distributor ignition with a Ford TFI-style ignition-bypass
 * output, a single injector firing all cylinders, and a 2-wire stepper idle valve wired to
 * what would otherwise be coil driver pins. See readme.md for the full pin-repurposing
 * rationale.
 *
 * @author Andrey Belomutskiy, (c) 2012-2024
 */

#include "pch.h"
#include "defaults.h"
#include "hellen_meta.h"
#include "board_overrides.h"

static void setInjectorPins() {
	// Single injector output drives all injectors in parallel (IM_SINGLE_POINT below) -- there
	// is no per-cylinder injection on this engine.
	engineConfiguration->injectionPins[0] = Gpio::D3;  // H144_OUT_IO1 / STM32_INJ1
	engineConfiguration->injectionPins[1] = Gpio::Unassigned;
	engineConfiguration->injectionPins[2] = Gpio::Unassigned; // INJ3 (A9/H144_OUT_IO2) repurposed as A/C request input, see setupDefaultSensorInputs()
	engineConfiguration->injectionPins[3] = Gpio::Unassigned; // INJ4 (D15/H144_OUT_PWM7) repurposed as TCC lockup output, board meta output only
	engineConfiguration->injectionPins[4] = Gpio::Unassigned; // INJ5 (A8/H144_OUT_IO12) repurposed as EGR solenoid output, board meta output only
	engineConfiguration->injectionPins[5] = Gpio::Unassigned;
	engineConfiguration->injectionPins[6] = Gpio::Unassigned;
	engineConfiguration->injectionPins[7] = Gpio::Unassigned;
}

static void setIgnitionPins() {
	// Only one coil -- an external distributor routes spark to each cylinder mechanically.
	engineConfiguration->ignitionPins[0] = Gpio::H144_IGN_1; // PC13 / STM32_IGN1 -- coil
	engineConfiguration->ignitionPins[1] = Gpio::Unassigned; // IGN2 (E5) repurposed as ignition bypass output, board meta output only
	engineConfiguration->ignitionPins[2] = Gpio::Unassigned; // IGN3 (E4) repurposed as idle stepper direction, see below
	engineConfiguration->ignitionPins[3] = Gpio::Unassigned; // IGN4 (E3) repurposed as idle stepper enable, see below
	engineConfiguration->ignitionPins[4] = Gpio::Unassigned; // IGN5 (E2) repurposed as idle stepper step, see below
	engineConfiguration->ignitionPins[5] = Gpio::Unassigned;
	engineConfiguration->ignitionPins[6] = Gpio::Unassigned;
	engineConfiguration->ignitionPins[7] = Gpio::Unassigned;
}

static void setupDefaultSensorInputs() {
	// Trigger inputs -- CKP
	engineConfiguration->triggerInputPins[0] = Gpio::E12; // H144_IN_D_1 (STM32_CRANK)
	engineConfiguration->triggerInputPins[1] = Gpio::Unassigned;

	// No cam sensor on this build -- neither the single-coil (IM_ONE_COIL) ignition nor the
	// single-point (IM_SINGLE_POINT) injection needs cam phase, so H144_IN_D_2 (cam on
	// protorico-econoline) is free here and used for VSS instead (see connectors.yaml).
	engineConfiguration->camInputs[0] = Gpio::Unassigned;
	engineConfiguration->camInputs[1] = Gpio::Unassigned;
	engineConfiguration->camInputs[2] = Gpio::Unassigned;
	engineConfiguration->camInputs[3] = Gpio::Unassigned;

	// Sensor ADC Channels
	engineConfiguration->clt.adcChannel = H144_IN_CLT; // PC2 / AIN11 -- real coolant temp sensor on this build
	engineConfiguration->iat.adcChannel = H144_IN_IAT; // PC3 / AIN14

	// TPS (Throttle Position Sensor) Inputs
	setTPS1Inputs(H144_IN_TPS, EFI_ADC_NONE); // PA4 / AIN17

	// PPS (Pedal Position Sensor) Inputs
	setPPSInputs(EFI_ADC_NONE, EFI_ADC_NONE); // No pedal position sensor on this build

	// MAP sensor
	engineConfiguration->map.sensor.hwChannel = H144_IN_MAP1; // PC0 / AIN9

	// No baro sensor, no O2 sensors, no aux linear sensors on this build
	engineConfiguration->baroSensor.hwChannel = EFI_ADC_NONE;
	engineConfiguration->afr.hwChannel = EFI_ADC_NONE;
	engineConfiguration->afr.hwChannel2 = EFI_ADC_NONE;
	engineConfiguration->auxLinear1.hwChannel = EFI_ADC_NONE;
	engineConfiguration->auxLinear2.hwChannel = EFI_ADC_NONE;
	engineConfiguration->auxLinear3.hwChannel = EFI_ADC_NONE;
	engineConfiguration->auxLinear4.hwChannel = EFI_ADC_NONE;

	// A/C request switch input -- physically the INJ3 driver pin (A9 / H144_OUT_IO2),
	// repurposed as a digital input since there is no third injector on this build.
	engineConfiguration->acSwitch = Gpio::A9;

	// Switch inputs -- none on this build
	engineConfiguration->clutchDownPin = Gpio::Unassigned;
	engineConfiguration->brakePedalPin = Gpio::Unassigned;
}

static void protorico_grummann_boardConfigOverrides() {
	setHellenMegaEnPin(); // PWR_EN (PE10)
	setHellenVbatt();     // VBAT (PA5) / VIGN (PA5)

	// No SPI/SD Card/Accelerometer
	// setHellenSdCardSpi3();
	// engineConfiguration->isSdCardEnabled = true;
	// hellenMegaAccelerometerPreInitCS2Pin();

	// CAN Configurations
	setHellenCan();

	// Force disable hidden/unconfigurable AuxLinear sensors to prevent pin conflicts (specifically PA2 for Knock control)
	engineConfiguration->auxLinear3.hwChannel = EFI_ADC_NONE;
	engineConfiguration->auxLinear4.hwChannel = EFI_ADC_NONE;
}

static void protorico_grummann_boardDefaultConfiguration() {
	setInjectorPins();
	setIgnitionPins();

	// Single coil, distributor-fired -- only SPARKOUT_1 is used, no per-cylinder coils.
	engineConfiguration->ignitionMode = IM_ONE_COIL;
	// Single injector feeding all cylinders -- one injector driver, full-length pulse fired
	// once per cylinder event (see IM_SINGLE_POINT in fuel_math.cpp/InjectionEvent::update()).
	engineConfiguration->injectionMode = IM_SINGLE_POINT;

	// Cylinder count/firing order are left at the generic default (see
	// default_base_engine.cpp) -- set these for your actual engine in TunerStudio. Neither
	// IM_ONE_COIL nor IM_SINGLE_POINT depend on this value for correct hardware operation
	// (both always drive slot 0), so it only affects total fuel delivered per cycle
	// (getNumberOfInjections()) until configured.

	setupDefaultSensorInputs();

	setCrankOperationMode();

	// Relays and Solenoids
	engineConfiguration->mainRelayPin = Gpio::Unassigned;
	engineConfiguration->fuelPumpPin = Gpio::B14;             // H_SPI2_MISO (STM32_FP)
	engineConfiguration->acRelayPin = Gpio::C6;               // H144_OUT_PWM2 -- A/C compressor relay output
	engineConfiguration->fanPin = Gpio::Unassigned;
	engineConfiguration->fan2Pin = Gpio::Unassigned;
	engineConfiguration->alternatorControlPin = Gpio::Unassigned;
	engineConfiguration->boostControlPin = Gpio::Unassigned;
	engineConfiguration->tachOutputPin = Gpio::C7;            // H144_OUT_PWM3 (STM32_TACHO)
	engineConfiguration->malfunctionIndicatorPin = Gpio::C8;  // H144_OUT_PWM4 (STM32_CEL)
	engineConfiguration->speedometerOutputPin = Gpio::Unassigned;

	// Idle Valve -- 2-wire (direction/step) stepper motor, not a PWM solenoid. Physically wired
	// to what were the IGN3/4/5 coil driver pins on this single-coil build.
	engineConfiguration->idle.solenoidPin = Gpio::Unassigned;
	engineConfiguration->useStepperIdle = true;
	engineConfiguration->idle.stepperDirectionPin = Gpio::E4; // H144_IGN_3 -- silkscreen "Ignition 3"
	engineConfiguration->idle.stepperStepPin = Gpio::E2;      // H144_IGN_5 -- silkscreen "Ignition 5"
	engineConfiguration->stepperEnablePin = Gpio::E3;         // H144_IGN_4 -- silkscreen "Ignition 4"

	// Disable SPI
	engineConfiguration->sdCardCsPin = Gpio::Unassigned;
	engineConfiguration->isSdCardEnabled = false;
	engineConfiguration->is_enabled_spi_1 = false;
	engineConfiguration->is_enabled_spi_2 = false;
	engineConfiguration->is_enabled_spi_3 = false;
	engineConfiguration->spi1mosiPin = Gpio::Unassigned;
	engineConfiguration->spi1misoPin = Gpio::Unassigned;
	engineConfiguration->spi1sckPin = Gpio::Unassigned;
	engineConfiguration->spi2mosiPin = Gpio::Unassigned;
	engineConfiguration->spi2misoPin = Gpio::Unassigned;
	engineConfiguration->spi2sckPin = Gpio::Unassigned;
	engineConfiguration->spi3mosiPin = Gpio::Unassigned;
	engineConfiguration->spi3misoPin = Gpio::Unassigned;
	engineConfiguration->spi3sckPin = Gpio::Unassigned;

	hellenWbo();
}

// Pins available for generic output selection in TunerStudio. Pins with no dedicated
// engineConfiguration field (ignition bypass, TCC lockup, EGR solenoid) are listed here only --
// same pattern as protorico-econoline's EGR/TCI/IMRC/Torque-Lockup outputs -- assign their
// function from the relevant TunerStudio dialog or drive them from Lua.
static Gpio OUTPUTS[] = {
	Gpio::D3,  // Injector 1 (all injectors / single-point injection)
	Gpio::C13, // Ignition Coil 1
	Gpio::E5,  // Ignition Bypass Output
	Gpio::E4,  // Idle Stepper Direction
	Gpio::E3,  // Idle Stepper Enable
	Gpio::E2,  // Idle Stepper Step
	Gpio::B14, // Fuel Pump
	Gpio::C6,  // A/C Relay Output
	Gpio::C8,  // CEL
	Gpio::D15, // Torque Converter Lockup
	Gpio::A8,  // EGR Solenoid
};

int getBoardMetaOutputsCount() {
	return efi::size(OUTPUTS);
}

Gpio* getBoardMetaOutputs() {
	return OUTPUTS;
}

int getBoardMetaDcOutputsCount() {
	return 0;
}

void setup_custom_board_overrides() {
	custom_board_DefaultConfiguration = protorico_grummann_boardDefaultConfiguration;
	custom_board_ConfigOverrides = protorico_grummann_boardConfigOverrides;
}

Gpio getCommsLedPin() {
	return Gpio::E7;
}

Gpio getRunningLedPin() {
	return Gpio::D7;
}

Gpio getWarningLedPin() {
	return Gpio::Unassigned;
}
