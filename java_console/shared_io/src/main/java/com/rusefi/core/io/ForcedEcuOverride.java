package com.rusefi.core.io;

import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Ports the user has explicitly told the console to connect to despite an unsupported/incompatible
 * ECU signature ("I know what I'm doing" in {@code UnsupportedEcuCardHost}). Once forced, a port stays
 * forced until it disappears from the hardware list — this keeps both the background port scanner
 * (which opens a fresh, throwaway {@code LinkManager} per probe) and the real connect attempt agreeing,
 * so the user can read the ECU's own settings (via its own signature/.ini, see
 * {@code BinaryProtocol.connectAndReadConfiguration}) to migrate them, then flash whatever firmware they
 * choose. Session-lifetime only; never persisted, so nothing is silently bypassed across restarts.
 */
public final class ForcedEcuOverride {
    private static final Set<String> forcedPorts = ConcurrentHashMap.newKeySet();

    private ForcedEcuOverride() {
    }

    public static void force(String port) {
        if (port != null) {
            forcedPorts.add(port);
        }
    }

    public static void clear(String port) {
        if (port != null) {
            forcedPorts.remove(port);
        }
    }

    public static boolean isForced(String port) {
        return port != null && forcedPorts.contains(port);
    }

    public static void clearForTests() {
        forcedPorts.clear();
    }
}
