#pragma once

/**
 * POSIX/ChibiOS simulator internal-flash stand-in.
 *
 * Call once at process start, before loadConfiguration(), so the previous
 * run's tune is versioned before this boot reads or rewrites flash.
 */
void simulatorFlashInitOnBoot();

/**
 * Pin the flash root directory (primary.bin/backup.bin/tunes live here).
 * For in-process (DLL) hosts: the firmware resolves this path on every access,
 * and an environment variable is not reliable across mixed CRTs (the host's
 * _putenv is invisible to a MinGW DLL's getenv), so hosts pass the directory
 * explicitly through ecu_init instead of relying on the process CWD.
 */
void simulatorFlashSetRoot(const char *path);
