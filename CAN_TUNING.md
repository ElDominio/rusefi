# CAN Tuning and Operating Value Modification in rusEFI

This document describes how rusEFI supports tuning, calibration changes, and dynamic operating value modifications over CAN-bus.

---

## 1. Overview of CAN Tuning Capabilities

rusEFI supports two primary mechanisms for modifying operating parameters and tuning maps over CAN:

1. **Full ECU Tuning over CAN (TunerStudio / Console over ISO-TP)**:
   - Tunnels the complete TunerStudio binary protocol over CAN using the **ISO 15765-2 (ISO-TP)** transport layer.
   - Provides full read/write/burn access to all calibration tables (VE maps, ignition maps, boost target/duty, lambda tables, idle curves, engine settings) exactly like a USB/Serial connection.
2. **Runtime Operating Value Adjustments via Lua Scripting**:
   - Allows external CAN devices (e.g. CAN keypads, steering-wheel switches, digital dashes, transmission controllers) to modify runtime trims (fuel, ignition, boost, idle, throttle), switch modes, or modify scalar calibrations and burn them to flash.
3. **Firmware Flashing over CAN (OpenBLT XCP-on-CAN)**:
   - Bootloader support for updating firmware binaries directly across the CAN bus.

---

## 2. Full ECU Tuning via TunerStudio over CAN (ISO-TP)

### Architecture
rusEFI implements an ISO-TP (`ISO 15765-2`) network layer that fragments and reassembles multi-frame TunerStudio serial packets over standard 8-byte CAN frames.

