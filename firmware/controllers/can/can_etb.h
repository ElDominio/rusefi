/**
 * @file can_etb.h
 *
 * CAN protocol for the external CH32V203-based ETB controller (see the external-etb project,
 * ../../external-etb/CH32V203_ETB_CONTROLLER.md and ../../external-etb/rusefi/RUSEFI_SIDE_TODO.md).
 *
 * This mirrors the board side's can_bus.h exactly (../../external-etb/firmware/src/can_bus.h) -
 * keep the two in sync by hand, there is no shared codegen between the two repos.
 *
 * Uses extended (29-bit) CAN IDs in a private block, CAN_ETB_BASE_ID + 0..15, rather than the
 * standard 11-bit 0x300-0x30F range this protocol used originally. Standard IDs in that range
 * are not free of conflicts on real vehicle buses - this same file tree's can_dash.cpp already
 * claims 0x300 (CAN_MAZDA_RX_STEERING_WARNING) and 0x308 (W202_STAT_1) for OEM dash decoding, and
 * RUSEFI_SIDE_TODO.md #5.1 flagged 0x300 as an unconfirmed placeholder for exactly this reason.
 * The extended block follows the same private-ID convention already used elsewhere in this file
 * tree (BENCH_TEST_BASE_ADDRESS 0x770000, GDI4_BASE_ADDRESS 0xBB20 - both in can_common.h) -
 * see CanListener::acceptFrame()/CAN_ID() (can.h): matching is purely by numeric ID value
 * regardless of standard/extended framing, so a private block only needs to avoid other IDs
 * actually in use, not just its own frame-type namespace.
 *
 * CAN_ID_ETB_AUTOTUNE_STATUS_1/2 and EtbCanMode::Autotune (below) are implemented on both sides:
 * the board runs its own relay autotune (external-etb/firmware/src/autotune.c) whenever
 * ETB_TARGET's mode byte is Autotune, and reports its current gain estimate alongside its other
 * telemetry (main.c's can_tx_autotune_status(), same 10Hz divider as the rest).
 */

#pragma once

#include <cstdint>
#include "dc_motor.h"

/**
 * The single gate for "is this ECU actually driving an external CAN ETB board right now?".
 *
 * Folds the compile-time feature flag into the runtime config bit deliberately: enableExternalCanEtb
 * lives in the shared config struct (rusefi_config.txt), so it exists - and is writable, via the
 * console/Lua value lookup or by importing a tune from a board that does have this feature - on
 * every board, including the ones built with EFI_EXTERNAL_CAN_ETB off. Honouring the bare bit in
 * shared code (init_tps.cpp's virtual TPS/pedal channels were the live example) would then hand
 * those boards a throttle and pedal wired to a CAN feed that no listener is registered for
 * (initExternalCanEtbSensors() is a stub in that build), i.e. permanently invalid TPS/PPS with no
 * diagnostic explaining why. Route every runtime check through here so the bit is inert unless the
 * code that services it was actually compiled in.
 */
static inline bool isExternalCanEtbEnabled() {
#if EFI_EXTERNAL_CAN_ETB
	return engineConfiguration->enableExternalCanEtb;
#else // !EFI_EXTERNAL_CAN_ETB
	return false;
#endif // EFI_EXTERNAL_CAN_ETB
}

// The external board's ADC: a fixed 5V rail with no divider network, sampled by its 12-bit ADC
// (raw 0-4095) - a hardware fact (CH32V203_ETB_CONTROLLER.md 1.1), not a tunable. Used in both
// directions: can_etb_remote.cpp's sendExternalEtbCalibration() converts
// engineConfiguration->tpsMin/tpsMax/tps1SecondaryMin/tps1SecondaryMax (volts) into these raw
// counts for ETB_CAL_TPS, and init_etb_can.cpp's EtbCanRawListener converts ETB_RAW's raw counts
// back into volts to feed rusEFI's own TPS1/TPSB calibration curve (init_tps.cpp's "virtual
// channel" support, tps.h's postExternalCanEtbRawTps()) - the same curve a local ADC pin uses,
// just fed from CAN instead of a physical channel.
constexpr float CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS = 5.0f;
constexpr float CAN_ETB_BOARD_ADC_MAX_COUNT = 4095.0f;

