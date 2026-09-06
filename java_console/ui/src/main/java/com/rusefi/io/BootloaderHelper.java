package com.rusefi.io;

import com.devexperts.logging.Logging;
import com.rusefi.UiProperties;
import com.rusefi.autoupdate.Autoupdate;
import com.rusefi.core.RusEfiSignature;
import com.rusefi.core.SignatureHelper;
import com.rusefi.core.io.BoardCompatibility;
import com.rusefi.core.io.BundleUtil;

import javax.swing.*;

import static com.devexperts.logging.Logging.getLogging;
import static com.rusefi.Timeouts.SECOND;
import static com.rusefi.binaryprotocol.BinaryProtocol.sleep;

/**
 * this code is shared between DFU and OpenBLT tracks
 */
public class BootloaderHelper {
    private static final Logging log = getLogging(BootloaderHelper.class);

    public static boolean sendBootloaderRebootCommand(JComponent parent, String signature, IoStream stream, UpdateOperationCallbacks callbacks, String command) {
        RusEfiSignature controllerSignature = SignatureHelper.parse(signature);
        String fileSystemBundleTarget = BundleUtil.getBundleTarget();
        if (fileSystemBundleTarget != null && controllerSignature != null) {
            String ecuTarget = controllerSignature.getBundleTarget();
            // hack: QC firmware self-identifies as "normal" not QC firmware :( [tag:QC_firmware]
            boolean ownBoard = fileSystemBundleTarget.equalsIgnoreCase(ecuTarget) || fileSystemBundleTarget.contains("_QC_");
            if (!ownBoard && !UiProperties.skipEcuTypeDetection()) {
                if (BoardCompatibility.matchesCompatibility(ecuTarget)) {
                    // #9714:  fetch the matching firmware on demand before rebooting to bootloader.
                    callbacks.logLine("[universal_bundle]: downloading firmware for \"" + ecuTarget + "\"...");
                    if (!Autoupdate.ensureFirmwareForTarget(ecuTarget, callbacks::updateProgress, callbacks::logLine)) {
                        SwingUtilities.invokeLater(() -> JOptionPane.showMessageDialog(parent, String.format(
                            "Universal bundle could not download firmware for \"%s\".\nPlease check your internet connection and retry.", ecuTarget)));
                        return false;
                    }
                } else {
                    String message = String.format("You have \"%s\" controller does not look right to program it with \"%s\"", ecuTarget, fileSystemBundleTarget);
                    log.info(message);

                    if (confirmFlashAnyway(parent, ecuTarget, fileSystemBundleTarget)) {
                        log.info("FORCED: proceeding to program \"" + ecuTarget + "\" with \"" + fileSystemBundleTarget + "\" firmware (user override).");
                        // fall through to sendBootloaderRebootCommand() below, same as the ownBoard/compatible case
                    } else {
                        SwingUtilities.invokeLater(() -> {
                            JOptionPane.showMessageDialog(parent, message);
                            // in case of mismatched bundle type we are supposed do close connection
                            // and properly handle the case of user hitting "Update Firmware" again
                            // closing connection is a mess on Windows so it's simpler to just exit
                            new Thread(() -> {
                                // let's have a delay and separate thread to address
                                // "wrong bundle" warning text sometimes not visible #3267
                                sleep(5 * SECOND);
                                System.exit(-5);
                            }).start();
                        });

                        return false;
                    }
                }
            }
        }

        BootloaderCommsHelper.sendBootloaderRebootCommand(stream, callbacks, command);
        return true;
    }

    /**
     * Fail-safe-but-escapable version of the board-mismatch block above: lets a user who knows what
     * they're doing (e.g. migrating an ECU from a since-renamed/split bundle target, such as a plain
     * "paralela" board being moved onto a "paralela_f427" bundle) proceed anyway instead of getting
     * the app killed. Blocks the calling thread for the answer; fails closed (no flash) if interrupted.
     */
    private static boolean confirmFlashAnyway(JComponent parent, String ecuTarget, String fileSystemBundleTarget) {
        String message = String.format(
            "DANGER: connected controller identifies as \"%s\", but this bundle programs \"%s\".\n\n" +
                "This is almost certainly the WRONG firmware for this board and could brick it.\n\n" +
                "Flash \"%s\" firmware anyway?", ecuTarget, fileSystemBundleTarget, fileSystemBundleTarget);
        final boolean[] confirmed = {false};
        Runnable ask = () -> confirmed[0] = JOptionPane.showConfirmDialog(
            parent, message, "Firmware / board mismatch", JOptionPane.OK_CANCEL_OPTION, JOptionPane.ERROR_MESSAGE) == JOptionPane.OK_OPTION;
        try {
            if (SwingUtilities.isEventDispatchThread()) {
                ask.run();
            } else {
                SwingUtilities.invokeAndWait(ask);
            }
        } catch (Exception e) {
            log.warn("confirmFlashAnyway interrupted, treating as cancel: " + e);
            return false;
        }
        return confirmed[0];
    }
}
