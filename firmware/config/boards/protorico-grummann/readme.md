# Protorico-Grummann rusEFI board definition

This is a board definition for the custom Protorico-Grummann board (Grumman step van
conversion). Same physical Hellen 100-pin "mega" module and power management as
[protorico-econoline](../protorico-econoline), but a different harness/wiring:

- Single-coil distributor ignition (IGN1) with a Ford TFI-style ignition bypass output
  (IGN2). IGN3/4/5 are repurposed to drive a 2-wire (direction/step) idle air control
  stepper motor instead of coils 3-5.
- Single injector output (INJ1, `IM_SINGLE_POINT`) fires all injectors together - there is
  no per-cylinder injection on this build. INJ3/4/5 are repurposed: INJ3 as an A/C request
  digital input, INJ4 as a torque converter lockup output, INJ5 as an EGR solenoid output.
- OUT_PWM2 drives the A/C relay (instead of the speedometer output on econoline);
  OUT_PWM4/check-engine light and the fuel pump (SPI2 MISO) are wired the same as econoline.
- Real CLT sensor wired to AIN11 (no CHT-to-CLT estimator needed here, unlike econoline).
- VSS, Park/Neutral switch and CKP all live on the module's D1/D2/D3 digital inputs
  (`H144_IN_D_1/2/3`); there is no cam sensor on this build (not needed by either
  `IM_ONE_COIL` ignition or `IM_SINGLE_POINT` injection), so D2 - cam on econoline - is
  free for VSS here. Power steering pressure switch is wired to AIN22
  (`H144_IN_AUX4_DIGITAL`), suggested as an idle-up switch input.

Pins without a matching dedicated `engineConfiguration` field (ignition bypass, TCC
lockup, EGR solenoid, VSS, Park/Neutral, power steering switch) are named in
`connectors/connectors.yaml` for TunerStudio pin pickers but are not hardcoded to a
specific config field in `board_configuration.cpp` - assign them to the relevant
TunerStudio feature (Wheel Speed Sensors, Idle-Up switches, etc.) or drive them from Lua,
same as econoline's EGR/TCI/IMRC/Torque-Lockup pins.

Cylinder count/firing order are left at the generic default (4 cylinders, `FO_1_3_4_2`) -
set these for your actual engine in TunerStudio. Neither `IM_ONE_COIL` ignition nor
`IM_SINGLE_POINT` injection depend on this value for correct hardware operation (both
always drive slot 0 - see `InjectionEvent::update()`), so it only affects total fuel
delivered per cycle until configured.