#define CAN_ETB_BASE_ID 0x790000u

// Telemetry: CH32 -> rusEFI, extended 29-bit frames, DLC 8
#define CAN_ID_ETB_STATUS     (CAN_ETB_BASE_ID + 0)
#define CAN_ID_ETB_PID_STATUS (CAN_ETB_BASE_ID + 1)
#define CAN_ID_ETB_RAW        (CAN_ETB_BASE_ID + 2)
// Board's current best pFactor/iFactor
// (frame 1) and dFactor (frame 2) guess while running PID autotune (mode == Autotune below),
// re-sent periodically ("keeps trying" - there is no "done", same as the local relay-autotune in
// electronic_throttle.cpp, which also runs until the user stops it).
#define CAN_ID_ETB_AUTOTUNE_STATUS_1 (CAN_ETB_BASE_ID + 9)
#define CAN_ID_ETB_AUTOTUNE_STATUS_2 (CAN_ETB_BASE_ID + 10)
// The feedforward term the board actually applied this tick (0 outside NORMAL/AUTOTUNE) - reported
// back rather than recomputed locally from engineConfiguration->etbBiasBins/Values, since the
// board is the authority on what it actually used (its own copy can briefly lag rusEFI's after a
// curve edit, until the next periodic ETB_BIAS_1..4 resend - see can_tx.cpp).
#define CAN_ID_ETB_FEEDFORWARD (CAN_ETB_BASE_ID + 15)

// Commands: rusEFI -> CH32, extended 29-bit frames, DLC 8
#define CAN_ID_ETB_LIMITS     (CAN_ETB_BASE_ID + 3)
#define CAN_ID_ETB_GAINS_1    (CAN_ETB_BASE_ID + 4)
#define CAN_ID_ETB_GAINS_2    (CAN_ETB_BASE_ID + 5)
#define CAN_ID_ETB_TARGET     (CAN_ETB_BASE_ID + 6)
#define CAN_ID_ETB_CAL_TPS    (CAN_ETB_BASE_ID + 7)
#define CAN_ID_ETB_CAL_PEDAL  (CAN_ETB_BASE_ID + 8)
// Feedforward/bias curve (engineConfiguration->etbBiasBins/etbBiasValues, ETB_BIAS_CURVE_LENGTH=8
// points from rusefi_config.txt): 2 points per frame, matching EtbController::getOpenLoop()'s
// interpolate2d(target, etbBiasBins, etbBiasValues) - the board runs the same interpolation
// (feedforward.c) and adds it on top of its own PID output, same "openLoop + closedLoop" split
// ClosedLoopController::update() does here.
#define CAN_ID_ETB_BIAS_1     (CAN_ETB_BASE_ID + 11)
#define CAN_ID_ETB_BIAS_2     (CAN_ETB_BASE_ID + 12)
#define CAN_ID_ETB_BIAS_3     (CAN_ETB_BASE_ID + 13)
#define CAN_ID_ETB_BIAS_4     (CAN_ETB_BASE_ID + 14)

// ETB_STATUS (CAN_ETB_BASE_ID+0) byte layout - see can_bus.c's can_tx_status()
#define ETB_STATUS_OFFSET_VERSION    0
#define ETB_STATUS_OFFSET_STATUS     1
#define ETB_STATUS_OFFSET_TPS1       2 // int16, x100 (scaled_percent / PACK_MULT_PERCENT)
#define ETB_STATUS_OFFSET_TPSB       4 // int16, x100
#define ETB_STATUS_OFFSET_DUTY       6 // int16, x10000 (-1.0..+1.0 -> -10000..+10000)

// ETB_PID_STATUS (CAN_ETB_BASE_ID+1) byte layout - see can_bus.c's can_tx_pid_status()
#define ETB_PID_STATUS_OFFSET_ITERM         0 // int16, x100
#define ETB_PID_STATUS_OFFSET_DTERM         2 // int16, x100
#define ETB_PID_STATUS_OFFSET_CURRENT_SENSE 4 // uint16, raw ADC
#define ETB_PID_STATUS_OFFSET_STATUS        6 // uint8, etb_status_t
#define ETB_PID_STATUS_OFFSET_SEQ           7 // uint8, tx sequence