* **Transport & Framing**: [`firmware/controllers/can/isotp/isotp.cpp`](file:///home/normanpaulino/Documents/GitHub/rusefi/firmware/controllers/can/isotp/isotp.cpp)
* **CAN Serial Bridge**: [`firmware/console/binary/serial_can.cpp`](file:///home/normanpaulino/Documents/GitHub/rusefi/firmware/console/binary/serial_can.cpp)
* **TunerStudio Channel**: [`firmware/console/binary/ts_can_channel.cpp`](file:///home/normanpaulino/Documents/GitHub/rusefi/firmware/console/binary/ts_can_channel.cpp) (enabled via feature flag `EFI_CAN_SERIAL`)

### Standard CAN Identifiers
By default, the standard broadcast/reception IDs configured across boards are:
* **Host to ECU (ECU RX)**: `0x710` (`CAN_ECU_SERIAL_RX_ID`)
* **ECU to Host (ECU TX)**: `0x720` (`CAN_ECU_SERIAL_TX_ID`)
*(Note: Small CAN boards may use 29-bit extended IDs such as `0x300100` / `0x300102`).*

### Supported Operations
Connecting via TunerStudio over CAN provides 100% parity with USB serial tuning:
* Live 3D VE, Ignition, Boost, and AFR table editing.
* TunerStudio live AutoTune (VE Analyze Live).
* Sensor calibration and trigger configuration.
* Flash persistence (Burn to Flash).

### Host Connection Options
1. **rusEFI Console Bridge / PCAN-USB**:
   - rusEFI Java Console includes a native PCAN driver (`PCanIoStream.java` / `PCanConnectorUI.java`) that connects directly to PEAK PCAN-USB interfaces.
2. **Linux SocketCAN**:
   - Linux users can run `CANConnectorStartup` using `socketcan_connector` to bridge any SocketCAN interface (e.g. `can0`) into a TCP port for TunerStudio.

---

## 3. Runtime Trims & Calibration Changes via Lua Scripting

For in-car controls, secondary ECUs, or CAN keypads to adjust parameters without a laptop, rusEFI provides an onboard Lua engine ([`firmware/controllers/lua/`](file:///home/normanpaulino/Documents/GitHub/rusefi/firmware/controllers/lua/)) with direct CAN frame reception and engine trim hooks.

### Receiving CAN Messages in Lua
Lua scripts register CAN filters and receive frames asynchronously:

```lua
-- Listen for CAN ID 0x500 (e.g., CAN keypad or dash selector)
canRxAdd(0x500)

function onCanRx(bus, id, dlc, data)
    if id == 0x500 then
        local boostMode = data[1]
        local tractionMode = data[2]
        
        -- Adjust boost target based on mode switch
        if boostMode == 1 then
            setBoostTargetAdd(0)       -- Base boost
        elseif boostMode == 2 then
            setBoostTargetAdd(50)      -- +50 kPa (~7.2 psi)
        elseif boostMode == 3 then
            setBoostTargetAdd(100)     -- +100 kPa (~14.5 psi)
        end
    end
end
```

### Available Dynamic Trim Functions

| Parameter | Function | Description |
|---|---|---|
| **Fueling** | `setFuelMult(coef)` | Multiplies base fuel injection calculation (e.g. `1.05` for +5%) |
| | `setFuelAdd(mg)` | Adds raw fuel mass (milligrams per cylinder) |
| | `setFuelDisabled(bool)` | Overrides fuel cut |
| **Ignition** | `setTimingAdd(deg)` | Adds/retards ignition timing in degrees |
| | `setTimingMult(coef)` | Multiplies ignition advance |
| | `setIgnDisabled(bool)` | Overrides ignition cut |
| **Boost Control** | `setBoostTargetAdd(kpa)` | Offsets closed-loop boost target (kPa) |
| | `setBoostTargetMult(coef)` | Multiplies closed-loop boost target |
| | `setBoostDutyAdd(pct)` | Offsets open-loop wastegate duty cycle |
| **Idle Control** | `setIdleRpm(rpm)` | Sets fixed target idle RPM override |
| | `setIdleAdd(pct)` | Adds idle valve/ETB target duty offset |
| **Throttle / Wastegate** | `setEtbAdd(pct)` | Electronic throttle position trim |
| | `setEwgAdd(pct)` | Electronic wastegate position trim |
| | `setEtbDisabled(bool)` | Disables electronic throttle body control |
| **Traction / Cuts** | `setSparkSkipRatio(ratio)` | Soft spark-cut ratio (0.0 to 1.0) |
| | `setSparkHardSkipRatio(r)` | Hard spark-cut ratio (0.0 to 1.0) |
| | `setLaunchTrigger(bool)` | Activates launch control logic |
| | `setRollingIdleTrigger(bool)`| Activates rolling launch / antilag |

---

## 4. Modifying Scalar Calibrations & Persistent Flash from CAN

Lua scripts can also change named scalar configuration parameters and commit them to non-volatile flash memory:

```lua
-- Update cranking RPM setting from a CAN packet
setCalibration("cranking.rpm", 850, true)

-- Burn all modified settings to permanent flash memory
burnConfig()
```

* `setCalibration(name, value, incrementVersion)`: Updates any scalar `engine_configuration_s` field by name (see [`firmware/controllers/lua/value_lookup.cpp`](file:///home/normanpaulino/Documents/GitHub/rusefi/firmware/controllers/lua/value_lookup.cpp)).
* `burnConfig()`: Issues an asynchronous flash write request to save the new tune.

---

## 5. Table Switching and Secondary Maps

rusEFI supports secondary tables for dual-fuel or switchable maps (TunerStudio Page 4, [`firmware/controllers/second_tables.cpp`](file:///home/normanpaulino/Documents/GitHub/rusefi/firmware/controllers/second_tables.cpp)):
* **Second VE Table**
* **Second Ignition Table**
* **VE / Ignition Blend Tables**

These tables can be blended or fully switched using input pins, sensor values (e.g. flex fuel ethanol content), or virtual Lua sensors fed directly from incoming CAN frames:

```lua
-- Create a virtual ethanol content sensor driven by CAN
local flexSensor = Sensor.new("EthanolPercent")

function onCanRx(bus, id, dlc, data)
    if id == 0x600 then
        local ethanol = data[1]
        flexSensor:set(ethanol) -- Blends primary and secondary fuel/ignition maps
    end
end
```

---

## 6. Comparison Summary

| Method | Best Used For | Changes Saved to Flash? | Requires Laptop/PC? |
|---|---|---|---|
| **TunerStudio over CAN** | Complete mapping, VE autotuning, dyno tuning | Yes (via TS "Burn") | Yes (PC with CAN adapter) |
| **Lua Dynamic Trims** | Mode switches, scramble boost, traction control, valet mode | No (dynamic runtime offsets) | No (uses in-car CAN devices) |
| **Lua `setCalibration` + `burnConfig`** | Programmatic updates to scalar configuration | Yes (upon `burnConfig()`) | No |
| **OpenBLT XCP-on-CAN** | Complete firmware updates / re-flashing | Yes (flashes binary image) | Yes (`BootCommander`) |
