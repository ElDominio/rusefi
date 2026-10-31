#include "hal.h"

#if HAL_USE_CAN

#include <chrono>
#include <thread>

// Local timing helpers so this file does not depend on posix/ch.cpp being linked (avoids
// undefined references to chVTGetSystemTimeX/chThdSleepMilliseconds when link order or
// gc-sections drops ch.o).
static systime_t posix_get_system_time_ticks(void) {
	uint64_t us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
	return (systime_t)((us * CH_CFG_ST_FREQUENCY) / 1000000ULL);
}

extern "C" {
void can_lld_init(void);
void can_lld_start(CANDriver* canp);
void can_lld_stop(CANDriver* canp);
bool can_lld_is_rx_nonempty(CANDriver* canp, canmbx_t mailbox);
void can_lld_receive(CANDriver* canp, canmbx_t mailbox, CANRxFrame* crfp);
bool can_lld_is_tx_empty(CANDriver* canp, canmbx_t mailbox);
void can_lld_transmit(CANDriver* canp, canmbx_t mailbox, const CANTxFrame* ctfp);
void _can_rx_full_isr(CANDriver*, canmbx_t);
}

void canInit(void) {
	can_lld_init();
}

void canObjectInit(CANDriver* canp) {
	if (!canp) {
		return;
	}
	canp->state = CAN_STOP;
	canp->config = nullptr;
}

void canStart(CANDriver* canp, const CANConfig* config) {
	if (!canp) {
		return;
	}
	canp->config = config;
	canp->state = CAN_READY;
	can_lld_start(canp);
}

void canStop(CANDriver* canp) {
	if (!canp) {
		return;
	}
	can_lld_stop(canp);
	canp->state = CAN_STOP;
}

msg_t canTransmitTimeout(CANDriver* canp, canmbx_t mailbox, const CANTxFrame* ctfp, sysinterval_t timeout) {
	if (!canp || !ctfp) {
		return MSG_TIMEOUT;
	}
	if (can_lld_is_tx_empty(canp, mailbox)) {
		can_lld_transmit(canp, mailbox, ctfp);
		return MSG_OK;
	}
	// Model the real hal_can.c behavior: a caller with a full queue stays blocked
	// in the driver until a mailbox frees or the timeout expires. Without this the
	// sim's fire-and-forget transmit hides driver-timeout bugs that only appear on
	// hardware (e.g. the CAN TX thread falling behind its schedule while the bus
	// is un-ACKed - see periodic_thread_controller.h and can_msg_tx.cpp).
	if (timeout != TIME_IMMEDIATE && timeout != TIME_INFINITE) {
		const uint64_t waitMs = (static_cast<uint64_t>(timeout) * 1000ULL) / CH_CFG_ST_FREQUENCY;
		std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
	}
	return MSG_TIMEOUT;
}

msg_t canReceiveTimeout(CANDriver* canp, canmbx_t mailbox, CANRxFrame* crfp, sysinterval_t timeout) {
	if (!canp || !crfp) {
		return MSG_TIMEOUT;
	}

	if (timeout == TIME_IMMEDIATE) {
		if (!can_lld_is_rx_nonempty(canp, mailbox)) {
			return MSG_TIMEOUT;
		}
		can_lld_receive(canp, mailbox, crfp);
		return MSG_OK;
	}

	auto start = posix_get_system_time_ticks();
	while (true) {
		if (can_lld_is_rx_nonempty(canp, mailbox)) {
			can_lld_receive(canp, mailbox, crfp);
			return MSG_OK;
		}

		if (timeout != TIME_INFINITE) {
			auto now = posix_get_system_time_ticks();
			if ((now - start) >= timeout) {
				return MSG_TIMEOUT;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

// Legacy macro expects these names
msg_t canTransmit(CANDriver* canp, canmbx_t mailbox, const CANTxFrame* ctfp, sysinterval_t timeout) {
	return canTransmitTimeout(canp, mailbox, ctfp, timeout);
}

msg_t canReceive(CANDriver* canp, canmbx_t mailbox, CANRxFrame* crfp, sysinterval_t timeout) {
	return canReceiveTimeout(canp, mailbox, crfp, timeout);
}

extern "C" void _can_rx_full_isr(CANDriver*, canmbx_t) {
	// no-op for POSIX simulator
}

#endif // HAL_USE_CAN
