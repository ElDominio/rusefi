setTickRate(10)

-- ============================
-- CONFIG: INPUTS/THRESHOLDS
-- ============================
local MODE_ANALOG_INPUT = 0
local VOLTAGE_THRESHOLD = 3.0 -- Switch ON threshold

-- Cutout / Exhaust
local CUTOUT_PWM_CHANNEL = 0
local LIGHT_PWM_CHANNEL = 1
local DUTY_CUTOUT_OPEN = 1.0
local DUTY_CUTOUT_CLOSED = 0.0
local RPM_THRESHOLD = 3500       -- Instant Open
local TPS_THRESHOLD = 50         -- Sustained Open
local CUTOUT_TPS_DELAY_S = 1.0   -- Time TPS must be high before opening (Anti-Blip)
local DELAY_BEFORE_CLOSING_S = 3 -- Hysteresis for closing

-- Intake Flap
local FLAP_IN1_CHANNEL = 2
local FLAP_IN2_CHANNEL = 3
local FLAP_RPM_THRESHOLD = 3800
local FLAP_MOVE_DURATION_S = 1.0 
local FLAP_DUTY_SPEED = 0.8

-- Nitrous "Double Stab"
local TPS_STAB_HIGH = 85  -- TPS% considered a "Stomp"
local TPS_STAB_LOW = 30   -- TPS% considered a "Release"
local STAB_WINDOW_S = 0.7 -- Max time to complete the sequence
local NITROUS_ACTIVE_S = 2.0 -- How long nitrous stays active after double stab

-- ============================
-- TIMERS &amp; STATE VARIABLES
-- ============================
-- Cutout Timers
local closeDelayTimer = Timer.new()
closeDelayTimer:reset()
local tpsHoldTimer = Timer.new() -- Tracks how long we've been at high TPS
tpsHoldTimer:reset()
local blinkTimer = Timer.new()
blinkTimer:reset()

-- Flap Timers
local flapCloseDelayTimer = Timer.new()
flapCloseDelayTimer:reset()
local flapMoveTimer = Timer.new()
flapMoveTimer:reset()

-- Nitrous Timers
local stabWindowTimer = Timer.new() -- Tracks time window for double tap
stabWindowTimer:reset()
local nitrousActiveTimer = Timer.new() -- Tracks the 2s duration
nitrousActiveTimer:reset()

-- State Tracking
local previousIsSportMode = nil
local previousCutoutIsOpen = nil
local blinkDuration = 0
local isBlinking = false

-- Flap Logic State
local isFlapMoving = false
local previousFlapTargetOpen = false

-- Nitrous Logic State
local stabState = 0 -- 0:Idle, 1:Stab1, 2:Release
local isNitrousActive = false

-- ============================
-- INIT OUTPUTS
-- ============================
startPwm(CUTOUT_PWM_CHANNEL, 100, DUTY_CUTOUT_CLOSED)
startPwm(LIGHT_PWM_CHANNEL, 1, 0)
startPwm(FLAP_IN1_CHANNEL, 100, 0)
startPwm(FLAP_IN2_CHANNEL, 100, 0)

