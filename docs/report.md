# Work Report

## 2026-07-14 - Investigation: "Malformed Packet: packet length" in USB.pcapng

What was done:
- Analyzed USB.pcapng (USBPcap capture of the rusEFI ECU USB link, device address 7)
  using tshark to explain the "malformed packet" warnings.

Findings:
- Protocol hierarchy: 84926 frames, mostly USB mass storage (usbms/scsi) plus 2 CDC
  (usbcom) control frames. 23 SCSI frames + 1 URB frame flagged _ws.malformed.
- The 23 usbms malformed frames are ALL the same case: the device reply to
  SCSI Mode Sense(6) (opcode 0x1a). Wireshark message: "SCSI: length of contained
  item exceeds length of containing item".
- Root cause is a Wireshark dissector strictness issue, NOT bad wire data. The 16-byte
  reply is self-consistent:
      0f 00 00 00  08 0a 00 00 00 00 00 00 00 00 00 00
  Mode Data Length=15 (=total-1), Caching mode page (0x08) with PageLength=0x0a=10.
  SBC-2 mandates the Caching page be 0x12=18 long (20-byte page); rusEFI emits a valid
  but non-standard SHORT caching page. Wireshark decodes the full 20-byte layout, reads
  past the 16-byte buffer, and raises the exception. Windows accepts it -> device works.
- Response is hardcoded in ChibiOS-Contrib USB-MSD SCSI target (os/hal/src/hal_usb_msd.c,
  submodule not checked out locally), driven by
  firmware/hw_layer/mass_storage/mass_storage_device.cpp.
- Frames 27759-27772 "USBPcap did not recognize URB Function code" are a USBPcap capture-
  driver limitation, unrelated to rusEFI traffic.

Decisions:
- Classified as cosmetic; no code change made. Not a functional defect.

Validation:
- tshark -z io,phs, per-frame -V dissection, and raw -x hex confirmed the byte layout
  and that all 23 malformed frames share the Mode Sense(6) cause.

Open follow-ups:
- Optional cleanup if the warning ever matters: set the Caching mode page length to 0x12
  and pad the page to the full 20 bytes, or drop the caching page from the Mode Sense
  reply. Lives upstream in ChibiOS-Contrib hal_usb_msd.c.

## 2026-07-14 - SD ECU<->PC switch soak sandbox + USB CDC link-drop investigation

What was done:
- Created a headless soak sandbox SdEcuPcCycleSandbox in the :ui test subproject
  (java_console/ui/src/test/java/com/rusefi/SdEcuPcCycleSandbox.java), modeled on the
  purple-gateway SdPcToEcuSwitchSandbox. It cycles the SD card ECU/logging <-> PC/MSD 10
  times, 20s dwell per mode, confirming each switch via the sd_present / sd_logging_internal
  / sd_msd output channels, and reports a pass/fail tally. Added Gradle task :ui:runSdCycle.
- Initialized two uninitialized git submodules required by the Java build:
  java_console/peak-can-basic (missing peak.can.basic.* -> :ecu_io compile fail) and
  java_console/luaformatter (missing neoe.formatter.lua -> :ui compile fail).

Result of the run (COM149, purple-gateway fw, USB-powered only / no +12V):
- Cycle 1 fully succeeded BOTH directions. PC/MSD->ECU no longer hits FR_DISK_ERR: firmware
  logged "SD: switched from PC/MSD to ECU/logging" and opened log file re_10.mlg; status bits
  confirmed sd_logging_internal=1.
- ~1.5s after the ECU switch the host CDC serial link dropped:
  "output channels: executeCommand failed: java.io.IOException: write failed: wrote 0 but
  expected 11", COM149 closed. Never recovered, so cycle 2's first command got no response and
  the soak aborted at 1/20. Sandbox behaved correctly - it detected and reported the drop.

Root cause (investigated, code-evidenced):
- CDC console and USB mass storage are interfaces on ONE composite USB device (USBD1). The
  config descriptor is fixed at 3 interfaces - MSD IF0 + CDC-control IF1 + CDC-data IF2, 98
  bytes (usbcfg.cpp DESCRIPTOR_SIZE/NUM_INTERFACES). MSD is always present in the enumerated
  descriptor whenever HAL_USE_USB_MSD is built in.
- The SD mode switch does NOT re-enumerate or reconfigure USB. attachMsdSdCard /
  deattachMsdSdCard (mass_storage_init.cpp) merely hot-swap LUN1's backing block device
  between the real SD card and the null device ND1 on the already-running MSD controller.
- Causal chain: PC/MSD->ECU calls deattachMsdSdCard() which swaps LUN1 (SD card -> ND1) while
  Windows still has that mass-storage volume mounted -> the medium vanishes under the mounted
  volume -> the Windows usbstor stack resets/re-enumerates the whole composite device to
  recover -> firmware gets USB_EVENT_RESET/SUSPEND, whose handler calls sduSuspendHookI(&SDU1)
  (usbcfg.cpp:446), tearing down the CDC channel -> host CDC write returns 0, COM149 drops.
- It is host-side (write wrote 0 = port handle invalidated), not a firmware stall: the switch
  completed cleanly, logging started, and the device kept emitting messages up to the drop.

Remediation directions (not implemented - investigation only):
- Don't swap the MSD LUN to a dead null device under a mounted volume. Instead present a stable
  medium or return SCSI "not ready / medium not present" (unit attention) so Windows performs an
  orderly media-eject rather than treating it as a device fault and resetting the port.
- Or signal proper SCSI medium-removal / unit-attention before switching so the host dismounts
  cleanly.
- Host-side, for a true 10x soak: reconnect LinkManager after each switch (treat the CDC drop
  as expected re-enumeration). The current sandbox intentionally reports it instead.
- The existing USB.pcapng capture can confirm the host-issued bus reset around a mode switch.

Validation:
- ./gradlew :ui:compileTestJava BUILD SUCCESSFUL after submodule init.
- ./gradlew :ui:runSdCycle exercised against real hardware; full log captured.

Open follow-ups:
- Decide remediation approach (firmware SCSI media-eject vs host-side reconnect).
- Consider gating: the soak cannot complete 10 cycles over one connection until the CDC drop is
  addressed.

## 2026-07-14 - SD indicator/output-channel name reuse between SdEcuPcCycleSandbox and .ini

What was done:
- Removed the duplicated magic strings sd_present / sd_logging_internal / sd_msd that existed
  independently in output_channels.txt (bit field names), tunerstudio.template.ini (6 indicator
  expressions) and SdEcuPcCycleSandbox.java (SensorCentral lookups). They now flow from a single
  source of truth via the existing code generation.

Change inventory:
| File                                                        | Change                                                          |
|-------------------------------------------------------------|-----------------------------------------------------------------|
| firmware/integration/rusefi_config_shared.txt               | new OUTPUT_CHANNEL_SD_PRESENT / _SD_LOGGING_INTERNAL / _SD_MSD quoted defines |
| firmware/console/binary/output_channels.txt                 | the 3 SD bit fields renamed to @#OUTPUT_CHANNEL_SD_...#@ references |
| firmware/tunerstudio/tunerstudio.template.ini               | 6 indicator lines reference @#OUTPUT_CHANNEL_SD_...#@           |
| java_tools/.../ReaderStateImpl.java (config_definition_base)| handleBitLine now applies variable substitution to the bit name (comment stays templated, matching plain-field parsing) |
| java_tools/.../ConfigFieldParserTest.java                   | new testBitNameViaVariableReference                             |
| java_console/.../SdEcuPcCycleSandbox.java                   | uses VariableRegistryValues.OUTPUT_CHANNEL_SD_* constants       |
| java_tools/version/.../UiVersion.java                       | CONSOLE_VERSION -> 20260714                                     |

Key decisions and why:
- Constants live in rusefi_config_shared.txt because it is prepended by BOTH pipelines that need
  them: gen_config_common.sh (template .ini + VariableRegistryValues.java) and the LiveData.yaml
  output_channels entry (LiveDataProcessor parsing output_channels.txt).
- Used the existing @#NAME#@ quote-stripping substitution (same as TS_HELLO_COMMAND usages) so the
  quoted define yields a bare identifier in struct field names and { } indicator expressions while
  generating a proper Java String constant.
- handleBitLine substitution was narrowed to the name part only: applying it to the whole line
  expanded @@...@@ comment templates at parse time, which changed engine_state_generated.h
  (clutchDownState comment). The narrowed version keeps every generated artifact byte-identical.