// ETB_RAW (CAN_ETB_BASE_ID+2) byte layout - see can_bus.c's can_tx_raw()
#define ETB_RAW_OFFSET_TPS1   0 // uint16, raw ADC 0-4095
#define ETB_RAW_OFFSET_TPSB   2 // uint16
#define ETB_RAW_OFFSET_PEDAL1 4 // uint16
#define ETB_RAW_OFFSET_PEDAL2 6 // uint16

// ETB_LIMITS (CAN_ETB_BASE_ID+3) byte layout - see can_bus.c's handle_frame() CAN_ID_ETB_LIMITS branch.
// int16, x100 each (not float32 - these are small, bounded percent values, same convention as
// the telemetry frames above). iTermMin/iTermMax feed the board's separate iTerm clamp
// (pid_gains_t.iTermMin/iTermMax); minValue/maxValue feed its output clamp - mirrors the two-
// clamp split rusEFI's own Pid class uses (iTermMin/iTermMax vs parameters->minValue/maxValue).
#define ETB_LIMITS_OFFSET_ITERM_MIN 0
#define ETB_LIMITS_OFFSET_ITERM_MAX 2
#define ETB_LIMITS_OFFSET_MIN_VALUE 4
#define ETB_LIMITS_OFFSET_MAX_VALUE 6

// ETB_BIAS_1..4 (CAN_ETB_BASE_ID+11..14) byte layout - see can_bus.c's handle_frame() CAN_ID_ETB_BIAS_N
// branches. Two curve points per frame, int16 x100 each: point 2*N is (BIN_A, VALUE_A), point
// 2*N+1 is (BIN_B, VALUE_B) - e.g. ETB_BIAS_1 carries etbBiasBins[0]/etbBiasValues[0] and
// etbBiasBins[1]/etbBiasValues[1].
#define ETB_BIAS_OFFSET_BIN_A   0
#define ETB_BIAS_OFFSET_VALUE_A 2
#define ETB_BIAS_OFFSET_BIN_B   4
#define ETB_BIAS_OFFSET_VALUE_B 6

// ETB_FEEDFORWARD (CAN_ETB_BASE_ID+15) byte layout - see can_bus.c's can_tx_feedforward(). The feedforward
// term actually applied this tick (0 outside NORMAL/AUTOTUNE) - populates
// IEtbController::setFeedForward()'s target field rather than rusEFI recomputing
// interpolate2d() locally, since the board is the authority on what it actually used.
#define ETB_FEEDFORWARD_OFFSET_VALUE  0 // int16, x100
#define ETB_FEEDFORWARD_OFFSET_STATUS 6 // uint8, etb_status_t
#define ETB_FEEDFORWARD_OFFSET_SEQ    7 // uint8, tx sequence

// etb_status_t (both ETB_STATUS byte1 and ETB_PID_STATUS byte6) - see can_bus.h
enum class EtbCanStatus : uint8_t {
	Fault = 0,     // ADC/CAN-stack fault, forced disable regardless of command
	Disabled = 1,  // no live command (stale/never received) - fail-safe default
	Normal = 2,    // closed-loop PID, tracking ETB_TARGET's targetPosition
	OpenLoop = 3,  // direct duty override (bench test / auto-calibrate sweep)
	Autotune = 4,  // relay/bang-bang autotune (board's autotune.h), tracking targetPosition
};

// ETB_TARGET (CAN_ETB_BASE_ID+6) mode byte6 - see can_bus.h's can_command_state_t.mode
enum class EtbCanMode : uint8_t {
	Normal = 0,
	OpenLoop = 1,
	// While in this mode the board runs its own local relay/bang-bang autotune (same
	// Åström-Hägglund method as EtbController::getClosedLoopAutotune(), electronic_throttle.cpp -
	// oscillate duty at a fixed amplitude around ETB_TARGET's targetPosition, measure
	// period/amplitude, derive Kp/Ki/Kd - see external-etb/firmware/src/autotune.c, which matches
	// rusEFI's autotuneAmplitude) and reports its current guess via ETB_AUTOTUNE_STATUS_1/2
	// alongside its other telemetry. rusEFI sends this mode
	// periodically (same as Normal) while engine->etbAutoTune is set (can_etb_remote.cpp's
	// sendExternalEtbTarget()) - stopping is the same "go quiet / switch mode" signal as everywhere
	// else in this protocol, no separate stop command.
	Autotune = 2,
};

