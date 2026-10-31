#pragma once

#include <cstdint>

// Virtual output edges belong to their scheduled instants. Host thread wake-up
// latency is not injector hardware latency. This context is used only by the
// POSIX host simulator; it never changes the ECU clock or hardware callbacks.
class SimulatorEventTimeScope {
public:
	explicit SimulatorEventTimeScope(int64_t deadline) : previous(current) { current = deadline; }
	~SimulatorEventTimeScope() { current = previous; }
	SimulatorEventTimeScope(const SimulatorEventTimeScope&) = delete;
	SimulatorEventTimeScope& operator=(const SimulatorEventTimeScope&) = delete;
	static int64_t currentOr(int64_t fallback) { return current != 0 ? current : fallback; }

private:
	inline static thread_local int64_t current = 0;
	int64_t previous;
};
