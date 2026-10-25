// closed_loop_fuel.h

#pragma once

#include "closed_loop_fuel_cell.h"
#include "deadband.h"
#include "short_term_fuel_trim_state_generated.h"

struct stft_s;
enum class EngineStateMachineState : uint8_t;

struct ClosedLoopFuelResult {
	ClosedLoopFuelResult() {
		// Default is no correction, aka 1.0 multiplier
		for (size_t i = 0; i < FT_BANK_COUNT; i++) {
			banks[i] = 1.0f;
		}
		region = ftRegionIdle;
	}

	float banks[FT_BANK_COUNT];
	ft_region_e region;
};

struct FuelingBank {
	ClosedLoopFuelCellImpl cells[STFT_CELL_COUNT];
};

class ShortTermFuelTrim : public EngineModule, public short_term_fuel_trim_state_s {
public:
	void init(stft_s *stftCfg);
	// EngineModule implementation
	void onSlowCallback() override;
	bool needsDelayedShutoff() override;

	ClosedLoopFuelResult getCorrection(float rpm, float fuelLoad);

	// Maps an Engine State Machine state onto one of the existing STFT region cells. Used when
	// "state based STFT" is enabled so the SM state, rather than RPM/load thresholds, selects
	// the active trim cell. Public/static so it can be unit-tested directly.
	static ft_region_e regionForSmState(EngineStateMachineState state);

#if ! EFI_UNIT_TEST
private:
#endif
	FuelingBank banks[FT_BANK_COUNT];

	Deadband<25> idleDeadband;
	Deadband<2> overrunDeadband;
	Deadband<2> loadDeadband;

	SensorType getSensorForBankIndex(size_t index);
	ft_region_e computeStftBin(float rpm, float load, stft_s& cfg);
	stft_state_e getCorrectionState();
	stft_state_e getLearningState(SensorType sensor);
};

void initStft(void);

/* TODO: move out of here */
bool checkIfTuningVeNow();
