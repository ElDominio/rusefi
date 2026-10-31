#pragma once
// EngineSim's named M controls are supported by the normal firmware build too.
// Return 0 for an ordinary sensor name, 1 for valid EngineSim input, -1 if invalid.
int validateEngineSimInput(const char* name, float value);
void stageEngineSimInput(const char* name, float value);
// The same seam by input index (1..8, 0 = not an EngineSim name): a name resolved once, then staged by index.
int engineSimInputIndex(const char* name);
int validateEngineSimInputIndex(int index, float value);
void stageEngineSimInputIndex(int index, float value);
void commitEngineSimInputs();
void releaseEngineSimInputs();
bool engineSimTriggerActive();
// In-process (DLL) mode: the plant commits at its physics cadence, so poll() runs
// synchronously from commitEngineSimInputs() instead of self-re-arming on the
// scheduler (the two otherwise double-integrate the shared crank phase).
void engineSimSetSynchronousPoll(bool on);
void engineSimSetWheelOffset(double deg);
float engineSimGetWheelPhaseDrift();
#if defined(EFI_SIMULATOR_POSIX)
struct EcuSimDiagnostics;
void engineSimReadDiagnostics(EcuSimDiagnostics& out);
int engineSimAdvancePlantTo(efitick_t target, float plantAngle, float plantSpeed);
#endif
// EngineSimStartButton: -1 = not mocked, else the plant operator's start/stop button (1 = pressed).
int engineSimStartButton();