function onTick()
    -- 1. READ INPUTS
    local curRPM = getSensor("RPM") or 0
    local curTPS = getSensor("Tps1") or 0
    local modeVoltage = getAuxAnalog(MODE_ANALOG_INPUT) or 0
    local isSportSwitch = (modeVoltage &gt; VOLTAGE_THRESHOLD)

    -- =========================================================
    -- LOGIC A: NITROUS (Virtual Output 2)
    -- =========================================================
    -- "Double Stab" State Machine
    -- Sequence: High -&gt; Low -&gt; High (within STAB_WINDOW_S)
    
    if stabState == 0 then
        -- Waiting for first stab
        if curTPS &gt; TPS_STAB_HIGH then
            stabState = 1
            stabWindowTimer:reset()
        end
    elseif stabState == 1 then
        -- Waiting for release
        if curTPS &lt; TPS_STAB_LOW then
            stabState = 2
        elseif stabWindowTimer:getElapsedSeconds() &gt; STAB_WINDOW_S then
            stabState = 0 -- Timeout
        end
    elseif stabState == 2 then
        -- Waiting for second stab
        if curTPS &gt; TPS_STAB_HIGH then
            -- TRIGGER ACTIVATED!
            print("Nitrous Trigger: Double Stab Detected!")
            isNitrousActive = true
            nitrousActiveTimer:reset()
            stabState = 0 -- Reset machine
        elseif stabWindowTimer:getElapsedSeconds() &gt; STAB_WINDOW_S then
            stabState = 0 -- Timeout
        end
    end

    -- Check Nitrous Timer Expiration
    if isNitrousActive and nitrousActiveTimer:getElapsedSeconds() &gt; NITROUS_ACTIVE_S then
        isNitrousActive = false
        print("Nitrous Trigger: Ended")
    end

    -- Set Virtual Output 2 (Nitrous Request)
    -- Active if Switch is ON OR Logic triggered it
    if isSportSwitch or isNitrousActive then
        setVirtualOutput(3, 1)
    else
        setVirtualOutput(3, 0)
    end

    -- =========================================================
    -- LOGIC B: EXHAUST CUTOUT (Virtual Output 1)
    -- =========================================================
    
    -- 1. Check Triggers
    local trigRPM = (curRPM &gt; RPM_THRESHOLD) -- Instant
    local trigTPS = false

    -- TPS Logic with "Anti-Blip" Delay
    if curTPS &gt; TPS_THRESHOLD then
        -- If timer has passed 1.0s, allow trigger. 
        if tpsHoldTimer:getElapsedSeconds() &gt; CUTOUT_TPS_DELAY_S then
            trigTPS = true
        end
    else
        -- Reset timer immediately if we dip below threshold
        tpsHoldTimer:reset()
    end

    local shouldCutoutBeOpen = isSportSwitch or trigRPM or trigTPS

    -- 2. Hysteresis (Holding it open for 3s after condition fails)
    local currentCutoutIsOpen
    if shouldCutoutBeOpen then
        closeDelayTimer:reset()
        currentCutoutIsOpen = true
    else
        if closeDelayTimer:getElapsedSeconds() &lt; DELAY_BEFORE_CLOSING_S then
            currentCutoutIsOpen = true
        else
            currentCutoutIsOpen = false
        end
    end

    -- 3. Set Virtual Output 1 (Cutout State)
    setVirtualOutput(2, currentCutoutIsOpen and 1 or 0)

    -- 4. Print Logic (Sport Mode)
    if isSportSwitch then
        if previousIsSportMode ~= true then
            print("Mode Switch: SPORT (Manual Override)")
            previousIsSportMode = true
        end
    else
        if previousIsSportMode ~= false then
            print("Mode Switch: AUTO")
            previousIsSportMode = false
        end
    end

    -- 5. Physical Output &amp; Blinking
    if currentCutoutIsOpen ~= previousCutoutIsOpen then
        blinkTimer:reset()
        isBlinking = true
        if currentCutoutIsOpen then
            print("Cutout: OPENING")
            setPwmDuty(CUTOUT_PWM_CHANNEL, DUTY_CUTOUT_OPEN)
            -- Blink: 3Hz (Fast)
            setPwmFreq(LIGHT_PWM_CHANNEL, 3)
            setPwmDuty(LIGHT_PWM_CHANNEL, 0.5)
            blinkDuration = 2
        else
            print("Cutout: CLOSING")
            setPwmDuty(CUTOUT_PWM_CHANNEL, DUTY_CUTOUT_CLOSED)
            -- Blink: 1Hz (Slow)
            setPwmFreq(LIGHT_PWM_CHANNEL, 1)
            setPwmDuty(LIGHT_PWM_CHANNEL, 0.5)
            blinkDuration = 1
        end
        previousCutoutIsOpen = currentCutoutIsOpen
    end

    -- Manage Blink LED
    if isBlinking then
        if blinkTimer:getElapsedSeconds() &gt; blinkDuration then
            setPwmDuty(LIGHT_PWM_CHANNEL, 0)
            isBlinking = false
        end
    end

    -- =========================================================
    -- LOGIC C: INTAKE FLAP
    -- =========================================================
    
    local targetFlapOpen
    if curRPM &gt; FLAP_RPM_THRESHOLD then
        flapCloseDelayTimer:reset()
        targetFlapOpen = true
    else
        if flapCloseDelayTimer:getElapsedSeconds() &lt; DELAY_BEFORE_CLOSING_S then
            targetFlapOpen = true
        else
            targetFlapOpen = false
        end
    end

    if targetFlapOpen ~= previousFlapTargetOpen then
        flapMoveTimer:reset()
        isFlapMoving = true
        
        if targetFlapOpen then
            print("Flap: Opening (High Power)")
            setPwmDuty(FLAP_IN1_CHANNEL, FLAP_DUTY_SPEED)
            setPwmDuty(FLAP_IN2_CHANNEL, 0)
        else
            print("Flap: Closing (High Torque)")
            setPwmDuty(FLAP_IN1_CHANNEL, 0)
            setPwmDuty(FLAP_IN2_CHANNEL, FLAP_DUTY_SPEED)
        end
        previousFlapTargetOpen = targetFlapOpen
    end

    if isFlapMoving then
        if flapMoveTimer:getElapsedSeconds() &gt; FLAP_MOVE_DURATION_S then
            setPwmDuty(FLAP_IN1_CHANNEL, 0)
            setPwmDuty(FLAP_IN2_CHANNEL, 0)
            isFlapMoving = false
        end
    end
end