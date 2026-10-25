package com.rusefi.maintenance.migration.migrators;

import com.opensr5.ini.field.EnumIniField;
import com.opensr5.ini.field.IniField;
import com.rusefi.maintenance.migration.TuneMigrationContext;
import com.rusefi.tune.xml.Constant;

import java.util.Optional;

import static javax.management.ObjectName.quote;

/**
 * [tag:alphax_upstream_sync_migration] AlphaX features adopted from upstream on the 2026-10-25
 * master sync moved from TS page 6 into the main configuration under the same names, which
 * {@link DefaultTuneMigrator} restores by itself (it resolves fields on every burnable page).
 * This migrator covers the few settings it would refuse:
 * <ul>
 *   <li>{@code secondaryFpActivationLoad}/{@code secondaryFpLoadHysteresis}: units "kPa" became
 *       "load". The stored value is unchanged (fueling load, which is MAP kPa for speed density).</li>
 *   <li>{@code ossMode}: AlphaX had a separate Output Shaft Speed mode. Upstream reads the pin
 *       when assigned and leaves the sensor to Lua/CAN otherwise, so any non-"Physical Pin" mode
 *       must leave {@code outputShaftSpeedSensorPin} unassigned.</li>
 * </ul>
 */
public enum AlphaxUpstreamSyncMigrator implements TuneMigrator {
    INSTANCE;

    static final String OSS_MODE = "ossMode";
    static final String OSS_PIN = "outputShaftSpeedSensorPin";
    static final String OSS_PHYSICAL_PIN_MODE = "Physical Pin";

    @Override
    public void migrateTune(final TuneMigrationContext context) {
        migrateKpaToLoad(context, "secondaryFpActivationLoad");
        migrateKpaToLoad(context, "secondaryFpLoadHysteresis");
        migrateOutputShaftSpeedMode(context);
    }

    private static void migrateKpaToLoad(final TuneMigrationContext context, final String name) {
        final Optional<IniField> prevField = context.getPrevIniFile().findIniField(name);
        final Optional<IniField> newField = context.getUpdatedIniFile().findIniField(name);
        if (!prevField.isPresent() || !newField.isPresent()
            || !"kPa".equals(prevField.get().getUnits()) || !"load".equals(newField.get().getUnits())) {
            return;
        }
        final Constant prevValue = context.getPrevTune().getConstantsAsMap().get(name);
        final Constant newValue = context.getUpdatedTune().getConstantsAsMap().get(name);
        if (prevValue == null || prevValue.getValue() == null || newValue == null) {
            return;
        }
        context.addMigration(name, newValue.cloneWithValue(prevValue.getValue()));
    }

    private static void migrateOutputShaftSpeedMode(final TuneMigrationContext context) {
        // Only when upgrading from an AlphaX ini that still had the separate mode selector.
        if (!context.getPrevIniFile().findIniField(OSS_MODE).isPresent()
            || context.getUpdatedIniFile().findIniField(OSS_MODE).isPresent()) {
            return;
        }
        final Constant mode = context.getPrevTune().getConstantsAsMap().get(OSS_MODE);
        final Optional<IniField> newPin = context.getUpdatedIniFile().findIniField(OSS_PIN);
        final Constant newPinValue = context.getUpdatedTune().getConstantsAsMap().get(OSS_PIN);
        if (mode == null || mode.getValue() == null || !newPin.isPresent()
            || !(newPin.get() instanceof EnumIniField) || newPinValue == null) {
            return;
        }
        if (quote(OSS_PHYSICAL_PIN_MODE).equals(mode.getValue())) {
            // Physical pin: the pin itself migrates by name in DefaultTuneMigrator.
            return;
        }
        final EnumIniField.EnumKeyValueMap pins = ((EnumIniField) newPin.get()).getEnums();
        // Index 0 of every pin enum is the "unassigned" entry.
        context.addMigration(OSS_PIN, newPinValue.cloneWithValue(quote(pins.get(0))));
    }
}