// ETB_TARGET (CAN_ETB_BASE_ID+6) byte layout - see can_bus.c's handle_frame() CAN_ID_ETB_TARGET branch
#define ETB_TARGET_OFFSET_POSITION       0 // float32, percent, meaningful only when mode == Normal
#define ETB_TARGET_OFFSET_BENCH_DUTY_RAW 4 // int16, x10000 (-1.0..+1.0), meaningful only when mode == OpenLoop
#define ETB_TARGET_OFFSET_MODE           6 // uint8, EtbCanMode
#define ETB_TARGET_OFFSET_SEQ            7 // uint8, tx sequence

// ETB_AUTOTUNE_STATUS_1/2 (CAN_ETB_BASE_ID+9/+10) byte layout - NOT YET IMPLEMENTED on the board, proposed
// to mirror ETB_GAINS_1/2's raw-float32 layout exactly (infrequent, no precision-loss concern).
#define ETB_AUTOTUNE_STATUS_1_OFFSET_PFACTOR 0 // float32
#define ETB_AUTOTUNE_STATUS_1_OFFSET_IFACTOR 4 // float32
#define ETB_AUTOTUNE_STATUS_2_OFFSET_DFACTOR 0 // float32

// ETB_CAL_TPS (CAN_ETB_BASE_ID+7) / ETB_CAL_PEDAL (CAN_ETB_BASE_ID+8) byte layout - see can_bus.c's handle_frame()
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

// iTerm/output limits (ETB_LIMITS) and feedforward/bias curve (ETB_BIAS_1..4) - same periodic-
// resend reasoning as sendExternalEtbGains(): both rarely change and the board forgets them on
// its own reset. Called from can_tx.cpp alongside sendExternalEtbGains().
void sendExternalEtbLimits();
void sendExternalEtbBiasCurve();

// #3.2/#6: sends the auto-calibrate sweep's raw ADC endpoints. Called from
// electronic_throttle_impl.h's doAutocalExternalCan() once a sweep completes.
void sendExternalEtbCalTps(uint16_t rawMin1, uint16_t rawMax1, uint16_t rawMin2, uint16_t rawMax2);

// Pedal equivalent of sendExternalEtbCalTps() - sent whenever engineConfiguration->canEtbPedal1/2
// RawMin/Max change (grabPedalIsUp()/grabPedalIsWideOpen(), tps.cpp), and periodically alongside
// it (see sendExternalEtbCalibration() below).
void sendExternalEtbCalPedal(uint16_t rawMin1, uint16_t rawMax1, uint16_t rawMin2, uint16_t rawMax2);

// Re-sends both ETB_CAL_TPS/PEDAL - TPS from engineConfiguration->tpsMin/tpsMax/tps1SecondaryMin/
// tps1SecondaryMax (volts, converted to raw here - see CAN_ETB_BOARD_ADC_FULL_SCALE_VOLTS above),
// pedal from the persisted canEtbPedal1/2RawMin/Max - called periodically from can_tx.cpp, same
// reasoning as sendExternalEtbGains(): the board doesn't remember calibration across its own
// reset, so this needs to be resent, not just transmitted once when it's determined.
void sendExternalEtbCalibration();

// #3.1 RX helper (init_etb_can.cpp): latest ETB_RAW TPS1/TPSB, for the auto-calibrate sweep to
// read instead of the local ADC voltages doAutocal() normally captures. Returns false if no
// ETB_RAW frame has arrived recently (same 3x-telemetry-period staleness margin as the CanSensors).
bool getExternalEtbRawTps(uint16_t& tps1, uint16_t& tpsB);
