#include "pch.h"

// do we use some sort of a custom bootloader protocol in rusEFI WBO?
// todo: should we move to any widely used protocol like OpenBLT or else?

#if EFI_WIDEBAND_FIRMWARE_UPDATE && EFI_CAN_SUPPORT

#include "ch.h"
#include "can_msg_tx.h"
#include "rusefi_wideband.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "wideband_can.h"
#pragma GCC diagnostic pop

// This file contains an array called build_wideband_noboot_bin
// This array contains the firmware image for the wideband contoller

#define EVT_BOOTLOADER_ACK EVENT_MASK(0)

static thread_t* waitingBootloaderThread = nullptr;

void handleWidebandBootloaderAck() {
	auto t = waitingBootloaderThread;
	if (t) {
		chEvtSignal(t, EVT_BOOTLOADER_ACK);
	}
}

bool waitAck() {
	return chEvtWaitAnyTimeout(EVT_BOOTLOADER_ACK, TIME_MS2I(1000)) != 0;
}

static size_t getWidebandBus() {
	return engineConfiguration->widebandOnSecondBus ? 1 : 0;
}

void updateWidebandFirmware() {
}

void setWidebandOffset(uint8_t index) {
	// Clear any pending acks for this thread
	chEvtGetAndClearEvents(EVT_BOOTLOADER_ACK);

	// Send messages to the current thread when acks come in
	waitingBootloaderThread = chThdGetSelfX();

	efiPrintf("***************************************");
	efiPrintf("          WIDEBAND INDEX SET");
	efiPrintf("***************************************");
	efiPrintf("Setting all connected widebands to index %d...", index);

	{
		CanTxMessage m(CanCategory::WBO_SERVICE, WB_MSG_SET_INDEX, 1, getWidebandBus(), true);
		m[0] = index;
	}

	if (!waitAck()) {
		criticalError("Wideband index set failed: no controller detected!");
	}

	waitingBootloaderThread = nullptr;
}

// huh? this code here should not be hidden under 'EFI_WIDEBAND_FIRMWARE_UPDATE' condition?!
void sendWidebandInfo() {
	CanTxMessage m(CanCategory::WBO_SERVICE, WB_MGS_ECU_STATUS, 2, getWidebandBus(), true);

	float vbatt = Sensor::getOrZero(SensorType::BatteryVoltage) * 10;

	m[0] = vbatt;

	// Offset 1 bit 0 = heater enable
	m[1] = engine->engineState.heaterControlEnabled ? 0x01 : 0x00;
}

#endif // EFI_WIDEBAND_FIRMWARE_UPDATE && HAL_USE_CAN
