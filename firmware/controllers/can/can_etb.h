/**
 * @file can_etb.h
 *
 * CAN protocol for the external CH32V203-based ETB controller (see the external-etb project,
 * ../../external-etb/CH32V203_ETB_CONTROLLER.md and ../../external-etb/rusefi/RUSEFI_SIDE_TODO.md).
 *
 * This mirrors the board side's can_bus.h exactly (../../external-etb/firmware/src/can_bus.h) -
 * keep the two in sync by hand, there is no shared codegen between the two repos.
 *
 * NOTE: 0x300 is a placeholder base ID, not yet confirmed as free of conflicts on any real vehicle
 * bus (see RUSEFI_SIDE_TODO.md #5.1). Renumber both sides together before this is more than a bench
 * experiment.
 *
 * NOTE: CAN_ID_ETB_AUTOTUNE_STATUS_1/2 and EtbCanMode::Autotune (below) are a rusEFI-side-only
 * protocol EXTENSION, not yet mirrored in the board's can_bus.h/.c - the CH32 does not implement
 * PID autotune today. This header documents the intended contract (frame IDs, byte layout,
 * semantics) for whoever implements it there; nothing on the rusEFI side sending/expecting these
 * will do anything useful against the current board firmware until that's built.
 */

#pragma once

#include <cstdint>
#include "dc_motor.h"

#define CAN_ETB_BASE_ID 0x300u

// Telemetry: CH32 -> rusEFI, standard 11-bit frames, DLC 8
#define CAN_ID_ETB_STATUS     (CAN_ETB_BASE_ID + 0)
#define CAN_ID_ETB_PID_STATUS (CAN_ETB_BASE_ID + 1)
#define CAN_ID_ETB_RAW        (CAN_ETB_BASE_ID + 2)
// NOT YET IMPLEMENTED on the board (see file header NOTE) - board's current best pFactor/iFactor
// (frame 1) and dFactor (frame 2) guess while running PID autotune (mode == Autotune below),
// re-sent periodically ("keeps trying" - there is no "done", same as the local relay-autotune in
// electronic_throttle.cpp, which also runs until the user stops it).
#define CAN_ID_ETB_AUTOTUNE_STATUS_1 (CAN_ETB_BASE_ID + 9)
#define CAN_ID_ETB_AUTOTUNE_STATUS_2 (CAN_ETB_BASE_ID + 10)

// Commands: rusEFI -> CH32, standard 11-bit frames, DLC 8
#define CAN_ID_ETB_GAINS_1    (CAN_ETB_BASE_ID + 4)
#define CAN_ID_ETB_GAINS_2    (CAN_ETB_BASE_ID + 5)
#define CAN_ID_ETB_TARGET     (CAN_ETB_BASE_ID + 6)
#define CAN_ID_ETB_CAL_TPS    (CAN_ETB_BASE_ID + 7)
#define CAN_ID_ETB_CAL_PEDAL  (CAN_ETB_BASE_ID + 8)

// ETB_STATUS (0x300) byte layout - see can_bus.c's can_tx_status()
#define ETB_STATUS_OFFSET_VERSION    0
#define ETB_STATUS_OFFSET_STATUS     1
#define ETB_STATUS_OFFSET_TPS1       2 // int16, x100 (scaled_percent / PACK_MULT_PERCENT)
#define ETB_STATUS_OFFSET_TPSB       4 // int16, x100
#define ETB_STATUS_OFFSET_DUTY       6 // int16, x10000 (-1.0..+1.0 -> -10000..+10000)

// ETB_PID_STATUS (0x301) byte layout - see can_bus.c's can_tx_pid_status()
#define ETB_PID_STATUS_OFFSET_ITERM         0 // int16, x100
#define ETB_PID_STATUS_OFFSET_DTERM         2 // int16, x100
#define ETB_PID_STATUS_OFFSET_CURRENT_SENSE 4 // uint16, raw ADC
#define ETB_PID_STATUS_OFFSET_STATUS        6 // uint8, etb_status_t
#define ETB_PID_STATUS_OFFSET_SEQ           7 // uint8, tx sequence

