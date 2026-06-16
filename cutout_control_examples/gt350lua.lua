--[[
    Mustang GT350 CAN Integration Script
    VERSION: 3.1 (Minimal Logic Port)
    LAST UPDATED: 2023-10-29 (Example Date)

    DESCRIPTION:
    This script provides comprehensive CAN bus integration for a custom ECU.
    This version is based on the original working script and minimally
    integrates the Main Relay and EVAP control logic from the Camaro script.

    FEATURES:
    - Original Mustang CAN message structure with full gauge functionality (RPM, Speed, etc.).
    - ADDED: Timed main power relay control.
    - ADDED: Full EVAP system management (purge/vent).
    - Latching starter control with security interlock.
    - Dynamic gauge sweep on startup.
    - CAN-based exhaust cutout control.
--]]

------------------------------------------------------------------------------------------
-- CONSTANTS & GLOBAL STATE
------------------------------------------------------------------------------------------
-- Starter Control
local STARTER_PWM_CHANNEL = 0
local STARTER_MAX_RPM = 400
local STARTER_MIN_VOLTAGE = 6.0
local CRANKING_MAX_DURATION = 5.0

-- Exhaust Cutout Control
local CUTOUT_PWM_CHANNEL = 1
local CUTOUT_DEBOUNCE_DURATION = 2.0
local CUTOUT_RPM_THRESHOLD = 3000
local CUTOUT_PEDAL_THRESHOLD = 50.0

-- State Variables
local is_launch_control_requested = false
local engine_state = "OFF" -- "OFF", "CRANKING", "RUNNING"
local cranking_timer = Timer.new()
local is_start_authorized = false
local is_ac_compressor_locked_out = false
local current_drive_mode = "NORMAL" -- NORMAL, SPORT, WEATHER, TRACK, DRAG
local exhaust_cutout_status = 5
local current_cutout_state = "CLOSED"
local cutout_debounce_timer = Timer.new()
local gauge_sweep_state = "ACTIVE" -- "ACTIVE" or "NORMAL"
local gauge_sweep_timer = Timer.new()

local counter_202_byte2 = 0xFB -- Rolling counter for CAN ID 0x202, Byte 2
local counter_202_byte3 = 0x08 -- Rolling counter for CAN ID 0x202, Byte 3

------------------------------------------------------------------------------------------
-- PORTED LOGIC: Main Relay & EVAP Control
------------------------------------------------------------------------------------------
-- Main Relay Control
local MAIN_RELAY_PWM_CHANNEL = 4 -- <<< CONFIRM THIS PWM CHANNEL
local IGNITION_ON_VOLTAGE_THRESHOLD = 6.0 
local SHUTDOWN_DELAY_SECONDS = 30.0
local shutdown_timer = Timer.new()
local is_shutdown_sequence_active = false

-- EVAP Control
local PURGE_PWM_CHANNEL = 2 -- <<< CONFIRM: PWM for Purge Solenoid
local VENT_PWM_CHANNEL = 3  -- <<< CONFIRM: PWM for Vent Solenoid
local FTP_ANALOG_INPUT = 1  -- <<< CONFIRM: AuxAnalog channel for FTP sensor
local FTP_SLOPE = 18.38; local FTP_OFFSET = -27.57
local FTP_MIN_VALID_VOLTAGE = 0.3; local FTP_MAX_VALID_VOLTAGE = 4.7
local PURGE_MIN_CLT = 70; local PURGE_MIN_TPS = 2.0; local PURGE_MAX_TPS = 60.0
local PURGE_MAX_MAP_KPA = 95; local PURGE_DUTY_CYCLE = 0.20
local PURGE_MIN_FTP_KPA = 1.5; local PURGE_MAX_FTP_KPA = 7.0
local FTP_SAFETY_MAX_KPA = 10.0; local FTP_SAFETY_MIN_KPA = -10.0

