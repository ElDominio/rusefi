package com.rusefi.maintenance.migration;

import com.opensr5.ini.field.EnumIniField;
import com.opensr5.ini.field.IniField;
import com.opensr5.ini.field.ScalarIniField;
import com.rusefi.maintenance.TestTuneMigrationContext;
import com.rusefi.maintenance.migration.migrators.ComposedTuneMigrator;
import com.rusefi.tune.xml.Constant;
import org.junit.jupiter.api.Test;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;

import static com.rusefi.config.FieldType.UINT16;
import static com.rusefi.config.FieldType.UINT8;
import static com.rusefi.maintenance.migration.TestTuneMigrationContextFactory.createTestMigrationContext;
import static javax.management.ObjectName.quote;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.mockito.Mockito.when;

// [tag:alphax_upstream_sync_migration]
public class AlphaxUpstreamSyncMigratorTest {
    private static final String OSS_PIN = "outputShaftSpeedSensorPin";
    private static final String OSS_MODE = "ossMode";

    @Test
    void canLuaModeUnassignsPin() {
        TestTuneMigrationContext context = ossContext("CAN/Lua", "PA5");

        ComposedTuneMigrator.INSTANCE.migrateTune(context);

        assertEquals(enumConstant(OSS_PIN, "NONE"), context.getMigratedValue(OSS_PIN));
    }

    @Test
    void physicalPinModeKeepsPin() {
        TestTuneMigrationContext context = ossContext("Physical Pin", "PA5");

        ComposedTuneMigrator.INSTANCE.migrateTune(context);

        assertEquals(enumConstant(OSS_PIN, "PA5"), context.getMigratedValue(OSS_PIN));
    }

    @Test
    void secondaryPumpLoadKpaBecomesLoad() {
        String name = "secondaryFpActivationLoad";
        TestTuneMigrationContext context = createTestMigrationContext(
            new Constant(name, "kPa", "180", "0"), scalar(name, "kPa"),
            new Constant(name, "load", "0", "0"), scalar(name, "load"));

        ComposedTuneMigrator.INSTANCE.migrateTune(context);

        assertEquals(new Constant(name, "load", "180", "0"), context.getMigratedValue(name));
    }

    private static TestTuneMigrationContext ossContext(String mode, String pin) {
        EnumIniField pinField = new EnumIniField(OSS_PIN, 100, UINT8, labels("NONE", "PA5", "PB3"), 0, 7);
        TestTuneMigrationContext context = createTestMigrationContext(
            enumConstant(OSS_PIN, pin), pinField,
            enumConstant(OSS_PIN, "NONE"), pinField);
        EnumIniField modeField = new EnumIniField(OSS_MODE, 101, UINT8, labels("None", "Physical Pin", "CAN/Lua"), 0, 1);
        when(context.getPrevIniFile().findIniField(OSS_MODE)).thenReturn(Optional.of(modeField));
        Map<String, IniField> fields = new LinkedHashMap<>(context.getPrevIniFile().getAllIniFields());
        fields.put(OSS_MODE, modeField);
        when(context.getPrevIniFile().getAllIniFields()).thenReturn(fields);
        Map<String, Constant> previous = new LinkedHashMap<>(context.getPrevTune().getConstantsAsMap());
        previous.put(OSS_MODE, enumConstant(OSS_MODE, mode));
        when(context.getPrevTune().getConstantsAsMap()).thenReturn(previous);
        return context;
    }

    private static ScalarIniField scalar(String name, String units) {
        return new ScalarIniField(name, 200, units, UINT16, 1, "0", 0);
    }

    private static EnumIniField.EnumKeyValueMap labels(String... values) {
        Map<Integer, String> result = new LinkedHashMap<>();
        for (int i = 0; i < values.length; i++) {
            result.put(i, values[i]);
        }
        return new EnumIniField.EnumKeyValueMap(result);
    }

    private static Constant enumConstant(String name, String value) {
        return new Constant(name, null, quote(value), null);
    }
}
