# Exhaust Cutout Control Implementation Plan

## Overview
This document outlines the plan to implement a native Exhaust Cutout Control feature in the rusEFI firmware, migrating logic previously handled by Lua scripts into a configurable and robust firmware-level module.

## Core Requirements

### 1. Activation Modes
The system must support two primary ways to select the operational mode of the cutout:
- **Hardware Switch:** A digital input (or analog threshold) that switches between two behaviors.
- **Lua Gauge:** A value from a Lua script (luaGauge) that switches behavior based on a configurable threshold.
  - Configuration: `luaGauge` index, `threshold_value`, `is_greater_than` logic.

### 2. Behavioral Mapping
Users should be able to map "Switch/Gauge State" to a "Cutout Behavior":
- **State A (Switch OFF / Gauge < Threshold):** Choose between `Always Closed`, `Always Open`, or `Auto`.
- **State B (Switch ON / Gauge > Threshold):** Choose between `Always Closed`, `Always Open`, or `Auto`.

*Note: This allows for "Sport Mode" switches that toggle between "Quiet" (Always Closed) and "Automatic" (RPM/TPS/Boost triggers).*

### 3. Automatic Control Triggers (Auto Mode)
When a state is set to `Auto`, the cutout opens if ANY of the following conditions are met:
- **RPM Trigger:** `RPM > Open_RPM`.
- **TPS Trigger:** `TPS > Open_TPS` (with optional `Anti-Blip Delay`).
- **Boost Trigger:** `MAP > Open_Boost_KPA`.

**Hysteresis/Timers:**
- **Closing Wait Time:** The duration the cutout remains open after all triggers are gone.
- **TPS Anti-Blip Delay:** Minimum duration TPS must be above threshold before opening (prevents opening on quick rev-matching or "blips").

### 4. Output Hardware Support

#### A. Digital Output (Single Pin)
- Used for simple solenoids or single-wire controls (e.g., FTO, GT350 style).
- **Configuration:** 
  - Output Pin.
  - Open Logic: `HIGH` means Open or `LOW` means Open.
  - PWM Support: Some valves (like GT350) might require specific PWM duties (e.g., 10% Open, 90% Closed).

#### B. H-Bridge Control (Dual Pin)
- Used for motor-driven cutouts (e.g., 3000GT, DRV8870).
- **Configuration:**
  - `Pin IN1` (Open direction).
  - `Pin IN2` (Close direction).
  - `Move Duration`: Time to apply power to reach the end stop (e.g., 1.2s).
- **Pin Visibility:** Ensure pins are clearly labeled for DRV8870 usage in the UI.

#### C. Status LED (Feedback Output)
- A configurable output pin used to provide visual feedback to the driver.
- **Behavior (Matching 3000GT/FTO pattern):**
  - **Off:** Cutout is fully closed.
  - **Fast Blink (e.g., 3Hz):** Cutout is currently opening.
  - **Slow Blink (e.g., 1Hz):** Cutout is currently closing.
  - **On (Solid) / Off:** Configurable behavior for when the cutout is fully open.
- **Configuration:**
  - Output Pin.
  - `show_open_state`: Boolean to decide if the LED stays ON when open, or turns OFF after opening completes.
  - Inverted Logic (if needed for active-low LEDs).

### 5. Proposed Configuration Structure (Firmware)

```cpp
struct exhaust_cutout_s {
    // Activation
    activation_mode_e mode; // Switch, LuaGauge
    uint8_t input_pin;      // If Hardware Switch
    uint8_t lua_gauge_idx;  // If Lua Gauge
    float gauge_threshold;
    
    // Behavior Mapping
    cutout_behavior_e behavior_low;  // Behavior when switch OFF or Gauge < Threshold
    cutout_behavior_e behavior_high; // Behavior when switch ON or Gauge > Threshold
    
    // Triggers
    uint16_t open_rpm;
    uint8_t open_tps;
    float tps_delay_s;      // Configurable Anti-Blip Delay
    float open_boost_kpa;
    float closing_delay_s;
    
    // Hardware
    output_type_e output_type; // Digital, H-Bridge
    pin_output_s pin1;
    pin_output_s pin2;         // Used for H-Bridge
    float move_duration_s;     // Used for H-Bridge
    bool inverted_logic;       // For Digital
    
    // Feedback
    pin_output_s status_led_pin;
    bool show_open_state;      // If true, LED stays ON when open. If false, LED OFF when open.
};
```

## Feedback & Missing Items Analysis

Based on the provided Lua examples and user requirements:

1.  **Feedback/Status Output:** 
    - **Requirement:** Status LED must be a configurable output following the 3000GT/FTO pattern (Off/Fast Blink/Slow Blink/On).
    - **Implementation:** Added `status_led_pin` to configuration and specified behavior.
2.  **Anti-Blip Delay:**
    - **Requirement:** The delay time must be user-configurable.
    - **Implementation:** Added `tps_delay_s` to the configuration struct.
3.  **Double-Stab Logic:**
    - **Requirement:** This should NOT be part of the cutout control module.
    - **Implementation:** Removed from the plan.
4.  **CAN Integration:**
    - Ensuring the cutout state is available as a `live_data` variable will allow Lua to broadcast it over CAN for vehicles like the GT350.
5.  **H-Bridge Safety:**
    - Ensure that if `move_duration` is active, no opposing command can be sent.
6.  **Multi-Thresholds:**
    - Keep it simple first with one set of "Auto" thresholds shared between behaviors.

## Next Steps
1. Define the `live_data` structure for Exhaust Cutout.
2. Implement the control logic in a new `ExhaustCutoutController`.
3. Add TunerStudio UI for configuration.
4. Add Unit Tests to verify trigger logic and timers.