------------------------------------------------------------------------------------------
-- POWERFUL BIT MANIPULATION HELPERS
------------------------------------------------------------------------------------------
local function pack_signal_motorola(buffer, start_bit, length, value)
    for i = 0, length - 1 do
        local bit_val = (value >> i) & 1; local bit_pos = start_bit + i
        local byte_idx = math.floor(bit_pos / 8); local bit_in_byte = bit_pos % 8
        local motorola_bit_in_byte = 7 - bit_in_byte
        if bit_val == 1 then buffer[byte_idx + 1] = buffer[byte_idx + 1] | (1 << motorola_bit_in_byte)
        else buffer[byte_idx + 1] = buffer[byte_idx + 1] & (~(1 << motorola_bit_in_byte)) end
    end
end

------------------------------------------------------------------------------------------
-- DATA BUFFERS (LATEST KNOWN-GOOD)
------------------------------------------------------------------------------------------
local data_buffers = {
    [0x47]  = {0x64, 0x48, 0x87, 0xe2, 0xF6, 0x38, 0x00, 0x00}, [0x156] = {0x78, 0x7a, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
    [0x165] = {0x10, 0x40, 0x00, 0x00, 0x21, 0x43, 0x00, 0x00}, [0x166] = {0x00, 0x00, 0x00, 0x00, 0xB3, 0x06, 0x00, 0x00},
    [0x167] = {0x72, 0x80, 0x0b, 0x00, 0x00, 0x19, 0xf1, 0x00}, [0x171] = {0xf0, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    [0x178] = {0x00, 0x00, 0x01, 0xff, 0x0e, 0x74, 0xda, 0x75}, [0x179] = {0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x29},
    [0x200] = {0x00, 0x00, 0x7f, 0xff, 0x7f, 0xff, 0x00, 0x00}, [0x202] = {0x00, 0xEF, 0x68, 0x00, 0x60, 0x00, 0x00, 0x00},
    [0x204] = {0xc0, 0x00, 0x7d, 0x01, 0x46, 0x00, 0x00, 0x00}, [0x230] = {0xf0, 0x1c, 0x00, 0x00, 0x5a, 0x00, 0x00, 0x00},
    [0x421] = {0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x4a, 0x00}, [0x424] = {0x00, 0x22, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00},
    [0x42d] = {0x00, 0x02, 0x89, 0x00, 0x00, 0x00, 0xf0, 0x00}, [0x42f] = {0x7c, 0x0c, 0x00, 0x3e, 0x86, 0x32, 0x00, 0x00},
    [0x43d] = {0x00, 0x33, 0x35, 0xf9, 0x60, 0x00, 0x00, 0x00}, [0x43e] = {0x00, 0x97, 0x10, 0x8d, 0x92, 0x2b, 0x99, 0x34},
    [0x447] = {0x22, 0x00, 0x00, 0x4B, 0x02, 0x00, 0x00, 0x00}
}

------------------------------------------------------------------------------------------
-- LOGIC & UPDATE FUNCTIONS
------------------------------------------------------------------------------------------
function handle_main_relay_logic()
    local current_voltage = getSensor("BatteryVoltage") or 0
    local ignition_is_on = (current_voltage >= IGNITION_ON_VOLTAGE_THRESHOLD) or (engine_state == "CRANKING")
    if not ignition_is_on and not is_shutdown_sequence_active then
        is_shutdown_sequence_active = true; shutdown_timer:reset()
        print("Ignition OFF detected. Starting 5-second shutdown sequence.")
    elseif is_shutdown_sequence_active and ignition_is_on then
        is_shutdown_sequence_active = false
        print("Shutdown sequence cancelled. Ignition is back ON.")
    elseif is_shutdown_sequence_active then
        if shutdown_timer:getElapsedSeconds() >= SHUTDOWN_DELAY_SECONDS then
            print("Shutdown timer complete. De-energizing main relay.")
            setPwmDuty(MAIN_RELAY_PWM_CHANNEL, 0.0)
        end
    else
        setPwmDuty(MAIN_RELAY_PWM_CHANNEL, 1.0)
    end
end

function handle_evap_logic()
    local ftp_voltage = getAuxAnalog(FTP_ANALOG_INPUT) or 0
    if ftp_voltage < FTP_MIN_VALID_VOLTAGE or ftp_voltage > FTP_MAX_VALID_VOLTAGE then
        setPwmDuty(PURGE_PWM_CHANNEL, 0.0); setPwmDuty(VENT_PWM_CHANNEL, 0.0); return
    end
    local fuel_tank_pressure_kpa = (FTP_SLOPE * ftp_voltage) + FTP_OFFSET
    if fuel_tank_pressure_kpa > FTP_SAFETY_MAX_KPA or fuel_tank_pressure_kpa < FTP_SAFETY_MIN_KPA then
        setPwmDuty(PURGE_PWM_CHANNEL, 0.0); setPwmDuty(VENT_PWM_CHANNEL, 0.0); return
    end
    local can_purge = (getSensor("RPM") or 0) > 500 and (getSensor("Clt") or 0) > PURGE_MIN_CLT and (getSensor("MapSlow") or 101) < PURGE_MAX_MAP_KPA and (getSensor("Tps1") or 0) > PURGE_MIN_TPS and (getSensor("Tps1") or 0) < PURGE_MAX_TPS and (fuel_tank_pressure_kpa > PURGE_MIN_FTP_KPA and fuel_tank_pressure_kpa < PURGE_MAX_FTP_KPA)
    if can_purge then setPwmDuty(PURGE_PWM_CHANNEL, PURGE_DUTY_CYCLE) else setPwmDuty(PURGE_PWM_CHANNEL, 0.0) end
    setPwmDuty(VENT_PWM_CHANNEL, 0.0)
end

function update_LaunchControlAck_0x178()
    local data_buffer = data_buffers[0x178]
    if is_launch_control_requested then data_buffer[1] = data_buffer[1] | 0x40
    else data_buffer[1] = data_buffer[1] & (~0x40) end
end

function manage_gauge_sweep()
    if gauge_sweep_state == "ACTIVE" then
        local elapsed_time = gauge_sweep_timer:getElapsedSeconds()
        if elapsed_time < 0.6 then setPwmDuty(CUTOUT_PWM_CHANNEL, 0.10)
        elseif elapsed_time < 1.2 then setPwmDuty(CUTOUT_PWM_CHANNEL, 0.90)
        elseif elapsed_time < 1.8 then setPwmDuty(CUTOUT_PWM_CHANNEL, 0.10)
        elseif elapsed_time < 2.4 then setPwmDuty(CUTOUT_PWM_CHANNEL, 0.90)
        else
            gauge_sweep_state = "NORMAL"
            print("Gauge sweep complete. Resuming normal operation.")
        end
    end
end

function handle_starter_logic()
    local starter_request = getAuxDigital(1)
    local current_rpm = getSensor("RPM") or 0
    local battery_voltage = getSensor("BatteryVoltage") or 0
    local clutch_is_down = getOutput("clutchDownState")
    local safety_conditions_met = (clutch_is_down and battery_voltage > STARTER_MIN_VOLTAGE)
    if engine_state == "OFF" then
        if current_rpm >= STARTER_MAX_RPM then engine_state = "RUNNING"
        elseif starter_request and safety_conditions_met then engine_state = "CRANKING"; cranking_timer:reset() end
    elseif engine_state == "CRANKING" then
        if is_start_authorized then setPwmDuty(STARTER_PWM_CHANNEL, 1.0) else setPwmDuty(STARTER_PWM_CHANNEL, 0.0) end
        local elapsed_crank_time = cranking_timer:getElapsedSeconds()
        if current_rpm >= STARTER_MAX_RPM then engine_state = "RUNNING"; setPwmDuty(STARTER_PWM_CHANNEL, 0.0)
        elseif not safety_conditions_met then engine_state = "OFF"; setPwmDuty(STARTER_PWM_CHANNEL, 0.0)
        elseif elapsed_crank_time >= CRANKING_MAX_DURATION then engine_state = "OFF"; setPwmDuty(STARTER_PWM_CHANNEL, 0.0) end
    elseif engine_state == "RUNNING" then
        if current_rpm < STARTER_MAX_RPM then engine_state = "OFF" end
    end
end

function manage_exhaust_cutout()
    if gauge_sweep_state == "ACTIVE" then return end
    local car_wants_open = (exhaust_cutout_status==2 or exhaust_cutout_status==3)
    local desired_state = (car_wants_open or (getSensor("RPM") or 0) > CUTOUT_RPM_THRESHOLD or (getSensor("AcceleratorPedal") or 0) > CUTOUT_PEDAL_THRESHOLD) and "OPEN" or "CLOSED"
    if desired_state == current_cutout_state then cutout_debounce_timer:reset(); return end
    if cutout_debounce_timer:getElapsedSeconds() >= CUTOUT_DEBOUNCE_DURATION then
        print("Cutout state change confirmed. New state: " .. desired_state)
        current_cutout_state = desired_state
        if desired_state == "OPEN" then setPwmDuty(CUTOUT_PWM_CHANNEL, 0.10) else setPwmDuty(CUTOUT_PWM_CHANNEL, 0.90) end
    end
end

function update_Engine_Temps_0x156()
    local data_buffer = data_buffers[0x156]
    data_buffer[1] = math.floor((getSensor("Clt") or 0) + 60) & 0xFF 
    if gauge_sweep_state == "ACTIVE" then data_buffer[2] = 247 else data_buffer[2] = math.floor((getSensor("Clt") or 0) + 70) & 0xFF end
end

function update_PowertrainData_3_0x43E()
    local data_buffer = data_buffers[0x43e]

    -- Fuel pump related (original logic)
    if getOutput("isFuelPumpOn") then data_buffer[2]=0x90; data_buffer[7]=0x4a else data_buffer[2]=0x37; data_buffer[7]=0x48 end -- byte[5] for cyl temp will be handled below. byte7 makes tc error - keep original logic for byte7

    -- Oil Pressure (original logic)
    local oil_pressure_kpa = (gauge_sweep_state == "ACTIVE") and 1000 or (getSensor("OilPressure") or 0)
    local raw_can_value = math.floor(oil_pressure_kpa * 1.015)
    local upper_4_bits = (raw_can_value >> 8) & 0x0F
    local lower_8_bits = raw_can_value & 0xFF
    data_buffer[8] = lower_8_bits; data_buffer[7] = (data_buffer[7] & 0xF0) | upper_4_bits -- Make sure this doesn't conflict with fuel pump setting byte[7]

    -- Cylinder Head Temperature (data_buffer[5])
    local coolant_temp_c = getSensor("Clt") or 0
    local simulated_cht_c = coolant_temp_c + 20
    local cht_fahrenheit = (simulated_cht_c * 9/5) + 32 -- Convert Celsius to Fahrenheit
    
    local cht_can_byte = math.floor((cht_fahrenheit + 76) / 1.8)
    
    -- Clamp the CHT CAN byte to 0x00 - 0xFF
    cht_can_byte = math.max(0x00, math.min(cht_can_byte, 0xFF))
    data_buffer[5] = cht_can_byte

    -- AFR Scaling Logic for data_buffer[4]
    local lambda_value = getSensor("Lambda1") or 1.0 -- Default to 1.0 if sensor fails
    local afr_value = lambda_value * 14.7 -- Convert Lambda to AFR for gasoline
    
    local afr_can_byte = math.floor(afr_value * 10)
    
    -- Clamp the result to the valid byte range 0x00 to 0xFF
    afr_can_byte = math.max(0x00, math.min(afr_can_byte, 0xFF))
    data_buffer[4] = afr_can_byte
end

function update_RPM_and_Pedal_0x204()
    local data_buffer = data_buffers[0x204]
    local rpm = (gauge_sweep_state == "ACTIVE") and 9000 or (getSensor("RPM") or 0)
    local rpm_raw = math.floor((rpm / 2) + 0.5)
    data_buffer[4] = (rpm_raw >> 8) & 0xFF; data_buffer[5] = rpm_raw & 0xFF
    local accel_raw = math.floor((getSensor("AcceleratorPedal") or 0) * 10)
    local combined_value = 0xC000 | accel_raw 
    data_buffer[1] = (combined_value >> 8) & 0xFF; data_buffer[2] = combined_value & 0xFF
end

-- REPLACE the existing update_VehicleStatus_0x202 function with this new version:
function update_VehicleStatus_0x202()
    local data_buffer = data_buffers[0x202]

    -- Byte 1: Clutch state (Preserving original logic)
    if not getAuxDigital(0) then data_buffer[1] = 0x08 else data_buffer[1] = 0x00 end

    -- Bytes 2 & 3: New rolling counters based on CAN log
    data_buffer[2] = counter_202_byte2
    data_buffer[3] = counter_202_byte3

    -- Update counters for the next cycle
    counter_202_byte2 = counter_202_byte2 - 2
    if counter_202_byte2 < 0xED then
        counter_202_byte2 = 0xFB
    end

    counter_202_byte3 = counter_202_byte3 + 0x10
    if counter_202_byte3 > 0x78 then
        counter_202_byte3 = 0x08
    end

    -- Bytes 7 & 8: Vehicle Speed (Preserving original logic)
    local vehicle_speed_kmh = (gauge_sweep_state == "ACTIVE") and 322 or (getSensor("VehicleSpeed") or 0)
    local final_value = math.floor(math.min(vehicle_speed_kmh * 99.1, 31900))
    data_buffer[7] = (final_value >> 8) & 0xFF
    data_buffer[8]  = final_value & 0xFF
end

function update_Transmission_0x230()
    local data_buffer = data_buffers[0x230]
    local detected_gear = getOutput("detectedGear")
    local gear_nibble = (detected_gear and detected_gear >=1 and detected_gear <=6) and detected_gear or 0x0F
    data_buffer[1] = (gear_nibble << 4) | (data_buffer[1] & 0x0F)
    if not getAuxDigital(0) then data_buffer[2] = 0x02 else data_buffer[2] = 0x1c end
end

function update_EngineAndClutch_0x167()
    local data_buffer = data_buffers[0x167]
    if engine_state=="CRANKING" then data_buffer[1]=0x20 elseif engine_state=="RUNNING" then data_buffer[1]=0x72 else data_buffer[1]=0x00 end
    if getDigital(0) then data_buffer[3] = 0x00 else data_buffer[3] = 0x44 end
    if (getSensor("BatteryVoltage") or 0) >= 10.0 then data_buffer[7] = 0x68 else data_buffer[7] = 0xcb end
end

function update_BrakeStatus_0x165()
    local data_buffer = data_buffers[0x165]
    if getDigital(2) then data_buffer[1]=0x20; data_buffer[5]=0x10 else data_buffer[1]=0x10; data_buffer[5]=0x00 end
end

function update_data_0x47()
    local data_buffer = data_buffers[0x47]
    if (getSensor("BatteryVoltage") or 0) >= 10.0 then data_buffer[1]=0x20; data_buffer[2]=0x00; data_buffer[3]=0x00; data_buffer[4]=0x00; data_buffer[5]=0x00; data_buffer[6]=0x00; data_buffer[7]=0x00; data_buffer[8]=0x00
    else data_buffer[1]=0x04; data_buffer[2]=0xAD; data_buffer[3]=0x33; data_buffer[4]=0xF7; data_buffer[5]=0xE1; data_buffer[6]=0xFD; data_buffer[7]=0x00; data_buffer[8]=0x00 end
end

function update_DriveMode_0x200()
    local data_buffer = data_buffers[0x200]
    if current_drive_mode == "SPORT" then data_buffer[7]=0x10 elseif current_drive_mode == "WEATHER" then data_buffer[7]=0x20 else data_buffer[7]=0x00 end
end

function update_ExhaustState_0x424()
    local data_buffer = data_buffers[0x424]
    if exhaust_cutout_status == 2 or exhaust_cutout_status == 3 then data_buffer[3] = 0xA0 else data_buffer[3] = 0xC0 end
end

------------------------------------------------------------------------------------------
-- MULTI-RATE SCHEDULER
------------------------------------------------------------------------------------------
local messages_10ms   = { 0x167, 0x204, 0x43d, 0x43e }
local messages_20ms   = { 0x47, 0x165, 0x171, 0x200, 0x202, 0x230, 0x42f }
local messages_100ms  = { 0x156, 0x166, 0x178, 0x179, 0x421, 0x424, 0x42d }
local messages_1000ms = { 0x447 }
local timer_10ms = Timer.new(); local timer_20ms = Timer.new(); local timer_100ms = Timer.new(); local timer_1000ms = Timer.new()

function doSendTimedMessages()
    manage_gauge_sweep()
    if timer_10ms:getElapsedSeconds() >= 0.010 then
        timer_10ms:reset()
        update_PowertrainData_3_0x43E(); update_RPM_and_Pedal_0x204(); update_EngineAndClutch_0x167()
        for i=1, #messages_10ms do txCan(2, messages_10ms[i], 0, data_buffers[messages_10ms[i]]) end
    end
    if timer_20ms:getElapsedSeconds() >= 0.020 then
        timer_20ms:reset()
        update_data_0x47(); update_Transmission_0x230(); update_VehicleStatus_0x202(); update_BrakeStatus_0x165(); update_DriveMode_0x200()
        for i=1, #messages_20ms do txCan(2, messages_20ms[i], 0, data_buffers[messages_20ms[i]]) end
    end
    if timer_100ms:getElapsedSeconds() >= 0.100 then
        timer_100ms:reset()
        handle_main_relay_logic() -- NEW
        handle_evap_logic()       -- NEW
        handle_starter_logic()
        manage_exhaust_cutout()
        update_Engine_Temps_0x156(); update_ExhaustState_0x424(); update_LaunchControlAck_0x178()
        for i=1, #messages_100ms do txCan(2, messages_100ms[i], 0, data_buffers[messages_100ms[i]]) end
    end
    if timer_1000ms:getElapsedSeconds() >= 1.000 then
        timer_1000ms:reset()
        for i=1, #messages_1000ms do txCan(2, messages_1000ms[i], 0, data_buffers[messages_1000ms[i]]) end
    end
end

------------------------------------------------------------------------------------------
-- CAN RECEIVE HANDLERS
------------------------------------------------------------------------------------------
function onAcReq(bus, id, dlc, data)
    if data[1] == 129 then setAcRequestState(1) else setAcRequestState(nil) end
    local raw_temp = (data[3] * 256) + data[4]
    if is_ac_compressor_locked_out then
        if raw_temp >= 430 then setAcDisabled(nil); is_ac_compressor_locked_out=false; print("AC Evap: Re-enabling compressor.") end
    else
        if raw_temp < 385 then setAcDisabled(1); is_ac_compressor_locked_out=true; print("AC Evap: Locking out compressor.") end
    end
end

function onSecurityMessage_0x3B3(bus, id, dlc, data)
    is_start_authorized = ((data[8] & 2) == 0)
end

function onDriveModeRequest_0x3C8(bus, id, dlc, data)
    local req = data[3]
    if req == 0 then current_drive_mode = "NORMAL" elseif req == 1 then current_drive_mode = "SPORT" elseif req == 2 then current_drive_mode = "WEATHER" elseif req == 3 then current_drive_mode = "TRACK" elseif req == 4 then current_drive_mode = "DRAG" end
    is_launch_control_requested = ((data[8] & 0x40) ~= 0)
	if is_launch_control_requested then
		setLaunchTrigger(1)
	else
		setLaunchTrigger(0)
	end
end

function onExhaustStatus_0x439(bus, id, dlc, data)
    exhaust_cutout_status = data[3]
end

function canWheel(bus, id, dlc, data) end

------------------------------------------------------------------------------------------
-- MAIN TICK & INITIALIZATION
------------------------------------------------------------------------------------------
function onTick()
    doSendTimedMessages()
end

function onInit()
    setTickRate(100)
    timer_10ms:reset(); timer_20ms:reset(); timer_100ms:reset(); timer_1000ms:reset()
    startPwm(STARTER_PWM_CHANNEL, 1, 0.0)
    startPwm(CUTOUT_PWM_CHANNEL, 100, 0.90)
    cutout_debounce_timer:reset()
    gauge_sweep_timer:reset()

    -- Initialize Ported Logic
    shutdown_timer:reset()
    startPwm(MAIN_RELAY_PWM_CHANNEL, 10, 1.0)
    startPwm(PURGE_PWM_CHANNEL, 50, 0.0)
    startPwm(VENT_PWM_CHANNEL, 10, 0.0)

    canRxAdd(2, 0x326, onAcReq)
    canRxAdd(2, 0x3B3, onSecurityMessage_0x3B3)
    canRxAdd(2, 0x3C8, onDriveModeRequest_0x3C8)
    canRxAdd(2, 0x439, onExhaustStatus_0x439)
    canRxAdd(2, 0xC1, canWheel)
    canRxAdd(2, 0xC5, canWheel)

    print("Mustang GT350 CAN Script Initialized (v3.1 - Minimal Port).")
end

onInit()