setTickRate(10)

-- ============================
-- CONFIG: INPUTS &amp; SETTINGS
-- ============================
local DEBUG_PRINTS = false        -- Set to false to disable console spam
local MODE_ANALOG_INPUT = 0
local VOLTAGE_THRESHOLD = 3.0 

-- Cutout (DRV8870 H-Bridge)
local CUT_IN1_CHANNEL = 2        -- Physical Pin for Open
local CUT_IN2_CHANNEL = 3        -- Physical Pin for Close
local CUT_MOVE_DURATION_S = 1.2  
local CUT_DUTY_SPEED = 1.0       

-- Light / Feedback (PD3)
local LIGHT_PWM_CHANNEL = 1      -- Mapped to PD3
local LIGHT_BLINK_OPEN_FREQ = 3  -- Fast blink (Opening)
local LIGHT_BLINK_CLOSE_FREQ = 1 -- Slow blink (Closing)

-- Cutout Logic Params
local RPM_THRESHOLD = 3500
local TPS_THRESHOLD = 50
local CUTOUT_TPS_DELAY_S = 1.0
local DELAY_BEFORE_CLOSING_S = 3 

-- Map Switch "Double Stab"
local TPS_STAB_HIGH = 85
local TPS_STAB_LOW = 30
local STAB_WINDOW_S = 0.7
local MAP_STAB_DURATION_S = 2.0  -- How long the stab stays active

-- ============================
-- TIMERS &amp; STATE VARIABLES
-- ============================
local closeDelayTimer = Timer.new()
local tpsHoldTimer = Timer.new()
local cutoutMoveTimer = Timer.new()
local blinkTimer = Timer.new()
local stabWindowTimer = Timer.new()
local mapStabTimer = Timer.new()

local stabState = 0
local isStabActive = false
local isCutoutMoving = false
local previousCutoutTargetOpen = nil
local isBlinking = false
local blinkDuration = 0

-- ============================
-- INIT OUTPUTS
-- ============================
startPwm(CUT_IN1_CHANNEL, 100, 0)
startPwm(CUT_IN2_CHANNEL, 100, 0)
startPwm(LIGHT_PWM_CHANNEL, 1, 0) 

function onTick()
    -- 1. READ INPUTS
    local curRPM = getSensor("RPM") or 0
    local curTPS = getSensor("Tps1") or 0
    local modeVoltage = getAuxAnalog(MODE_ANALOG_INPUT) or 0
    local isSportSwitch = (modeVoltage &gt; VOLTAGE_THRESHOLD)

    -- =========================================================
    -- LOGIC A: MAP SWITCH / BOOST (Virtual Output 2)
    -- =========================================================
    -- Double Stab State Machine
    if stabState == 0 then
        if curTPS &gt; TPS_STAB_HIGH then stabState = 1; stabWindowTimer:reset() end
    elseif stabState == 1 then
        if curTPS &lt; TPS_STAB_LOW then stabState = 2
        elseif stabWindowTimer:getElapsedSeconds() &gt; STAB_WINDOW_S then stabState = 0 end
    elseif stabState == 2 then
        if curTPS &gt; TPS_STAB_HIGH then
            isStabActive = true
            mapStabTimer:reset()
            stabState = 0 
            if DEBUG_PRINTS then print("Stab Triggered!") end
        elseif stabWindowTimer:getElapsedSeconds() &gt; STAB_WINDOW_S then stabState = 0 end
    end

    -- Expire the stab timer
    if isStabActive and mapStabTimer:getElapsedSeconds() &gt; MAP_STAB_DURATION_S then
        isStabActive = false
    end
    
    -- VO2 = Switch OR Stab (Typically for Boost/Map control)
    setVirtualOutput(2, (isSportSwitch or isStabActive) and 1 or 0)

    -- =========================================================
    -- LOGIC B: CUTOUT CONTROL (Virtual Output 3 &amp; Hardware)
    -- =========================================================
    local trigRPM = (curRPM &gt; RPM_THRESHOLD)
    local trigTPS = false
    
    if curTPS &gt; TPS_THRESHOLD then
        if tpsHoldTimer:getElapsedSeconds() &gt; CUTOUT_TPS_DELAY_S then trigTPS = true end
    else
        tpsHoldTimer:reset()
    end

    local shouldBeOpen = (isSportSwitch or trigRPM or trigTPS)
    local targetCutoutOpen = false

    if shouldBeOpen then
        closeDelayTimer:reset()
        targetCutoutOpen = true
    else
        targetCutoutOpen = (closeDelayTimer:getElapsedSeconds() &lt; DELAY_BEFORE_CLOSING_S)
    end

    -- VO3 = Cutout Target State
    setVirtualOutput(3, targetCutoutOpen and 1 or 0)

    -- Handle DRV8870 Motor Movement &amp; PD3 Blinking
    if targetCutoutOpen ~= previousCutoutTargetOpen then
        cutoutMoveTimer:reset()
        blinkTimer:reset()
        isCutoutMoving = true
        isBlinking = true
        
        if targetCutoutOpen then
            if DEBUG_PRINTS then print("Cutout: OPENING") end
            setPwmDuty(CUT_IN1_CHANNEL, CUT_DUTY_SPEED)
            setPwmDuty(CUT_IN2_CHANNEL, 0)
            setPwmFreq(LIGHT_PWM_CHANNEL, LIGHT_BLINK_OPEN_FREQ)
            setPwmDuty(LIGHT_PWM_CHANNEL, 0.5)
            blinkDuration = 2
        else
            if DEBUG_PRINTS then print("Cutout: CLOSING") end
            setPwmDuty(CUT_IN1_CHANNEL, 0)
            setPwmDuty(CUT_IN2_CHANNEL, CUT_DUTY_SPEED)
            setPwmFreq(LIGHT_PWM_CHANNEL, LIGHT_BLINK_CLOSE_FREQ)
            setPwmDuty(LIGHT_PWM_CHANNEL, 0.5)
            blinkDuration = 1
        end
        previousCutoutTargetOpen = targetCutoutOpen
    end

    -- Movement Stop Logic
    if isCutoutMoving and cutoutMoveTimer:getElapsedSeconds() &gt; CUT_MOVE_DURATION_S then
        setPwmDuty(CUT_IN1_CHANNEL, 0)
        setPwmDuty(CUT_IN2_CHANNEL, 0)
        isCutoutMoving = false
    end

    -- Blink Stop Logic
    if isBlinking and blinkTimer:getElapsedSeconds() &gt; blinkDuration then
        setPwmDuty(LIGHT_PWM_CHANNEL, 0)
        isBlinking = false
    end
end