/**
 * @file    tps.h
 * @brief
 *
 *
 * @date Nov 15, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#pragma once

#include "global.h"
#include "engine_configuration.h"

// todo: '_TS_' part here no longer makes sense now that we use volts!
// Scaled to 1000 counts = 5.0 volts
#define TPS_TS_CONVERSION (1 / PACK_MULT_RAW_VOLTAGE)

// we have this '100'  magic constant too often for two many other reasons todo: refactor further?
#define POSITION_FULLY_OPEN 100

// todo: looks like we need to rename this method since we no longer store sensors in ADC units?
constexpr inline int convertVoltageTo10bitADC(float voltage) {
	return (int) (voltage * TPS_TS_CONVERSION);
}

void grabTPSIsClosed();
void grabTPSIsWideOpen();
void grabPedalIsUp();
void grapTps1PrimaryIsClosed();
void grapTps1PrimaryIsOpen();
void grabPedalIsWideOpen();

// External CAN ETB support (init_tps.cpp): feeds TPS1/TPSB volts into the same calibration curve
// (tpsMin/tpsMax/tps1SecondaryMin/tps1SecondaryMax) a physically-wired TPS1/TPSB would use - see
// can_etb.h's CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS for the raw-ADC-counts-to-volts conversion the
// caller (init_etb_can.cpp) applies before calling this.
void postExternalCanEtbRawTps(float tps1Volts, float tpsBVolts, efitick_t nowNt);

// Same as postExternalCanEtbRawTps() above, for the pedal - also wired to the board (not rusEFI's
// own ADC) under external CAN ETB mode. Feeds throttlePedalUpVoltage/WOTVoltage/
// SecondaryUpVoltage/SecondaryWOTVoltage's calibration curve.
void postExternalCanEtbRawPedal(float pedal1Volts, float pedal2Volts, efitick_t nowNt);

#if EFI_SENT_SUPPORT

struct SentTps : public StoredValueSensor {
	SentTps() : StoredValueSensor(SensorType::Tps1, MS2NT(200)) {
	}

	bool isRedundant() const override {
		return true;
	}
};

void sentTpsDecode(SentInput sentCh);
float decodeTpsSentValue(float sentValue);
bool isDigitalTps1();

#endif
