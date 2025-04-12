#include "pch.h"
#include "proteus_meta.h"
#include "harley_canned.cpp"
#include "harley.h"

static void harleyEngine() {
    engineConfiguration->cylindersCount = 2;
    engineConfiguration->firingOrder = FO_1_2;
    strcpy(engineConfiguration->engineMake, "Harley");
}

/**
 * HARLEY
 * set engine_type 6
 */
void setHarley() {
    harleyEngine();
    
	engineConfiguration->mapCamDetectionAnglePosition = 50;

	setCustomMap(/*lowValue*/ 20, /*mapLowValueVoltage*/ 0.79, /*highValue*/ 101.3, /*mapHighValueVoltage*/ 4);

#if HW_PROTEUS && EFI_PROD_CODE
    engineConfiguration->acrPin = Gpio::PROTEUS_IGN_8;
    engineConfiguration->acrPin2 = Gpio::PROTEUS_IGN_9;

    engineConfiguration->triggerInputPins[0] = PROTEUS_VR_1;
	// for now we need non wired camInput to keep TS field enable/disable logic happy
#if EFI_PROD_CODE
	  engineConfiguration->camInputs[0] = PROTEUS_DIGITAL_6;
#else
    engineConfiguration->camInputs[0] = Gpio::Unassigned;
#endif

	engineConfiguration->luaOutputPins[0] = Gpio::PROTEUS_LS_12;

	setTPS1Inputs(PROTEUS_IN_TPS, PROTEUS_IN_TPS1_2);

	setPPSInputs(PROTEUS_IN_ANALOG_VOLT_4, PROTEUS_IN_ANALOG_VOLT_5);


	strncpy(config->luaScript, R"(


function onCanRx(bus, id, dlc, data)

end


function onTick()

end
)", efi::size(config->luaScript));
#endif
}