- Did not resurrect the deleted generated TsOutputs.java (removed in #6711); defines + existing
  VariableRegistryValues generation is the sanctioned mechanism.

Validation:
- ./gradlew :config_definition:test :config_definition_base:test green including the new test.
- gen_live_documentation.sh + gen_config_board.sh f407-discovery: all generated outputs
  (output_channels_generated.h, live_data_fragments.ini, data_logs.ini, board .ini indicator
  lines) byte-identical to committed state; only VariableRegistryValues.java gains the 3 new
  String constants. Board-generated .h/.ini signature/date churn reverted (CI regenerates).
- ./gradlew :ui:compileTestJava BUILD SUCCESSFUL with the sandbox on the generated constants.

Open follow-ups:
- Other magic output-channel names shared between java_console and .ini (e.g. sd_error,
  sd_formating, sd_active_wr/rd) could adopt the same pattern when java code starts using them.

---

## 2026-07-14 - N52 preset: bake in TPS/PPS calibration from reference tune

What: Ported TPS + PPS calibration out of the "super N52" TunerStudio tune (CurrentTune.msq)
into the `bmwN52()` engine preset in `firmware/config/engines/bmw.cpp`, so a fresh N52 selection
ships with the real throttle-body/pedal calibration instead of raw defaults.

| Field(s)                                                   | Source (msq, volts) | Stored as |
|------------------------------------------------------------|---------------------|-----------|
| tpsMin / tpsMax                                            | 4.545 / 0.58        | 10-bit ADC via convertVoltageTo10bitADC |
| tps1SecondaryMin / tps1SecondaryMax                       | 0.75 / 4.72         | 10-bit ADC via convertVoltageTo10bitADC |
| throttlePedalUpVoltage / throttlePedalWOTVoltage          | 0.625 / 2.230       | float volts (verbatim) |
| throttlePedalSecondaryUpVoltage / ...WOTVoltage           | 0.947 / 4.197       | float volts (verbatim) |

Key decisions:
- Used shared helpers setTPS1Calibration()/setPPSCalibration() (defaults.h), matching subaru.cpp.
- TPS stored as 10-bit ADC counts (not volts): wrapped msq volts in convertVoltageTo10bitADC
  (= volts*200). PPS stored as float volts, copied through directly. This units split is the main
  porting hazard and is now documented.
- Skipped tps2* fields: msq had them at defaults (0/5, 5/0); N52 runs a single dual-sensor throttle.
- Added #include "defaults.h"; convertVoltageTo10bitADC comes transitively via pch.h.

Docs: new docs/AI/engine_presets.md documents the canned-tune/preset process end to end
(enum -> engine_type_impl.cpp dispatch -> config/engines setup fn), the TPS-vs-PPS units gotcha,
and a step-by-step msq->preset porting recipe.

Validation: static review only - mirrors the established subaru.cpp calibration pattern; all
referenced helpers are declared in the included headers. No generated files touched (presets are
plain code, no gen_config step).

Open follow-ups:
- Confirm on hardware that a defaults-reset N52 reads plausible TPS
---

## 2026-07-14 - N52 preset: bake in TPS/PPS calibration from reference tune

What: Ported TPS + PPS calibration out of the "super N52" TunerStudio tune (CurrentTune.msq)
into the bmwN52() engine preset in firmware/config/engines/bmw.cpp, so a fresh N52 selection
ships with the real throttle-body/pedal calibration instead of raw defaults.

| Field(s)                                           | Source (msq, volts) | Stored as |
|----------------------------------------------------|---------------------|-----------|
| tpsMin / tpsMax                                     | 4.545 / 0.58        | 10-bit ADC via convertVoltageTo10bitADC |
| tps1SecondaryMin / tps1SecondaryMax                | 0.75 / 4.72         | 10-bit ADC via convertVoltageTo10bitADC |
| throttlePedalUpVoltage / throttlePedalWOTVoltage   | 0.625 / 2.230       | float volts (verbatim) |
| throttlePedalSecondaryUpVoltage / ...WOTVoltage    | 0.947 / 4.197       | float volts (verbatim) |

Key decisions:
- Used shared helpers setTPS1Calibration()/setPPSCalibration() (defaults.h), matching subaru.cpp.
- TPS stored as 10-bit ADC counts (not volts): wrapped msq volts in convertVoltageTo10bitADC
  (-> volts*200). PPS stored as float volts, copied through directly. This units split is the
  main porting hazard and is now documented.
- Skipped tps2* fields: msq had them at defaults (0/5, 5/0); N52 runs a single dual-sensor throttle.
- Added #include "defaults.h"; convertVoltageTo10bitADC comes transitively via pch.h.

Docs: new docs/AI/engine_presets.md documents the canned-tune/preset process end to end
(enum -> engine_type_impl.cpp dispatch -> config/engines setup fn), the TPS-vs-PPS units gotcha,
and a step-by-step msq->preset porting recipe.

Validation: static review only - mirrors the established subaru.cpp calibration pattern; all
referenced helpers are declared in the included headers. No generated files touched (presets are
plain code, no gen_config step).

Open follow-ups:
- Confirm on hardware that a defaults-reset N52 reads plausible TPS%/pedal% before user tuning.
---

## 2026-07-15 - SD card logging: overview doc + code comments (f_expand logic)

What: Documented the SD card logging subsystem and annotated its three core source files.

| File | Change |
|-------------------------------------------------------|--------------------------------------------|
| docs/AI/sd_card_logging.md (new)                       | End-to-end overview: SD thread mode state machine (IDLE/ECU/PC/UNMOUNT/FORMAT, all transitions via IDLE), .mlg vs .teeth loggers, FileBufferedWriter path, f_expand pre-allocation, file naming, status channels, console commands |
| firmware/hw_layer/mmc_card.cpp                         | Expanded LOGGER_MAX_FILE_SIZE, f_expand and f_truncate comments; doc pointer in file header |
| firmware/console/binary_mlg_log/binary_mlg_logging.cpp | File-header overview (MLG v2 layout, who owns file lifecycle); comments on writeFileHeader/writeSdBlock/writeSdLogLine/resetFileLogging |
| firmware/console/binary/tooth_logger.cpp               | Comment on freeBuffers/filledBuffers multi-buffering (BigBuffer, interrupt producers -> TS/SD consumers); ToothLoggerWriter() contract incl. 3s idle timeout -> new file |
| CLAUDE.md                                              | Added sd_card_logging.md to Deep Dive AI Guidance list |

Key facts captured (the f_expand logic in particular):
- sdLoggerCreateFile() pre-allocates each log file to 32Mb with f_expand(fd, size, opt=1)
  (allocate-now, contiguous; FF_USE_EXPAND=1 in firmware/ext/FatFS/ffconf.h). All FAT
  updates happen up-front, so writes inside the pre-allocated area never touch FAT
  structures -> sudden power loss loses buffered data but not the filesystem.
- f_expand failure (fragmented card) is deliberately non-fatal: FatFS falls back to
  cluster-by-cluster growth, logging works without the corruption protection.
- sdLoggerCloseFile() f_truncate()s back to actual size; a power-lossed file stays 32Mb
  with trailing garbage.
- Both loggers share sdLoggerCreateFile() (so .teeth files are pre-allocated too) but
  only sdLoggerMlg() enforces the 32Mb rollover cap.

Validation: comment/doc-only changes, no code touched; facts verified against source
(mmc_card.cpp, file_writer.h, ffconf.h FF_USE_EXPAND=1, tooth_logger.cpp, sd_log_trigger.h).

Open follow-ups: none.

## 2026-07-15 - Lua scripting API: categorized hook inventory doc

What: Reviewed every custom Lua method registered around lua_hooks.cpp and documented
them in a new docs/AI/lua_scripting.md, grouped into 11 categories.

| File | Change |
|-------------------------------|--------------------------------------------------------|
| docs/AI/lua_scripting.md (new) | Full inventory of Lua hooks by category: input reads, virtual sensors, virtual switches, closed-loop trims, cut/disable controls, PWM/DAC outputs, CAN, config/calibration access, state queries, luaaa helper classes, framework/test hooks; plus registration-site map, indexing conventions, build-flag gating, and an "adding a new hook" recipe |
| CLAUDE.md | Added lua_scripting.md to the Deep Dive AI Guidance list |

Key decisions / findings:
- Registration is spread over four files: lua_hooks.cpp (bulk + luaaa classes),
  lua_hooks_util.cpp (print/interpolate/find*/mcu_standby), lua.cpp (setTickRate,
  onTick dispatch), lua_can_rx.cpp (onCanRx dispatch, global_can_data workaround).
  lua_hooks_ext.cpp is an empty extension point; boardConfigureLuaHooks() is a weak
  board hook with no in-tree overrides.
- Documented the mixed indexing convention explicitly: 1-based (HUMAN_OFFSET) for
  TS-facing entities (CAN bus, curves, tables, TS buttons, gauges), 0-based for
  sensor indices, PWM channels, aux digital inputs, vin().
- Documented flash-saving exclusions (#if !defined(STM32F4) group) and the
  DISABLE_LUA_* / WITH_LUA_* opt-out macros.
- setTickRate code clamps 1..2000 Hz while its comment says 1..200 - doc records
  the code behavior (comment discrepancy left in source, not a functional issue).

Validation: doc-only change; every listed hook, guard macro and constant
(LUA_PWM_COUNT=8, LUA_GAUGE_COUNT=8, LUA_DIGITAL_INPUT_COUNT=8, LUA_BUTTON_COUNT=10,
CMD_BURNCONFIG="burnconfig") verified against source via grep/read of the four
registration files, lua_pid.h and rusefi_config.txt.

Open follow-ups:
- lua.cpp setTickRate comment ("Limit to 1..200 hz") disagrees with clampF(1, x, 2000).
- lua_hooks.cpp has a commented-out hasCriticalReportFile hook referencing issue #7291.

## 2026-07-17 - loss-of-cdc.pcapng analysis: one-shot composite reset from pre-capture MSD wedge

What: Analyzed loss-of-cdc.pcapng (repo root, USBPcap, 24.6 s, captured 2026-07-17
12:25 - i.e. the day AFTER the #9860 fix series landed) against the recent
mass_storage changes. Goal: confirm/refute whether the CDC drop mechanism from
issue #9860 is still present.

Devices in capture: address 21 = the ECU (VID 0483:5740, composite MSD+CDC),
address 22 = PEAK PCAN-USB adapter (19.9k of the 22k packets - unrelated noise).

Timeline (t = seconds from capture start):
- t=0..10.8: ZERO MSD traffic from the ECU. A healthy medium-less device gets
  ~1 Hz Test Unit Ready polls (visible later in this same capture), so at capture
  start usbstor already had one command in flight that never completed - the MSD
  side was already wedged/stuck before the capture began.
- t=7.79: host opens the COM port (GET/SET LINE CODING burst); CDC request/reply
  traffic (TS-style 7/11-byte commands, 1024-byte replies) runs cleanly for 3 s.
- t=10.847: usbstor ~20 s give-up timer fires -> all-endpoint cancel storm on the
  ECU: 10 URBs with USBD_STATUS_CANCELED (0xc0010000) - MSD bulk-IN 0x81 (the
  stuck data/CSW read, pending since before capture start), CDC data 0x82/0x02,
  CDC interrupt 0x83, plus control. This is the loss-of-CDC moment.
- t=10.883: host immediately retries line coding - those control URBs are
  canceled too (device still resetting).
- t=11.03..11.05: MSD recovers: Test Unit Ready on LUN0 and LUN1 -> Check
  Condition -> Request Sense (Good) -> Mode Sense(6) (the known-cosmetic
  "malformed" short caching page). Both LUNs report medium-not-present.
- t=11.28: CDC port re-opens at USB level (line coding OK) but NO data traffic
  follows - the app-level session was dead, host serial layer sat in its ~10 s
  timeout.
- t=12..24.5: clean steady state: 1 Hz TUR polls per LUN, no stalls, no babble,
  no further cancels or resets.
- t=20.79: app fully reconnects (line coding + control line state), TS-style
  traffic resumes. Total user-visible CDC outage: ~10 s (10.85 -> 20.79).

Reading vs the 2026-07-16 fix series (298162eb075..68e7d77c042, all in
firmware/hw_layer/mass_storage/):
- 298162eb0/8a515546c (MSD diag #9838): sdinfo diagnostics incl. per-opcode
  in-flight timer.
- e1feee380 (isCommandAbandoned #9861): 10 s data-phase timeouts on all SCSI
  transfers + CSW via msdUsb*Timeout helpers -> wedged thread self-recovers,
  re-arms bulk-OUT.
- 12b613c59 (#9864): LUN detach now synchronizes with in-flight command
  (m_lunMutex held around scsiExecCmd+CSW) -> kills the SPI double-waiter
  deadlock from the SD mode switch.
- 04331c28f (#9866) + 68e7d77c0 (uaefi): medium-less data-IN commands answered
  with ZLP instead of STALL -> no EP0 clear-halt round-trip near CDC traffic.
The capture is consistent with the fixes WORKING as designed for the recurring
part: exactly ONE reset (the tail of a wedge that began ~9 s before capture,
matching usbstor's ~20 s timer), then 13.5 s of clean behavior with no repeat
reset - the old signature was a reset every ~20 s.

Remaining gap (why one reset still happens): the firmware 10 s data-phase
timeout releases the MSD *thread*, but leaves the *host's* pending IN URB
hanging - firmware just returns to CBW wait and never completes/STALLs the
IN transfer the host is still waiting on. usbstor therefore still escalates to
a full composite reset once, taking CDC down with it. A full fix would complete
the host's data phase on timeout (e.g. STALL the IN endpoint so the host gets
an immediate error -> clear-halt -> CSW path) instead of leaving the URB
pending. Caveat: cannot verify from the capture which firmware build was
flashed or which opcode wedged (the CBW predates the capture); console sdinfo
counters (data-phase timeouts / no-data ZLPs) on the connected unit would
distinguish "fixed firmware, host-side URB gap" from "stale firmware".

Validation: tshark 3.6.2 field-level analysis (usb.usbd_status, endpoints,
SCSI dissection); code cross-checked at HEAD (mass_storage_device.cpp timeout/
ZLP/mutex mechanisms present).

Open follow-ups:
- On data-phase timeout, also complete the host-visible transfer (STALL data-IN
  or arm+flush) so usbstor never needs its 20 s reset - would remove the single
  remaining CDC drop.
- Confirm via sdinfo on hardware whether the flashed build has the 07-16 fixes
  and whether data-phase timeout counters tick.

## 2026-07-17 - MSD data-phase timeout: close the command host-side (stall + phase-error CSW)

What: Implemented the follow-up from the loss-of-cdc.pcapng analysis (previous
entry). Before this change, a data-phase timeout only freed the MSD *thread*
(e1feee380 #9861); the *host's* pending URB was left hanging and the CSW was
skipped, so usbstor still escalated to one full composite-device reset per
wedge - taking the CDC console down for ~10 s each time.

| File | Change |
|----------------------------------------------------|----------------------------------------|
| firmware/hw_layer/mass_storage/mass_storage_device.cpp | ThreadTask: split the abandoned-command check. BOT reset still skips the CSW (host is not expecting one). Data-phase timeout now STALLs the data endpoint in the CBW's direction (usbStallTransmitI/usbStallReceiveI) and then sends a CSW with CSW_STATUS_PHASE_ERROR and honest residue. sendCsw() now returns whether the host read the CSW; sdinfo prints "N data-phase timeouts (M closed by CSW)" |
| firmware/hw_layer/mass_storage/mass_storage_device.h | sendCsw() -> bool; new m_timeoutCswDeliveredCount counter |

Key decisions and why:
- STALL is the BOT-sanctioned "cannot complete this data phase" signal: a host
  still waiting on its data URB completes it with an error immediately (well
  before usbstor's ~20 s give-up), does a clear-halt on this one endpoint, and
  collects the CSW - recovery stays class-level on the MSD interface, the CDC
  endpoints never notice. A host that already canceled its URBs ignores the
  stall and resets anyway - no worse than before.
- Arming the CSW while the endpoint is still stalled is the exact sequence the
  pre-ZLP medium-less path used (04331c28f), already validated on Windows
  hardware (STALL -> clear-halt -> CSW observed on the wire).
- CSW_STATUS_PHASE_ERROR rather than FAILED: after a broken data phase the
  transport has genuinely lost sync; phase error makes the host run Bulk-Only
  Reset Recovery (class request + clear both halts), fully resynchronizing
  data toggles without any port-level reset. The existing onBulkOnlyResetIsr
  path handles that request.
- Safe to stall: all three msdUsb*Timeout helpers clear the endpoint's
  active flag on timeout, so usbStall*I (which refuses while a transfer is
  active) always takes effect by the time ThreadTask runs the recovery.
- The no-data-ZLP timeout path intentionally keeps its plain 'continue': a
  host that will not even take a zero-length packet is gone from the data
  phase entirely; its next action is a new CBW (accepted normally) or a reset.

Validation: uaefi firmware build (see below). No unit-test coverage exists for
this path (EFI_PROD_CODE + HAL_USE_USB_MSD only). Hardware validation plan:
reproduce the wedge (host abandons a command mid-data-phase), then check
1. sdinfo shows "closed by CSW" ticking together with data-phase timeouts,
2. a capture shows STALL -> clear-halt -> CSW(phase error) -> BOT reset
   instead of the all-endpoint cancel storm,
3. the CDC console stays connected across the event.

Open follow-ups:
- Wedges *below* the USB layer (e.g. blkRead stuck on a dying SD card) are
  still uncovered: no timeout wraps lib_scsi's block-device calls, so such a
  wedge never reaches the new recovery path (lib_scsi is in ChibiOS-Contrib).
- The loss-of-cdc.pcapng pre-capture wedge could not be attributed (stale
  firmware vs blkRead wedge); confirm the flashed build via sdinfo counters.

## 2026-07-23 - alphax-4chan_f7: Lua script (TS page 5) not persisting across reboot

Reported symptom: sending a Lua script to the ECU via console on
alphax-4chan_f7 runs immediately but does not survive a power cycle, while
the same workflow is fine on alphax-s550-pnp (also an F7/mm176 board).

Root cause: page5_s (which holds page5_s::luaScript,
firmware/integration/config_page_5.txt:7) is an "extra page" - on F7 boards
it only gets an internal-flash backend if board.mk both (a) includes
hw_layer/ports/stm32/2mb_flash.mk, which relocates config storage above the
first 1.5MB of flash into 128KB sectors (the only F7 layout where extra-page
piggybacking is valid, per the STM32F7XX guard in
storage_flash.cpp::getExtraPageFlashAddr()), and (b) sets
-DEFI_STORAGE_SD=FALSE so a stale SD custom_page.bin doesn't clobber the
flash copy of page 5 on read (EFI_STORAGE_SD defaults TRUE). Without both,
the page has no backend and silently resets to defaults every boot - live
uploads work because they only touch RAM.

This is the same defect class previously found and fixed on alphax-gold (see
FLASH_DATA_VERSION / page-5 flash persistence history). alphax-4chan's F7
branch in firmware/config/boards/hellen/alphax-4chan/board.mk was never
given the fix; alphax-gold, alphax-s550-pnp, alphax-4K-GDI, uaefi, and
super-uaefi all already include 2mb_flash.mk.

Fix: added the same two lines (2mb_flash.mk include + EFI_STORAGE_SD=FALSE)
inside the ARCH_STM32F7 branch of alphax-4chan/board.mk, mirroring
alphax-s550-pnp's board.mk verbatim (comments included).

Validation: `./compile_alphax-4chan_f7.sh -j12` builds clean. Flash layout
now shows the same relocated split as alphax-gold (flash0 1504KB / flash1
1504KB, 44.56%/33.34% used) instead of a single un-split flash region.
Hardware validation (flash Lua script, power-cycle, confirm it survives) not
yet performed - firmware image only.

Open follow-ups:
- Flash the built image to hardware and confirm the Lua script (and other
  page 5 fields, e.g. custom lookup tables) survive a real power cycle.
- Consider a compile-time guard (like the existing page5_container_s static
  assert) that fails the build for any F7 board defining page-5-backed
  features without EFI_STORAGE_INT_FLASH properly wired, so this class of
  bug can't recur silently on a new board.

## 2026-07-30 - TCU Input Speed Sensor: "shared with main VSS" toggle

What was done:
- Added a dropdown to the TCU Input Speed Sensor panel letting the user mark
  that sensor as physically the same wire as the main Chassis VSS, instead of
  a second independent input.

Key decisions and why:
- Config field: repurposed the unused `devBit0` placeholder
  (firmware/integration/rusefi_config.txt) as
  `tcuInputSpeedSensorSharedWithVss` rather than growing the struct, avoiding
  a FLASH_DATA_VERSION bump (see FLASH_DATA_VERSION bump history). Removed
  its stale `field = devBit0, devBit0` line from the "Experimental 3" parking
  lot dialog in tunerstudio.template.ini.
- Investigated whether "shared" could simply mean pointing
  tcuInputSpeedSensorPin at the same physical pin as
  vehicleSpeedSensorInputPin. Ruled out: firmware/hw_layer/digital_input/digital_input_exti.cpp
  keeps one ExtiChannel per physical pin index (`channels[16]`); a second
  FrequencySensor::initIfValid() on an already-claimed pin hard-fails via
  firmwareError(CUSTOM_ERR_PIN_ALREADY_USED_2) and returns -1 - two
  independent EXTI registrations on one pin are not possible on STM32.
- Implemented instead as edge fan-out inside FrequencySensor
  (firmware/controllers/sensors/frequency_sensor.{h,cpp}): a FrequencySensor
  can now be initialized via initShared() to skip owning a pin/EXTI callback
  entirely and instead receive the raw edge frequency from another
  FrequencySensor's onEdge() through a new onSharedEdge() path
  (setSharedListener()/m_sharedListener). Each side still applies its own
  biquad filter and its own SensorConverter (VehicleSpeedConverter vs.
  InputShaftSpeedConverter) independently on top of the same raw frequency,
  so filter tuning (vssFilterReciprocal vs issFilterReciprocal) and units
  (km/h via gear ratio vs. RPM via tooth count) stay fully independent even
  though the physical signal is shared.
- firmware/init/sensor/init_input_shaft_speed_sensor.cpp now branches on
  engineConfiguration->tcuInputSpeedSensorSharedWithVss: shared mode calls
  inputShaftSpeedSensor.initShared(...) and attaches it as
  vehicleSpeedSensor's shared listener (extern-declared from
  init_vehicle_speed_sensor.cpp); non-shared mode is unchanged
  (initIfValid on tcuInputSpeedSensorPin). Both init and deinit paths clear
  the listener to avoid a stale pointer across a mode switch or engine
  reconfiguration.
- TS UI (tunerstudio.template.ini, inputSpeedSensorPanel): added the
  "Shared with main VSS" field; "Input Pin" is now hidden when shared is
  enabled; "Filter parameter" stays visible either way since it still
  applies to the shared reading. tcuInputSpeedSensorTeeth is left
  independently user-editable in both modes (not auto-derived from
  vssToothCount) - kept the change minimal since only the pin-sharing
  dropdown was requested.

Validation:
- `./gradlew -p java_tools :config_definition:shadowJar` - rebuilt a stale
  config_definition-all.jar first (unrelated pre-existing issue on this
  branch, see Stale config_definition jar history: a page5_s static_assert
  was failing at 8000 vs 10000 bytes purely from the stale jar, nothing to
  do with this change).
- `unit_tests/test.sh` - full suite compiles and links; 1297/1298 pass. The
  one failure (ClosedLoopFuel.StateBasedRegionMapping, a
  ShortTermFuelTrim::regionForSmState(S::Coasting) mismatch) is unrelated to
  this change - none of the edited files touch STFT/region-mapping code -
  and predates this session's work.
- Confirmed generated INI (firmware/tunerstudio/generated/rusefi_f407-discovery.ini)
  renders the field correctly: `tcuInputSpeedSensorSharedWithVss = bits, U32,
  1680, [18:18], "Disabled", "Enabled"` (defaults to Disabled/not-shared),
  and the Input Pin field's visibility condition
  `{ !tcuInputSpeedSensorSharedWithVss }` is present.
- Not yet built for a real board or tested on hardware.

Open follow-ups:
- Hardware validation: confirm InputShaftSpeed reads sensibly when
  tcuInputSpeedSensorSharedWithVss is enabled and vehicleSpeedSensorInputPin
  is wired to the transmission input shaft sensor.
- Consider whether tcuInputSpeedSensorTeeth should warn/default from
  vssToothCount when shared is enabled, if users find the duplicate field
  confusing in practice.

## 2026-07-31 - A/C pressure fan mode: pressure is now the command, independent of compressor state

What was done:
- Fixed `FanController::enabledForAcByPressure()` (firmware/controllers/modules/fan_control/fan_control.cpp)
  in the existing `EFI_AC_PRESSURE_FAN` / `fan_ac_mode_e::Pressure` feature: previously it
  required `acActive` (A/C compressor currently enabled) AND a valid AcPressure reading before
  the fan could be commanded on. Per user direction, when a fan is set to Pressure mode the
  high-side pressure reading is the command by itself - if pressure is over the On threshold,
  the fan turns on regardless of whether the A/C compressor is currently engaged (e.g. static
  heat soak with the clutch open still raises high-side pressure and should still get airflow).
- Dropped the now-unused `acActive` parameter from `enabledForAcByPressure()` (and its call site
  in `FanController::getState()`); Relay mode is untouched and still gates on `acActive`.
- Updated the `fan1AcMode`/`fan2AcMode` field docs and the struct comment block in
  firmware/integration/config_page_6.txt to state that Pressure mode is independent of
  compressor state.

Decisions:
- Scoped strictly to the on/off relay-mode fan logic (`getState`/`enabledForAcByPressure`).
  Left the PWM-mode A/C adder path (`onSlowCallbackPwm`, still keyed off `acActive` via
  `getPwmAcAdder()`) untouched - it's a separate pre-existing TODO (pressure-proportional PWM
  curve) not part of this request.

Validation:
- Added `Actuators.FanAcPressureModeIgnoresCompressorState` in
  unit_tests/tests/actuators/test_fan_control.cpp: with the mock A/C compressor OFF, drives
  AcPressure sensor above/below the on/off thresholds and confirms the fan follows pressure
  alone (also covers invalid-pressure-reading fail-safe: fan not commanded on).
- `unit_tests/test.sh Actuators.FanAcPressureModeIgnoresCompressorState` - passes.
- Full `test.sh "Actuators.Fan*"` re-run after the (separately fixed) Oil Life Monitor
  gauge-name-length codegen issue was resolved: codegen and build now succeed; all 10
  Actuators.Fan* tests pass, including the new
  Actuators.FanAcPressureModeIgnoresCompressorState.

Open follow-ups:
- Consider whether the PWM-mode A/C adder should eventually get the same pressure-is-the-command
  treatment as the relay-mode path (existing TODO in fan_control.cpp).

## 2026-07-31 - Feature: Weighted Engine Oil Life Monitor (EFI_OIL_LIFE_MONITOR)

What was done:
- New AlphaX page-6 feature: tracks a temperature-weighted cumulative engine-revolution counter
  and reports remaining oil life as a percentage (`oilLifePercent` output channel), plus which
  temperature source is active (`oilLifeTempSource`).
- New module `firmware/controllers/modules/oil_life_monitor/` (`OilLifeMonitor : EngineModule`),
  registered in `engine.h`'s `type_list` and `modules/modules.mk`, gated by a new
  `EFI_OIL_LIFE_MONITOR` flag (FALSE on f4, TRUE on f7/h7 - see FEATURE_FLAGS.md).
- Algorithm: every `onSlowCallback()` tick, diffs `getRevolutionCounter()` against the last-seen
  value, reads `SensorType::OilTemperature` (falls back to `SensorType::Clt` if invalid, tracked
  via an internal `TempSource` enum), looks up a per-zone multiplier (fixed thresholds 70/105/125
  degC; 8 TS-tunable multipliers, oil vs. coolant-fallback x 4 zones), and accumulates
  `revs * multiplier` into an in-RAM `uint32_t`. `oilLifePercent = clamp(0,100, (1 - weightedRevs /
  (oilLifeRevsScaleMillions * 1e6)) * 100)`; `oilLifeRevsScaleMillions` is a TS-tunable 1-100 scalar.
- Persistence: deliberately NOT periodic. Accumulates in RAM for the whole time the ECU is
  powered and flushes to flash exactly once, on ignition-off, via a new `EFI_OIL_LIFE_RECORD_ID`
  storage item (`storage.h`/`storage.cpp`/`storage_sd.cpp`) and the `needsDelayedShutoff()`
  EngineModule hook (holds the main relay open until the write completes). Because of this, the
  feature requires `EFI_MAIN_RELAY_CONTROL` and enforces it with a compile-time `#error` guard -
  confirmed firing correctly for all four TRUE/FALSE combinations via a standalone preprocessor
  check. Consequently the feature is intentionally NOT enabled in the simulator
  (`simulator/simulator/efifeatures.h` sets `EFI_MAIN_RELAY_CONTROL FALSE`).
- Reset: both a TS command button (new `OIL_LIFE_RESET` in `bench_mode_e`, wired through
  `bench_test.cpp` / `cmd_oil_life_reset` / a new dialog under `&Advanced`) and a Lua function
  `resetOilLifeMonitor()` (`lua_hooks.cpp`, documented in `docs/AI/lua_scripting.md`), both calling
  the same `OilLifeMonitor::reset()`.
- Config fields added to `config_page_6.txt` (`PAGE6_DATA_VERSION` bumped 19 -> 20, defaults added
  in `custom_page.cpp`); output channels added to `console/binary/output_channels.txt` and wired
  in `status_loop.cpp`.

Decisions:
- Config scope: user chose AlphaX page 6 + dedicated `EFI_` flag (not a master-portable
  `rusefi_config.txt` addition), matching every other custom feature already on this branch.
- Zone multipliers are 8 discrete named TS fields, not a `float[8]` array - the zones are a fixed
  step function, not an interpolated curve, so discrete labeled fields read clearer in the TS UI.
- No periodic flash save (unlike the LTFT `SAVE_AFTER_HITS` precedent this was originally modeled
  on) - user explicitly rejected it: flash can't be written while the engine runs on most boards
  anyway, so the only meaningful save point is ignition-off.
- `oilLifePercent` is an `output_channels.txt` gauge only, no `LiveData.yaml` live-dialog-panel
  addition (used by Misfire/Burst Knock/WOT Enrichment) - not required by the spec.

Validation:
- `unit_tests/tests/test_oil_life_monitor.cpp` (6 tests: percent formula incl. div-by-zero
  fail-open, zone boundary edges for both temp sources, sensor fallback flip/recovery,
  weighted-rev accumulation math, shutdown-save `needsDelayedShutoff()` transition, reset) - all
  pass. Plus `LuaHooks.ResetOilLifeMonitor` in `test_lua_hooks.cpp` exercising the actual Lua
  binding. Full `unit_tests/test.sh` run: 1305/1306 pass; the one failure
  (`ClosedLoopFuel.StateBasedRegionMapping`) is pre-existing and unrelated (confirmed failing in
  isolation on a clean `git status` for the STFT files it touches).
- Full firmware build: `proteus` F7 (`EFI_OIL_LIFE_MONITOR=TRUE`) links successfully. `f407-discovery`
  F4 (`EFI_OIL_LIFE_MONITOR=FALSE`, stub path) - all Oil-Life-Monitor-touched object files
  (`oil_life_monitor.o`, `storage.o`, `custom_page.o`, `status_loop.o`, `bench_test.o`,
  `lua_hooks.o`, `storage_sd.o`) compiled cleanly; the board's full link was blocked by a
  pre-existing, unrelated `ramdisk_image` codegen issue on this branch (stale placeholder left
  over from a prior `proteus_f7` build; matches a previously-recorded F4 INI-ramdisk-limit issue on
  this branch) - not caused by this feature, not fixed here.
- Found (via a concurrent session's report entry above, which hit it as a side effect) and fixed a
  real bug this feature introduced: the initial `oilLifePercent`/`oilLifeTempSource` output-channel
  comments exceeded `DataLogConsumer`'s 34-char gauge-name limit, breaking `LiveDataProcessor`
  codegen for every board. Fixed by giving both fields a short first-line gauge name
  (`Oil Life %` / `Oil Life Temp Src`) with the longer description on a second `\n`-separated line,
  matching the existing `actualLastInjectionRatio`-style convention.
- Confirmed the `#error` guard with a standalone `g++` preprocessor check across all four
  `EFI_OIL_LIFE_MONITOR` x `EFI_MAIN_RELAY_CONTROL` TRUE/FALSE combinations.

Open follow-ups:
- The pre-existing `ramdisk_image` / F4 INI codegen issue still blocks a full f407-discovery link;
  unrelated to this feature.
- The pre-existing `ClosedLoopFuel.StateBasedRegionMapping` unit test failure is unrelated and
  still open.
- No hardware validation yet (flash persistence across a real power cycle, TS dialog/button
  round-trip) - only unit tests and firmware compilation were exercised this session.

## 2026-08-01 - Transmission Settings / TCU: trim UI to a minimal working subset, add 5 gauges

Context: the native TCU stack (`firmware/controllers/tcu/`) is marked by its own readme as
"very unfinished... no plans to invest into this area" (09/2022). Its TS "Transmission
Settings" surface had accumulated dialogs for features that either don't work end-to-end
(line-pressure-per-gear/per-shift, 3-2 solenoid duty table - all `Gm4l6x`-only) or become
unreachable once mode-selection dropdowns are removed (range-selector matrix belongs to
`GearControllerMode::Generic`, button-shift belongs to `ButtonShift` - neither is the mode
being defaulted to). User also supplied a working reference Lua TCU script (CAN-sourced
sensors, VSS/TPS shift tables + WOT curves, 2-solenoid PWM, idle-forced-1st-gear, 5
`setLuaGauge()` outputs) to compare against the native implementation and mine for gauge
ideas. Comparison finding: the Lua script isn't a new architecture, it's a more complete,
currently-functional version of what `AutomaticGearController`/`SimpleTransmissionController`
already do on paper (VSS/TPS shift-table state machine, per-gear solenoid pattern table) -
just missing WOT curves, idle-shift, and gear-verification timeout. This pass ports the gauge
surface only, not the WOT-curve algorithm (separate future project).

What was done:
- `tunerstudio.template.ini`: rewrote `transmissionPanel` (kept TCU Enabled, added "# of
  Gears" locked to 4), `shiftSolenoidPanel` (trimmed from 6 solenoids + 3-2 solenoid down to
  just Solenoid 1 & 2), `otherSolenoidPanel` (kept TCC on/off, TCC PWM, pressure control -
  ungated by mode), migrated `inputSpeedSensorPanel` into `tcuControls` as a 5th panel.
  Deleted `buttonShiftInputPanel`, `rangeMatrixInputPanel`, `gearControls` (emptied once its
  two panels were removed), `inputSpeedSensor` (standalone wrapper), `pcPerGearDialog`,
  `pcPerShiftDialog`, `32Dialog`, `rangeMatrixDocumentation`, `rangeMatrixDialog`. Left
  `shiftSpeedDialog`, `tccCurves`, `tcuSolenoidTableTbl` untouched per explicit user decision.
- `top_level_menu.ini`: removed the 6 now-dead subMenu lines to match.
- `rusefi_config.txt`: `totalGearsCount` (pre-existing but never TS-exposed field) got its
  `lo,hi` clamped from `1, TCU_GEAR_COUNT` to `4,4` plus a descriptive comment - this is the
  zero-risk way to render a scalar as a locked spinner in this ini dialect (no per-field
  min/max override syntax exists). Added a `//`-comment near `TCU_SOLENOID_COUNT` documenting
  future 6-solenoid support (10R80 motivating example).
- `default_base_engine.cpp`: `defaultsOrFixOnBurn()` now forces `gearControllerMode` ->
  `Automatic` and `transmissionControllerMode` -> `Generic4` whenever `tcuEnabled` is true and
  they're still `None` (the as-shipped default) - the established idiom for "hardcode a
  default without a config-layout change" (see `docs/calibration-compatibility.md`), so TCU
  works without the now-removed mode dropdowns. Guarded by `== None` so explicit presets
  (`configureTcu4R70W()`) and existing unit tests are untouched.
- `tcu_controller.txt` + `simple_tcu.cpp` + `gc_auto.cpp` + `gauge_declarations.ini`: added 5
  new fields modeled on the Lua script's gauges, all under the existing `gaugeCategory =
  Transmission` block, gated `@@if_show_tcu_gauges` like their nearest siblings:
  `tcu_solenoid1On`/`tcu_solenoid2On` (read back from the existing `tcuSolenoidTable` lookup
  in `SimpleTransmissionController::update()`), `tcu_idleShiftToFirst` (new behavior - forces
  a downshift to 1st at idle in `AutomaticGearController::update()`, previously nothing did
  this), `tcu_upshiftMargin`/`tcu_downshiftMargin` (captured from the existing `curveSpeed`
  computation in `AutomaticGearController::shift()`).
- `unit_tests/tests/test_tcu.cpp`: added 5 new tests (mode defaulting on/off, mode not
  clobbered when already explicit, idle-shift-to-first fires and clears, solenoid/margin
  gauge values against the `TCU_4R70W` preset's known shift tables at a bin-aligned TPS).

Gotchas hit:
- `rusefi_config.txt`/`*.txt` struct-definition comments do NOT support `;`-prefixed
  standalone line comments (that's an `.ini`-only convention) - `ConfigDefinition` throws
  `Cannot parse line`. Standalone comments in these files use `//` (or legacy `!`).
- The 34-char `DataLogConsumer` gauge-name limit (documented in CLAUDE.md for
  `output_channels.txt`) also applies to `tcu_controller.txt` struct-field comments feeding
  `LiveData.yaml` - `tcu_upshiftMargin`'s first attempt (a long single-line comment) broke
  `LiveDataProcessor` codegen for every board. Fixed with the `\n`-split short-name/long-desc
  convention.
- Confirmed via `gauge_declarations.ini:330` that `tcuDesiredGear`/`desiredGearGauge` (the
  Lua script's gauge #1, "intended gear") already existed - zero new plumbing needed for it.

Validation:
- `unit_tests/test.sh`: 1314/1315 pass. The one failure, `ClosedLoopFuel.StateBasedRegionMapping`,
  is unrelated (STFT region-for-state mapping, no file this session touched) and reproduces in
  isolation - pre-existing on this branch.
- All 7 tests in the `tcu` suite pass (2 pre-existing + 5 new).
- `firmware/gen_config_board.sh config/boards/proteus proteus_f7` regenerated cleanly; manually
  inspected the generated `rusefi_proteus_f7.ini` to confirm the trimmed `tcuControls` dialog
  matches spec exactly and all 9 deleted dialogs are absent, and that the 5 new gauge fields
  (`entry =`, `indicator =`, `graphLine =`) are present.

Open follow-ups:
- TCC PWM Solenoid fields stay visible per user's explicit "keep the tcc solenoids" (plural)
  instruction, but `Generic4TransmissionController` (the hardcoded default) never drives that
  pin - only `Gm4l6xTransmissionController` does. So those 3 fields are currently assignable
  but inert plumbing under the new default. Flagged to the user before implementation; no
  change requested.
- 5-10 gear support and the WOT-curve shift logic from the reference Lua script are explicitly
  out of scope - `AutomaticGearController`/`Generic4TransmissionController` remain hardcoded
  to a 4-gear, non-WOT-aware state machine.
- No hardware/TS-console validation this session (both `ts_show_tcu` and `show_tcu_gauges`
  alpha flags stay `false` by default, unchanged) - only codegen + unit tests were exercised.
- `ClosedLoopFuel.StateBasedRegionMapping` remains open and unrelated to this work.

## 2026-08-01 - TCU follow-up: fix invisible gauges, make idle-shift-to-first configurable + SM-driven

Two corrections to the same-day TCU rework above, both reported by the user after reviewing
the generated console.

**1. New Transmission gauges were invisible in TunerStudio.** All 5 new gauges added earlier
today were gated `@@if_show_tcu_gauges`, matching `desiredGearGauge`/`currentGearGauge`/
`ISSGauge`/`tcRatioGauge` (the "most similar" siblings, per that session's own flagged
judgment call). Checked every `board.mk`/`prepend*.txt` in the repo: `show_tcu_gauges`
defaults `false` (`rusefi_config.txt:2941`) and **no board anywhere overrides it to `true`** -
so that gate is dead code, permanently hiding anything behind it. Fix: dropped the
`@@if_show_tcu_gauges` suffix from the 5 new gauge lines in `gauge_declarations.ini` to match
the *majority* of the category's gauges (`detectedGearGauge`, `speedToRpmRatioGauge`,
`shiftTimeGauge`, `idealEngineTorqueGauge`, `pressureControlGauge`, `torqueConverterGauge`,
all ungated). Verified by regenerating `protorico-econoline`'s `.ini` (`EFI_TCU=TRUE`,
`ts_show_tcu=true`) - all 5 now appear under `gaugeCategory = Transmission`. Did not touch the
4 pre-existing gauges still gated behind the same dead flag (out of scope, pre-existed before
today's TCU rework).

**2. Idle-shift-to-first made configurable and delegated to the Engine State Machine.** The
feature was unconditionally on with hardcoded RPM/TPS/VSS thresholds duplicated in
`gc_auto.cpp`. Per user request:
- Added `bit tcuIdleShiftToFirstEnabled` and `uint8_t tcuIdleShiftToFirstMaxVss` (km/h, 0 =
  ignore speed entirely) to `rusefi_config.txt`, exposed in the `shiftSettingsPanel` of
  Transmission Settings as "Shift to First if Idle" / "Idle Shift Max Speed (km/h, 0 = ignore
  speed)", the latter greyed out unless the former is checked.
- Then, per a second follow-up request, replaced the hardcoded RPM/TPS idle check entirely
  with a query to the Engine State Machine's `engineSmIsIdle` bit
  (`engine->module<EngineStateMachine>().unmock().engineSmIsIdle`) - the exact same
  idiom `MisfireController::onEnginePhase` already uses for its own idle-only gating
  (`misfire_detection.cpp:194`). `gc_auto.cpp` no longer computes idle itself at all; it just
  asks the state machine and applies the optional VSS gate on top.
- Default `tcuIdleShiftToFirstEnabled = true` set in `setDefaultBaseEngine()` (not
  `defaultsOrFixOnBurn()` - a plain bit has no "unset" sentinel distinguishable from a
  deliberate `false`, so the migration-safe `== 0` guard pattern doesn't apply here; this is a
  brand-new field on an unreleased feature, so a real factory default in the reset-to-defaults
  path is correct and won't fight a user's later choice to disable it on burn).
  `tcuIdleShiftToFirstMaxVss` needs no explicit default - 0 (ignore speed) is naturally both
  the zero-init value and the safe/neutral choice per `docs/calibration-compatibility.md`.

Gotcha hit: `tcuEnabled`/`gearControllerMode`/`totalGearsCount` (accessed via
`engineConfiguration->`) and `tcu_shiftTime`/`tcu_shiftSpeed12`/my new idle fields (accessed
via `config->`) are declared in the *same* `rusefi_config.txt` file but land in two different
generated C++ structs - `engine_configuration_s` (lines 1375-6824 of the generated header) vs.
a second struct nested in `persistent_config_s` alongside it (~lines 6982-8821). Using the
wrong pointer for a new field compiles fine for `.ini`/gauge purposes (TS's flat field
namespace doesn't care) but fails C++ compilation with a "no member named X" error that
doesn't obviously point at the real cause. Fixed by matching the pointer convention of the
nearest pre-existing sibling field in the `.txt` file (`config->tcu_shiftTime` right next to
my insertion), not by assuming `engineConfiguration`/`config` are interchangeable aliases.

Validation: `unit_tests/test.sh` - 1316/1317 pass (all 9 `tcu` tests, including 2 new:
`testIdleShiftToFirstDisabled`, `testIdleShiftToFirstVssThreshold`; `testIdleShiftToFirst`
rewritten to drive `engineSmIsIdle` directly instead of mocking RPM/TPS/VSS). The one failure
is the same pre-existing unrelated `ClosedLoopFuel.StateBasedRegionMapping`. One test-design
pitfall worth noting for future TCU tests: forcing `desiredGear` down via the idle-shift path
and then leaving VSS at a "driving" value in the same update() call lets the *ordinary*
VSS/TPS shift-table logic in the same `update()` immediately shift back up before the test can
observe the idle-forced gear - discovered via `testIdleShiftToFirst` failing until the idle
step's VSS mock was also dropped to a realistic (stopped) value; the dedicated VSS-threshold
test sidesteps this by using a TPS value (11%, `tcu_shiftTpsBins[0]`) whose ordinary shift
thresholds sit safely below every VSS value the test uses.

Open follow-ups: same as the original TCU entry above (no >4 gear/WOT support, no hardware/TS
validation, `ClosedLoopFuel.StateBasedRegionMapping` still open).

## 2026-08-01 - alphax-s550-pnp: A/C Pressure reads wrong on power-on until pin is re-selected

**Symptom** (user-reported, real hardware): on the S550 PNP board, the A/C Pressure sensor
(PF9 mux=1, `EFI_ADC_43` which is numerically channel 44 - this board's `adc_channel_e` enum
has a permanent +1 offset from `EFI_ADC_NONE=0`) reads a plausible-looking but wrong voltage
(~0.8-0.9V) from power-on, and only reads correctly (~1.45-1.5V, matching real A/C system
pressure) after the user manually re-selects the input in TunerStudio away from A/C Pressure
and back (any burn touching that specific field - "any burn" alone, e.g. an unrelated fan
setting, does not fix it).

**Investigation path** (several disproven theories worth recording so they aren't re-tried):
- Not a mux-GPIO-polarity/boot-race issue (this board sets `ADC_MUX_PIN_INVERTED=1`) - a patch
  forcing the mux to its documented primary state at `portInitAdc()` was written, then reverted
  once hardware data contradicted it (the bad reading is stable/rock-steady for 10+ seconds,
  not a one-cycle race that a low-pass filter would self-correct).
- Not `AdcSubscription`'s per-channel `VoltsPerAdcVolt` divider-coefficient caching - the
  `sensorinfo` console command showed `divider=2.00` identically in both the wrong and fixed
  states.
- Not a silent ADC3 conversion failure - `ECU: Slow ADC errors`/`overruns` sat at flat 0 in the
  user's `.msl` log throughout the bad-reading window.
- Not a slow analog RC settling time on the sensor's own input filter - proven wrong by the
  user: leaving the (still-wrong) reading untouched for 20-30+ seconds does not make it
  converge; it only ever fixes instantly, exactly when the A/C-Pressure channel field itself is
  reconfigured+burned.
- A real, reproducible-on-the-bench-with-nothing-connected anomaly was key: added a `muxdiag`
  console command (`stm32_adc_v2.cpp`) dumping the mux GPIO logic level, PF9's *actual* GPIO
  mode/pull straight from the hardware registers (`debugBrainPin(..., Gpio::F9)`), and raw
  ADC counts for both mux positions of all 16 standard channels - all in one shot, so a "stuck
  wrong" state and a "just fixed" state can be compared without needing to catch a boot-time
  transient (the bad reading persists indefinitely once present). This showed PF9 sitting at
  **`Input Pull-down`** (not `Mode Analog`) whenever A/C Pressure read wrong, flipping to
  `Mode Analog` the instant the fix was applied - with nothing connected on the bench, ruling
  out any real sensor/hardware explanation entirely.

**Root cause**: `adcIsMuxedInput()`/`adcMuxedGetParent()` (`firmware/hw_layer/ports/stm32/
stm32_adc.cpp`) only recognize `adc_channel_e` values 40-47 as "muxed" (an alias of a lower
root channel, needing pin-ownership-bypass handling) when `ADC3_SLOW_CHANNEL_COUNT` is defined
- and grepping the whole repo confirms **no board anywhere defines that macro**; it's dead
code. This board (alphax-s550-pnp) populates channels 40-47 through a *different*, unguarded
mechanism instead (`board.mk`'s `-DEFI_SLOW_ADC=ADCD3`, aliased in `stm32_adc_v2.cpp::
readSlowAnalogInputs()`'s `EFI_SLOW_ADC == ADCD3` block), which those two lookup functions
didn't know about. Consequence: A/C Pressure's channel (44) was never recognized as muxed, so
`getAdcChannelBrainPin()` couldn't even resolve it to a physical pin (its lookup table only
lists root channels; the 44->36 alias translation is exactly what `adcMuxedGetParent()` is
supposed to provide and didn't), and `AdcSubscription::SubscribeSensor()` therefore never
touched PF9's GPIO pad mode at all on a normal boot - it just sat at this MCU's power-on
default (`stm32f7/board.h`: `EFI_DR_DEFAULT = PIN_PUPDR_PULLDOWN`, applied to *every* pin),
which is noisy enough (digital input circuitry active) to overpower the board's 680k pulldown
and read a plausible-but-wrong voltage. TPS/PPS live on the exact same shared-pin structure
(e.g. TPSA/PPSB are also in the unrecognized 40-47 range) but were never affected, because
their *root* counterpart (TPSB/PPSA, `setTPS1Inputs`/`setPPSInputs`) is *also* permanently,
independently subscribed and correctly configures the shared physical pin regardless - A/C
Pressure's root-channel sibling (Fuel Rail Pressure) is not independently active in this tune,
so nothing else ever came along to fix the pin, until the user's manual pin-swap-and-back
incidentally did (by momentarily subscribing the root/Fuel-Rail-Pressure channel, which *is*
correctly resolved, setting PF9 to analog and leaving it there).

**Fix**: extended `adcIsMuxedInput()`/`adcMuxedGetParent()` with an `#elif defined(EFI_SLOW_ADC)
&& (EFI_SLOW_ADC == ADCD3)` branch recognizing 40-47 unconditionally in that case. Scoped
correctly - confirmed via repo-wide grep that alphax-s550-pnp is the only board defining
`EFI_SLOW_ADC=ADCD3`, so this is a no-op for every other board.

**Validation**: full firmware build for alphax-s550 (F7) succeeds. User confirmed on real
hardware, via the new `muxdiag` command: PF9 now reads `Mode Analog` on a fresh reboot with
no pin-swap workaround needed at all (previously always `Input Pull-down` until manually
fixed). Did not run `unit_tests/test.sh` - this fix lives entirely in `EFI_PROD_CODE`-only,
STM32-hardware-specific files with no host-side test coverage.

**Kept**: the `muxdiag` console command (`stm32_adc_v2.cpp`, gated `#if ADC1_SLOW_MUXED`) -
cheap, generically useful for this whole shared-mux board family for any future "which mux
channel is actually configured how" debugging, not just this one bug.

Open follow-ups: none identified for this specific bug. Worth a broader repo grep next time
someone touches `ADC3_SLOW_CHANNEL_COUNT` to confirm whether that whole code path (also
unreferenced by any board) should be removed outright rather than left as effectively-dead
code that nearly caused a second maintenance-time mix-up.

## 2026-08-01 - TCU follow-up 2: expose commanded gear, wire "Current Gear" to real gear detection

User reported live-testing: at conditions where the TCU should be commanding 1st gear, the
"TCU: Solenoid 1 On" gauge read 0 despite the configured solenoid table requiring solenoid 1
on / solenoid 2 off for gear 1. Asked for a "commanded gear" gauge to cross-check against.

Investigation found the diagnostic tool they needed already existed but was invisible:
`desiredGearGauge`/`currentGearGauge`/`ISSGauge`/`tcRatioGauge` in `gauge_declarations.ini`
were still gated `@@if_show_tcu_gauges` - the same dead flag fixed for the 5 gauges added
earlier today, just not touched then because that fix was scoped to only the new gauges.
`tcuDesiredGear` ("Desired Gear") is exactly "the gear passed to the solenoid lookup table" -
`GearControllerBase::update()` calls `transmissionController->update(getDesiredGear())`
directly, and that same value indexes `tcuSolenoidTable` in `SimpleTransmissionController::
update()`. Un-gated all 4 remaining `@@if_show_tcu_gauges`-gated Transmission-category gauges
so the user can now see it. Re-verified the solenoid capture formula added earlier
(`config->tcuSolenoidTable[i][static_cast<int>(gear) + 1]`) is byte-for-byte identical to the
pre-existing formula that drives the physical solenoid pins (`simple_tcu.cpp`), so a gauge/pin
mismatch given the same commanded gear should not be possible - the live-test discrepancy is
most likely explained by the commanded gear not actually being 1 at that moment, or testing
against a pre-today firmware build; newly-visible "Desired Gear" should confirm which.

Follow-up user request, addressed in the same session: **redefine gauge semantics.**
"Desired Gear" should stay as-is (commanded/solenoid-lookup value). "Current Gear" should
instead be *derived from the VSS/RPM ratio* (same concept as "Detected Gear" - user explicitly
equated the two), not a mirror of the commanded value.

Found `firmware/controllers/modules/gear_detector/gear_detector.cpp` (`GearDetector`, compiled
into every board unconditionally via `controllers/modules/modules.mk`) already does exactly
this: computes driveshaft RPM from `SensorType::VehicleSpeed` + `driveWheelRevPerKm` +
`finalGearRatio`, compares against `InputShaftSpeed` (or `Rpm` if no ISS), matches the ratio
against a per-gear `gearRatio[]` table, and publishes the result as `SensorType::DetectedGear`.
This module was previously dormant/disconnected from the TCU work: `gearRatio[]` (the
threshold table it needs) was declared in `rusefi_config.txt` but had **zero TS exposure
anywhere** - so it was permanently all-zeros, which would trigger `GearDetector::
initGearDetector()`'s `criticalError("Expecting positive gear ratio for #%d", ...)` (halts the
engine) for anyone who happened to have `totalGearsCount != 0`. It stayed silently safe only
because nothing ever set `totalGearsCount` either.

Also found (before making any change) that `totalGearsCount` and a full 10-entry `gearRatio1..
10` table **already exist** in a pre-existing `gearDetection` panel inside the "Speed sensor"
dialog (`tunerstudio.template.ini:4478`, part of the always-visible `speedSensor` dialog) -
this morning's earlier TCU pass had duplicated `totalGearsCount` into a new "# of Gears" field
inside Transmission Settings without knowing this, and additionally clamped its `lo,hi` to
`4,4` in `rusefi_config.txt` - which would have silently broken `gearDetection`'s existing
5th-10th-gear fields (and any non-TCU/manual-transmission use of `GearDetector`) for every
board, since the field is shared, not TCU-exclusive.

Asked the user whether `GearDetector` should auto-arm with placeholder ratios or stay
opt-in-only; they chose opt-in (matches existing default, zero risk) and specified the gear
ratio table belongs solely in the Speed Sensor dialog, not duplicated into Transmission
Settings. Implemented:
- `rusefi_config.txt`: reverted `totalGearsCount`'s `lo,hi` from `4,4` back to `1,
  @@TCU_GEAR_COUNT@@` (its original range); updated its comment to explain the field is shared
  between `GearDetector` (any count) and TCU Automatic mode (hardcoded to 4 gears regardless of
  this count) and that it's configured in the Speed Sensor dialog.
- `tunerstudio.template.ini`: removed the duplicate "# of Gears" field + its comment block from
  `transmissionPanel`, replaced with a single `field = "!..."` pointer note directing to the
  Speed Sensor dialog.
- `tcu.cpp`: `TransmissionControllerBase::postState()` no longer sets `tcuCurrentGear =
  getCurrentGear()` (the last-commanded-gear tracker, which is still needed internally
  unchanged for `Generic4TransmissionController`'s shift-start detection via `shiftingFrom`/
  `isShifting`). It now reads `Sensor::get(SensorType::DetectedGear)` and only updates
  `tcuCurrentGear` when that sensor is valid, otherwise retaining its last value. Zero risk of
  the `criticalError` path being hit by this change alone, since `totalGearsCount` still
  defaults to 0 and nothing in this pass changes that default.
- `gauge_declarations.ini`: un-gated `desiredGearGauge`/`currentGearGauge`/`ISSGauge`/
  `tcRatioGauge` (dropped `@@if_show_tcu_gauges`), matching the fix already applied to the 5
  gauges added earlier today and the majority-ungated convention in that category.

Validation: `unit_tests/test.sh` - 1316/1317 pass, same pre-existing unrelated
`ClosedLoopFuel.StateBasedRegionMapping` failure, no `tcu` or `gear_detector` test regressions
(neither test file references `tcuCurrentGear`/`getCurrentGear()` directly, so none needed
updating). Regenerated `protorico-econoline`'s `.ini` and confirmed: Transmission Settings no
longer duplicates the gear-count field, the pre-existing Speed Sensor dialog's 10-gear table is
back to its full un-clamped range, and all 9 Transmission-category gauges (4 re-enabled + 5
from earlier today) are present.

Open follow-ups:
- `tcuCurrentGear` ("Current Gear") will read invalid/stale until a user actually configures
  "Forward gear count" and the per-gear ratio table in the Speed Sensor dialog - this is
  unfinished setup work inherent to `GearDetector` itself, not something this pass changes.
- Live-hardware confirmation of the original solenoid-gauge-vs-commanded-gear discrepancy is
  still outstanding - depends on the user re-testing with today's build and the now-visible
  Desired Gear gauge.
- Same open items as both earlier TCU entries today (no >4-gear/WOT support in Automatic mode,
  `ClosedLoopFuel.StateBasedRegionMapping` still open, no hardware validation this session).

## 2026-08-01 - TCU follow-up 3: root-caused "Desired Gear stuck at 0" via user-supplied .msl log

User captured a short log (`whygear0.msl`, protorico-econoline, engine idling, RPM ~886, VSS
0) showing every TCU channel pinned at 0 the whole time: Desired Gear, Current Gear, both
solenoid-on flags, EPC/TC Duty. Engine SM channels (`Engine SM: enabled`/`Engine SM: Idle` = 1)
confirmed the rest of the ECU was running fine, isolating the problem to the TCU chain
specifically. Parsed the .msl header (tab-separated, columns at fixed offsets after 5 header
lines) with a small Python script to pull just the relevant columns across all 113 rows rather
than reading the ~30k-token raw file.

Root cause, established by elimination against the actual code path (not the log alone):
`AutomaticGearController::update()` unconditionally promotes `NEUTRAL -> GEAR_1` the instant
it's called (`if (getDesiredGear() == NEUTRAL) setDesiredGear(GEAR_1);`) - so `desiredGear`
staying 0 across 113 rows / 1.2s only makes sense if that `update()` call never ran at all,
which happens if either `tcuEnabled` is false or `gearControllerMode == None`
(`engine_controller.cpp:192`'s gate). User confirmed `tcuEnabled` was on, narrowing it to
`gearControllerMode`. `defaultsOrFixOnBurn()`'s TCU block (added earlier today) only forced
the mode when it was still exactly `None`, specifically to avoid disturbing
`configureTcu4R70W()`'s deliberate `Generic` selection - but that same guard meant a tune
already sitting on `Generic` or `ButtonShift` from *before* today's UI changes (dropdown +
range-selector dialog both removed) would never get corrected, and had no way to be fixed
through TS anymore either (nothing left in the UI reads or writes those fields). User confirmed
this board previously had "Generic gear controller" selected in an earlier firmware build -
exact match.

User's call on the tradeoff (asked because it also affects `configureTcu4R70W()`, a
hypothetical real-board preset unrelated to this specific bug): don't preserve compatibility
with anything, this is a ground-up reimplementation. Changed `defaultsOrFixOnBurn()`'s TCU
block from "set only if still `None`" to **unconditional**: whenever `tcuEnabled`, force
`gearControllerMode = Automatic` and `transmissionControllerMode = Generic4` on every boot,
full stop, overriding whatever was previously persisted (including `configureTcu4R70W()`'s
explicit `Generic` choice, and including a mode a user leaves selected while `tcuEnabled` is on
- there is no longer any code path that can set anything else, so this simply keeps
re-asserting the only supported state every startup/burn).

Updated `test_tcu.cpp`'s `testDefaultModeDoesNotOverrideExplicitChoice` (whose whole premise -
"a tune with Generic set must not be touched" - is now the opposite of intended behavior) to
`testDefaultModeOverridesExplicitChoice`, asserting a `Generic`-mode tune gets forced to
Automatic/Generic4. Along the way discovered `EngineTestHelper` construction itself already
runs `defaultsOrFixOnBurn()` a second time post-`applyEngineType()` (simulating a real boot
sequence past the initial "reset to defaults" pass) - so `configureTcu4R70W()`'s live `Generic`
selection was *already* being overridden to `Automatic` by the time the test body started
running, before its own explicit `defaultsOrFixOnBurn()` call. Rewrote the test to explicitly
re-set `gearControllerMode = Generic` after construction (simulating a stale *persisted* value,
which is the actual real-world scenario) rather than relying on the preset's transient
in-memory value, which is both more realistic and avoids the ordering trap.

Validation: `unit_tests/test.sh` - 1316/1317 pass, same pre-existing unrelated
`ClosedLoopFuel.StateBasedRegionMapping` failure; the 9 `tcu` tests all pass including the
rewritten mode-override test. No other tests reference `gearControllerMode`/
`transmissionControllerMode` post-`defaultsOrFixOnBurn()` in a way this change could affect.

Open follow-ups:
- User has not yet re-flashed/re-tested on the actual protorico-econoline hardware to confirm
  the fix resolves the stuck-at-NEUTRAL symptom live - this session's validation is unit-test
  only.
- `GenericGearController`/`ButtonShiftController` and their supporting config fields
  (range-selector matrix, button pins) are now fully unreachable dead code paths in production
  (no UI can select or configure them, and the one remaining C++ path that could -
  `configureTcu4R70W()` - is itself now overridden every boot). Not removed this session; worth
  a follow-up cleanup pass given the user's "ground-up reimplementation" framing.
- Same other open items as the earlier TCU entries today (no >4-gear/WOT support in Automatic
  mode, `ClosedLoopFuel.StateBasedRegionMapping` still open).

## 2026-08-01 - protorico-econoline: enable CLT-from-CHT estimation

User asked to enable the CHT-to-CLT estimator (`cht_clt_estimator.h`/`.cpp`, page 6
`cltFromCht`) for protorico-econoline. This board is a Ford Modular V8 (firing order
`FO_1_3_7_2_6_5_4_8`, cylindersCount 8) - these engines ship with a head-mounted CHT sensor and
no separate coolant sensor, so the harness signal previously wired as `clt.adcChannel` (labeled
`CLT_OUT` on the connector, PC2 / `H144_IN_CLT`) is actually a CHT signal.

Two things were required, not just the runtime toggle:
1. `board.mk`: this board's `meta-info.env` pins `PROJECT_CPU=ARCH_STM32F4`, and
   `stm32f4ems/efifeatures.h` defaults `EFI_CHT_CLT_ESTIMATOR` to `FALSE` (only
   `stm32f7ems`/`stm32h7ems` default it `TRUE`) - without an explicit override the estimator
   code compiles out entirely (`cht_clt_estimator.cpp`'s `#else` stub). Added
   `-DEFI_CHT_CLT_ESTIMATOR=TRUE`. Caught this only after the user asked "don't we need to
   enable this in board.mk?" - initially assumed it was already compiled in because
   `board.mk` has F7/H7-conditional sections, without checking which `PROJECT_CPU` this board
   actually builds as.
2. `board_configuration.cpp`: repointed the PC2 ADC input from `engineConfiguration->clt` to
   `engineConfiguration->chtSensor` (nothing else on this board reads `chtSensor`, so it was
   previously wired to nothing), gave `chtSensor.config` a placeholder thermistor curve (reused
   the same generic NTC curve `engine_configuration.cpp` uses as the global `clt` default -
   `chtSensor` has no default curve set anywhere in the codebase, and an unconfigured
   `{0,0,0,...}` curve trips `validateThermistorConfig`'s ascending-order check and calls
   `firmwareError` at boot), and set `getCustomPage()->cltFromCht = true` in
   `protorico_econoline_boardDefaultConfiguration()` (runs after `resetExtraPages()`'s
   `customPageSetDefaults()`, which sets `cltFromCht = false`, so the override sticks; it's a
   soft default like the other ADC channel assignments in the same function, not forced via
   `ConfigOverrides`, so a user can still flip it back to a real CLT sensor in TS if the harness
   changes).

Validation: full firmware build via `bin/compile.sh config/boards/protorico-econoline/meta-info.env
-j12` - succeeds, flash 75.86% (571712/736KB), ram0 100% (pre-existing headroom pattern, not a
regression). Not flashed/tested on hardware this session.

**Follow-up same day**: user reported `build_gui.py`'s bundle build (`compile.sh -b ...`, which
additionally builds the OpenBLT bootloader region) failing with `LAUNCH_POWER_RAMP_CURVE_SIZE`/
`BURST_KNOCK_*_SIZE`/`WOT_ENRICHMENT_SIZE was not declared in this scope` plus a static_assert
failure, all in `page_6_generated.h`, compiling `board_configuration.o` under `-DEFI_BOOTLOADER`.
Root cause: `#include "custom_page.h"` (added above, for `getCustomPage()`) pulls in
`page_6_generated.h` directly; those curve-size macros are normally supplied by `engine.h`'s
unconditional module-header includes (`launch_power_ramp.h`, `burst_knock.h`,
`wot_enrichment.h`), but the bootloader's compile of `board_configuration.cpp` never includes
`engine.h` at all (`pch.h` only pulls in `engine_configuration.h`, and the bootloader
never runs engine config) - so in that one translation unit the macros were simply never
defined. Confirmed no other OpenBLT board (`f407-discovery`, `alphax-s197-v2`,
`alphax-gold`, `alphax-2chan`, etc.) includes `custom_page.h` from `board_configuration.cpp`,
which is why this class of break hadn't surfaced before. Fix: wrapped the `#include
"custom_page.h"` and the `getCustomPage()->cltFromCht = true;` call in `#ifndef
EFI_BOOTLOADER` (mirroring the guard `engine.h` itself uses around
`engine_modules_generated.h`) - both are dead code under `EFI_BOOTLOADER` anyway.
Re-validated with the actual failing command, `bin/compile.sh -b
config/boards/protorico-econoline/meta-info.env BUNDLE_SIMULATOR=false` (bootloader + firmware
+ bundle zip) - now completes successfully end to end.

Open follow-ups:
- `chtSensor.config` curve is an unvalidated placeholder (same caveat as the existing CLT and
  EOT estimator defaults) - needs bench/real-sensor calibration against the actual Ford CHT
  sensor part before trusting the reported temperature.
- Not yet confirmed on hardware that PC2 is actually reading a CHT-characteristic sensor and
  not something else; relies on user's stated wiring.
- Only validated the plain `bin/compile.sh ... -j12` build the first time, missing that
  `build_gui.py`/`compile.sh -b` exercises an additional bootloader compile of the same source
  file with a much smaller define/include set - worth remembering to test the `-b` bundle path
  (or at least grep for what an added `#include` pulls in) whenever touching a board's
  `board_configuration.cpp` on an OpenBLT-enabled board, not just the plain firmware build.

## 2026-08-01 - Oil Life Monitor: add set_oil_life console command

What was done:
- Added `OilLifeMonitor::setOilLifePercent(float percent)`: clamps to 0-100, converts back to
  a weighted-revolution count using the current `oilLifeRevsScaleMillions`, and immediately
  requests a flash flush (`requestFlush()`), same as `reset()`.
- Wired it up as a console command `set_oil_life` (`addConsoleActionF`, `initOilLifeMonitor()`
  in `oil_life_monitor.cpp`), e.g. `set_oil_life 62.5`, for manually correcting the tracked
  value after a settings loss. Not exposed in TunerStudio - it's a one-off correction, not a
  tune setting.
- Added the `EFI_UNIT_TEST`-stub counterpart (`void OilLifeMonitor::setOilLifePercent(float) {
  }`) alongside the other stubbed methods for builds with `EFI_OIL_LIFE_MONITOR` off.

Validation:
- `unit_tests/tests/test_oil_life_monitor.cpp`: new `OilLifeMonitor.SetOilLifePercent` test
  covers the direct call (75% round-trips exactly) and clamping at both ends (150 -> 100,
  -10 -> 0).

Open follow-ups: none identified.
