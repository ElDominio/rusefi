#include "pch.h"

/*
 * - we have many board with one or two on-board F0 WBO module
 * - we have a requirement to use external AEM controller instead of onboard module ('Genesis use-case')
 * - we also have requirement for CANbus ID conflict resolution in case of vehicle integration ('Camaro use-case')
 */

// do we use some sort of a custom bootloader protocol in rusEFI WBO?
// todo: should we move to any widely used protocol like OpenBLT or else?

#if EFI_WIDEBAND_FIRMWARE_UPDATE && EFI_CAN_SUPPORT

#include "ch.h"
#include "can_msg_tx.h"
#include "rusefi_wideband.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "wideband_firmware/for_rusefi/wideband_can.h"
#pragma GCC diagnostic pop

// This file contains an array called build_wideband_noboot_bin
// This array contains the firmware image for the wideband contoller
#include "wideband_firmware/for_rusefi/wideband_image.h"

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
	size_t bus = getWidebandBus();

	// Clear any pending acks for this thread
	chEvtGetAndClearEvents(EVT_BOOTLOADER_ACK);

	// Send messages to the current thread when acks come in
	waitingBootloaderThread = chThdGetSelfX();

	efiPrintf("***************************************");
	efiPrintf("        WIDEBAND FIRMWARE UPDATE");
	efiPrintf("***************************************");
	efiPrintf("Wideband Update: Rebooting to bootloader...");

	// The first request will reboot the chip (if necessary), and the second one will enable bootloader mode
	// If the chip was already in bootloader (aka manual mode), then that's ok - the second request will
	// just be safely ignored (but acked)
	for (int i = 0; i < 2; i++) {
		{
			// Send bootloader entry command
			CanTxMessage m(CanCategory::WBO_SERVICE, WB_BL_ENTER, 0, bus, true);
		}

		if (!waitAck()) {
			efiPrintf("Wideband Update ERROR: Expected ACK from entry to bootloader, didn't get one.");
			return;
		}

		// Let the controller reboot (and show blinky lights for a second before the update begins)
		chThdSleepMilliseconds(200);
	}

	efiPrintf("Wideband Update: in update mode, erasing flash...");

	{
		// Erase flash - opcode 1, magic value 0x5A5A
		CanTxMessage m(CanCategory::WBO_SERVICE, 0xEF1'5A5A, 0, bus, true);
	}

	if (!waitAck()) {
		efiPrintf("Wideband Update ERROR: Expected ACK from flash erase command, didn't get one.");
		return;
	}

	size_t totalSize = sizeof(build_wideband_image_bin);

	efiPrintf("Wideband Update: Flash erased! Sending %d bytes...", totalSize);

	// Send flash data 8 bytes at a time
	for (size_t i = 0; i < totalSize; i += 8) {
		{
			CanTxMessage m(CanCategory::WBO_SERVICE, 0xEF2'0000 + i, 8, bus, true);
			memcpy(&m[0], build_wideband_image_bin + i, 8);
		}

		if (!waitAck()) {
			efiPrintf("Wideband Update ERROR: Expected ACK from data write, didn't get one.");
			return;
		}
	}

	efiPrintf("Wideband Update: Update complete! Rebooting controller.");

	{
		// Reboot to firmware!
		CanTxMessage m(CanCategory::WBO_SERVICE, 0xEF3'0000, 0, bus, true);
	}

	waitAck();

	waitingBootloaderThread = nullptr;
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
