/**
 * Portable socket boundary for the C++ host simulator.
 *
 * Keep platform socket headers out of firmware translation units: pch.h includes FatFS, whose
 * DWORD/WCHAR names conflict with windows.h. The implementation is therefore deliberately opaque.
 */
#pragma once

#include <cstdint>

using sim_socket_t = intptr_t;
constexpr sim_socket_t SIM_INVALID_SOCKET = static_cast<sim_socket_t>(-1);

bool simNetValid(sim_socket_t socket);
void simNetInit();
sim_socket_t simNetListen(int port);
sim_socket_t simNetAccept(sim_socket_t listener);
int simNetWaitReadable(sim_socket_t socket, int timeoutMs);
int simNetRecv(sim_socket_t socket, void* buffer, int length);
int simNetSend(sim_socket_t socket, const void* buffer, int length);
void simNetClose(sim_socket_t socket);
const char* simNetLastError();

// Replace this process with the same executable and argv. Returns only on failure.
int simReexec(char** argv);
