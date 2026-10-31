/**
 * POSIX simulator reboot.
 *
 * The host process does not have NVIC_SystemReset(), and rusefi.cpp (which
 * calls it) is not linked into this binary. Console `reboot` and the
 * TunerStudio Reboot button (Z / 0xBB) re-exec this process after the
 * file-backed flash is idle, so a burn is not torn and the process does
 * not abort. The PID stays the same. Uptime (`seconds`) restarts at 0.
 */
#pragma once

void simRebootRememberArgs(int argc, char** argv);
void simRequestReboot();
void simRebootPoll();

/**
 * Write the current process id to `simulator.pid` in the working directory.
 *
 * The driver (simulator/tools/sim_engine.py) uses this to find the process
 * again after a reboot: POSIX execv keeps the pid, but the Windows _execv
 * fallback starts a new process, so the recorded pid would go stale.
 */
void simWritePidFile();