// ETB_RAW (0x302) byte layout - see can_bus.c's can_tx_raw()
#define ETB_RAW_OFFSET_TPS1   0 // uint16, raw ADC 0-4095
#define ETB_RAW_OFFSET_TPSB   2 // uint16
#define ETB_RAW_OFFSET_PEDAL1 4 // uint16
#define ETB_RAW_OFFSET_PEDAL2 6 // uint16

// etb_status_t (both ETB_STATUS byte1 and ETB_PID_STATUS byte6) - see can_bus.h
enum class EtbCanStatus : uint8_t {
	Fault = 0,     // ADC/CAN-stack fault, forced disable regardless of command
	Disabled = 1,  // no live command (stale/never received) - fail-safe default
	Normal = 2,    // closed-loop PID, tracking ETB_TARGET's targetPosition
	OpenLoop = 3,  // direct duty override (bench test / auto-calibrate sweep)
};

// ETB_TARGET (0x306) mode byte6 - see can_bus.h's can_command_state_t.mode
enum class EtbCanMode : uint8_t {
	Normal = 0,
	OpenLoop = 1,
	// NOT YET IMPLEMENTED on the board (see file header NOTE): while in this mode, the board should
	// run its own local relay/bang-bang autotune (same Åström-Hägglund method as
	// EtbController::getClosedLoopAutotune(), electronic_throttle.cpp - oscillate duty at a fixed
	// amplitude around ETB_TARGET's targetPosition, measure period/amplitude, derive Kp/Ki/Kd) and
	// periodically report its current guess via ETB_AUTOTUNE_STATUS_1/2. rusEFI sends this mode
	// periodically (same as Normal) while engine->etbAutoTune is set (can_etb_remote.cpp's
	// sendExternalEtbTarget()) - stopping is the same "go quiet / switch mode" signal as everywhere
	// else in this protocol, no separate stop command.
	Autotune = 2,
};

// ETB_TARGET (0x306) byte layout - see can_bus.c's handle_frame() CAN_ID_ETB_TARGET branch
#define ETB_TARGET_OFFSET_POSITION       0 // float32, percent, meaningful only when mode == Normal
#define ETB_TARGET_OFFSET_BENCH_DUTY_RAW 4 // int16, x10000 (-1.0..+1.0), meaningful only when mode == OpenLoop
#define ETB_TARGET_OFFSET_MODE           6 // uint8, EtbCanMode
#define ETB_TARGET_OFFSET_SEQ            7 // uint8, tx sequence

// ETB_AUTOTUNE_STATUS_1/2 (0x309/0x30A) byte layout - NOT YET IMPLEMENTED on the board, proposed
// to mirror ETB_GAINS_1/2's raw-float32 layout exactly (infrequent, no precision-loss concern).
#define ETB_AUTOTUNE_STATUS_1_OFFSET_PFACTOR 0 // float32
#define ETB_AUTOTUNE_STATUS_1_OFFSET_IFACTOR 4 // float32
#define ETB_AUTOTUNE_STATUS_2_OFFSET_DFACTOR 0 // float32

// ETB_CAL_TPS (0x307) / ETB_CAL_PEDAL (0x308) byte layout - see can_bus.c's handle_frame()
#define ETB_CAL_OFFSET_CH1_MIN 0 // uint16, raw ADC
#define ETB_CAL_OFFSET_CH1_MAX 2 // uint16
#define ETB_CAL_OFFSET_CH2_MIN 4 // uint16
#define ETB_CAL_OFFSET_CH2_MAX 6 // uint16

/**
 * DcMotor that drives the external CH32 ETB board(s) over CAN instead of a local h-bridge: every
 * set()/disable() sends ETB_TARGET with mode=OpenLoop and the given duty as benchDutyOverride.
 *
 * Used only for bench-test/auto-calibrate (RUSEFI_SIDE_TODO.md #3.2) - normal closed-loop
 * operation is a completely separate periodic component (#3.1, see sendExternalEtbTarget() below)
 * that never touches this class. EtbController::init() wires this in as m_motor for any throttle
 * (DC_Throttle1 or DC_Throttle2) with enableExternalCanEtb set (electronic_throttle.cpp), which is
 * what lets startBenchTest()/doAutocal() (electronic_throttle_impl.h) drive it unmodified via the
 * existing DcMotor interface - see #3.2's "would actually work unmodified" note.
 */
class CanDcMotor : public DcMotor {
public:
	bool set(float duty) override;
	float get() const override { return m_duty; }
	void enable() override;
	void disable(const char* msg) override;
	bool isOpenDirection() const override { return m_duty >= 0; }

private:
	void sendTarget();

	float m_duty = 0;
	uint8_t m_seq = 0;
};

// Single shared instance for both DC_Throttle1 and DC_Throttle2: the wire protocol has one base ID
// with no per-throttle addressing by design - a dual-throttle-body engine in this mode is two
// physical boards on the same bus/ID, both mirroring whatever this sends (see
// sendExternalEtbTarget() below for the normal-operation equivalent of this sharing). See
// electronic_throttle.cpp for how both throttle instances end up pointing m_motor at this same
// instance.
extern CanDcMotor externalEtbCanMotor;

// Physical CAN bus index (0/1/2) the external ETB board is wired to -
// (int)engineConfiguration->canEtbBusIndex, same can_broadcast_channel_e dropdown type
// canBroadcastUseChannel already uses (can_verbose.cpp/can_dash.cpp). Every send in this file
// goes through this instead of a hardcoded bus, so it's the one place to look if traffic isn't
// reaching the board.
size_t getExternalEtbBus();

// #3.1: periodic remote-target component, called from can_tx.cpp's CanWrite::PeriodicTask().
// No-ops (and never actually called, since the caller also checks the config flag) when
// EFI_CAN_SUPPORT is off, so electronic_throttle.cpp/.h don't need their own CAN guards.
void sendExternalEtbGains();
void sendExternalEtbTarget();

// #3.2/#6: sends the auto-calibrate sweep's raw ADC endpoints. Called from
// electronic_throttle_impl.h's doAutocalExternalCan() once a sweep completes.
void sendExternalEtbCalTps(uint16_t rawMin1, uint16_t rawMax1, uint16_t rawMin2, uint16_t rawMax2);

// Pedal equivalent of sendExternalEtbCalTps() - sent whenever engineConfiguration->canEtbPedal1/2
// RawMin/Max change (grabPedalIsUp()/grabPedalIsWideOpen(), tps.cpp), and periodically alongside
// it (see sendExternalEtbCalibration() below).
void sendExternalEtbCalPedal(uint16_t rawMin1, uint16_t rawMax1, uint16_t rawMin2, uint16_t rawMax2);

// Re-sends both ETB_CAL_TPS/PEDAL from the persisted engineConfiguration->canEtb*RawMin/Max -
// called periodically from can_tx.cpp, same reasoning as sendExternalEtbGains(): the board doesn't
// remember calibration across its own reset (see can_etb.h's canEtbTps1RawMin comment in
// rusefi_config.txt), so this needs to be resent, not just transmitted once when it's determined.
void sendExternalEtbCalibration();

// #3.1 RX helper (init_etb_can.cpp): latest ETB_RAW TPS1/TPSB, for the auto-calibrate sweep to
// read instead of the local ADC voltages doAutocal() normally captures. Returns false if no
// ETB_RAW frame has arrived recently (same 3x-telemetry-period staleness margin as the CanSensors).
bool getExternalEtbRawTps(uint16_t& tps1, uint16_t& tpsB);
