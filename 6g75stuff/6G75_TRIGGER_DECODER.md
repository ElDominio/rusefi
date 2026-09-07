# Mitsubishi 6G75 (36-2-1-1) Trigger Decoder Analysis & Implementation

## 1. Overview & Findings

This document details the reverse-engineered crank and camshaft trigger decoding architecture for the **Mitsubishi 6G75** (3.8L V6) engine, extracted from the official **Megasquirt-3 (MS3) Release 1.6.2 firmware** (`ms3.s19`).

### Key Findings:
* **Firmware Version Status:**
  * **MS3 1.4.0 (Baseline Source):** Neither `"6G75"` nor `"36-2-1-1"` is mentioned anywhere in the 1.4.0 tree. The only supported Mitsubishi engines are the 6G72 and 4G63.
  * **MS3 1.6.2:** Support is natively present under spark mode index **61** (`0x3D`), labeled `"6G75"` in `ms3.ini`.
* **Crank Wheel Classification:**
  * The factory 6G75 crank trigger is a **36-2-1-1** pattern (also frequently referred to as 36-2-1).
  * It is based on a **36-slot wheel** ($10.0^\circ$ nominal tooth spacing), containing **29 physical teeth** and **3 distinct missing-tooth gaps** per crank revolution ($360^\circ$).
* **Sync Loss Code:**
  * Sync loss reason **100** is assigned specifically to 6G75 tooth/sync failure (`outpc.syncreason = 100`).
* **Implementation Source:**
  * Complete, working C code with a standalone test simulation has been created in [`ms3_ign_6g75.c`](file:///home/normanpaulino/Downloads/ms3-source-1.4.0/ms3/ms3_ign_6g75.c).

---

## 2. Physical Trigger Wheel Geometry

The 6G75 crankshaft trigger wheel has 36 equal angular slots of $10.0^\circ$ each. Four slots have missing teeth, divided into two single-missing teeth and one double-missing tooth:

```
0°                                                                  360°
|--- 10 Teeth ---|--Gap 1--|--- 10 Teeth ---|--Gap 2--|--- 9 Teeth ---|--Gap 3--|
|  10 x 10.0°    | 1 x 20° |  10 x 10.0°    | 1 x 20° |  9 x 10.0°    | 1 x 30° |
|  = 100.0°      | = 20.0° |  = 100.0°      | = 20.0° |  = 90.0°      | = 30.0° |
```

$$\text{Total Angular Span} = 100.0^\circ + 20.0^\circ + 100.0^\circ + 20.0^\circ + 90.0^\circ + 30.0^\circ = 360.0^\circ$$

### Tooth Angle Table (`deg_per_tooth` array in MS3, tenths of a degree):
* **Normal teeth:** `100` ($10.0^\circ$)
* **Gap 1 (index 10):** `200` ($20.0^\circ$ — 1 missing tooth)
* **Gap 2 (index 21):** `200` ($20.0^\circ$ — 1 missing tooth)
* **Gap 3 (index 31):** `300` ($30.0^\circ$ — 2 missing teeth)

**Total physical teeth per crank revolution:** $10 + 1 + 10 + 1 + 9 + 1 = 32\text{ teeth/events}$.

---

## 3. Crank Position Decoding: Using vs. Not Using Camshaft

The decoder supports two primary operating configurations:

### Option A: Crank-Only Mode (Without Camshaft Sensor)
* **Ignition / Injection Type:** Wasted spark ignition (3 coil pairs) and semi-sequential or batch fuel injection.
* **Cycle Degrees:** `cycle_deg = 3600` ($360.0^\circ$ — one crank revolution).
* **Teeth per Cycle:** `no_teeth = 32`.
* **Triggers per Cycle:** `no_triggers = 3` (events spaced $120.0^\circ$ apart).
* **Trigger Teeth:** Teeth `12`, `23`, and `1`.
* **Base Angle:** `trig_angs = -350 + tmp_offset` ($-35.0^\circ$ BTDC).
* **Synchronization:** Does not require the camshaft. It syncs directly off the $30^\circ$ double-missing tooth gap on the crankshaft wheel.

### Option B: Crank + Camshaft Mode (With Camshaft Sensor)
* **Ignition / Injection Type:** Coil-on-Plug (COP) individual coil ignition and fully sequential fuel injection.
* **Cycle Degrees:** `cycle_deg = 7200` ($720.0^\circ$ — two crank revolutions / complete 4-stroke cycle).
* **Teeth per Cycle:** `no_teeth = 64` ($32 \times 2$).
* **Triggers per Cycle:** `no_triggers = 6` (one per cylinder firing event at $120.0^\circ$ crank spacing).
* **Trigger Teeth:**
  * Revolution 1: Teeth `12`, `23`, `1`.
  * Revolution 2: Teeth `44`, `55`, `33`.
* **Cam Sensor Operation:**
  * The camshaft target wheel generates an **asymmetric pulse count** between crank revolutions:
    * **Revolution 1 (Phase 1):** 2 cam pulses (`trig2cnt == 2`).
    * **Revolution 2 (Phase 2):** 1 cam pulse (`trig2cnt == 1`).
  * When the crank decoder detects the double-missing tooth gap, it inspects `trig2cnt` accumulated over the preceding 10 teeth:
    * If `trig2cnt == 2`: Revolution 1 confirmed $\rightarrow$ `tooth_no = 1`.
    * If `trig2cnt == 1`: Revolution 2 confirmed $\rightarrow$ `tooth_no = 33`.
    * Any other value is treated as a phase error, resetting the synchronization state.

---

## 4. Synchronization State Machine & Mathematical Principles

### Step 1: Pre-Sync Filtering
Before attempting ratio checks, the engine must rotate through at least 3 valid tooth periods so that `tooth_diff_this`, `tooth_diff_last`, and `tooth_diff_last_1` are populated and non-zero:
```c
if ((!tooth_diff_this) || (!tooth_diff_last) || (!tooth_diff_last_1)) {
    return 0;
}
```

### Step 2: Gap Detection Formula
A gap transition is validated when a long tooth interval is immediately followed by a normal short tooth interval, ensuring the long interval was a genuine gap and not engine deceleration:
$$\text{tooth\_diff\_last} > 2 \times \text{tooth\_diff\_this} \quad \text{AND} \quad \text{tooth\_diff\_last\_1} < \text{tooth\_diff\_last}$$

In HCS12X assembly (`ms3.s19` at `$19CD`):
```assembly
ldd   $2ebb           ; tooth_diff_this (low word)
asld                  ; 2 * tooth_diff_this
std   $328e
ldd   $2eb9           ; tooth_diff_this (high word)
rolb
rola
std   $328c
movw  $397a, $280a    ; tooth_diff_last (low word)
movw  $3978, $2808    ; tooth_diff_last (high word)
ldx   #$328c
ldd   $2808
cpd   0, x            ; compare high words
lbcs  $1afd           ; if tooth_diff_last < 2 * tooth_diff_this -> exit
bhi   $1a00           ; if > -> gap candidate
ldd   $280a
cpd   $328e           ; compare low words
lbls  $1afd
; Now verify tooth_diff_last_1 < tooth_diff_last:
ldd   $2c1b
ldx   $2c19
ldy   #$3978
cpx   0, y
lbhi  $1afd
bcs   $1a18
```

### Step 3: Unambiguous Wheel Identification (`tooth_no == 10`)
Why does the decoder test `tooth_no == 10`?
Consider the distance (in physical tooth counts) between each gap:
* Between **Gap 1** ($20^\circ$) and **Gap 2** ($20^\circ$): **11 tooth events**.
* Between **Gap 2** ($20^\circ$) and **Gap 3** ($30^\circ$): **10 tooth events** (9 short teeth + 1 gap).
* Between **Gap 3** ($30^\circ$) and **Gap 1** ($20^\circ$): **11 tooth events**.

Because the distance between Gap 2 and Gap 3 is **strictly 10 teeth**, testing `tooth_no == 10` upon encountering the gap guarantees that the current gap is **Gap 3 (the 2-missing-teeth gap)**. If any other gap is encountered, `tooth_no` will be 11, causing the decoder to reset and maintain semi-sync until the true 10-tooth interval appears.

---

## 5. Reverse-Engineering Map from `ms3.s19` (v1.6.2)

| Component | Flash Address | Flash Bank / Section | Function / Context |
| :--- | :--- | :--- | :--- |
| **`spk_mode0` Enum** | — | `ms3.ini` Line 753 | Spark mode index 61 (`0x3D`) |
| **Wheel Setup** | `0x7EEB59` | Page `0xFB` (`textfb`) | `ign_wheel_init()`: sets `deg_per_tooth`, triggers, cylinder check |
| **ISR Dispatch** | `0x7D460A` | Page `0xF5` (`textf5`) | `cmpb #61; lbeq $1997` in `ISR_Ign_TimerIn_paged2` |
| **ISR Sync Logic** | `0x7D5997` | Page `0xF5` (`textf5`) | 6G75 gap recognition, semi-sync, cam phase check |
| **Steady-State Check** | `0x7D5AB7` | Page `0xF5` (`textf5`) | Rollover at tooth 32/64 and loss of sync (Reason 100) |
| **Cylinder Guard** | `0x7EEC30` | Page `0xFB` (`textfb`) | Verifies `num_cyl == 6`; sets `conf_err = 17` if not |

---

## 6. Porting to MS3 1.4.0

To add native 6G75 support to your MS3 1.4.0 tree:

1. **[`core.ini`](file:///home/normanpaulino/Downloads/ms3-source-1.4.0/ms3/core.ini#L684):**
   Replace the first `"INVALID"` in `spk_mode0` (slot 61) with `"6G75"`:
   ```ini
   "Ski doo PTEC", "Nissan QG15", "Mazda MZR", "6G75", "INVALID", "INVALID"
   ```
2. **[`ms3.h`](file:///home/normanpaulino/Downloads/ms3-source-1.4.0/ms3/ms3.h#L1865):**
   Add sync error definition:
   ```c
   100 = 6G75 tooth/sync fault
   ```
3. **[`ms3_ign_wheel.c`](file:///home/normanpaulino/Downloads/ms3-source-1.4.0/ms3/ms3_ign_wheel.c):**
   Include `ign_wheel_init_6g75()` inside `ign_wheel_init()` guarded by `else if (spkmode == 61)`.
4. **[`ms3_ign_in.c`](file:///home/normanpaulino/Downloads/ms3-source-1.4.0/ms3/ms3_ign_in.c):**
   In `ISR_Ign_TimerIn_paged2()`, add branch:
   ```c
   else if (spkmode == 61) {
       goto SPKMODE61;
   }
   ```
   and insert the `SPKMODE61:` block from [`ms3_ign_6g75.c`](file:///home/normanpaulino/Downloads/ms3-source-1.4.0/ms3/ms3_ign_6g75.c).

---

## 7. Standalone Verification Suite

The companion file [`ms3_ign_6g75.c`](file:///home/normanpaulino/Downloads/ms3-source-1.4.0/ms3/ms3_ign_6g75.c) includes a self-contained simulation test suite that tests both Crank-Only and Crank+Cam decoding with simulated tach intervals.

To compile and run:
```bash
gcc -Wall -Wextra -DSTANDALONE_6G75_TEST ms3_ign_6g75.c -o test_6g75
./test_6g75
```

### Execution Output:
```
==================================================================
   Mitsubishi 6G75 (36-2-1-1) Decoder Standalone Test Suite
==================================================================

=== SIMULATING 6G75 DECODER: CRANK ONLY (360 deg / Wasted Spark) ===
Wheel Init: cycle_deg=3600, no_teeth=32, no_triggers=3, last_tooth=32
>>> FULL RPM SYNC ACHIEVED at pulse 33! (tooth_no=1, trig2cnt=0, synch=0x01)
Simulation finished successfully: In full sync, outpc.syncreason=0

=== SIMULATING 6G75 DECODER: CRANK + CAM (720 deg / COP) ===
Wheel Init: cycle_deg=7200, no_teeth=64, no_triggers=6, last_tooth=64
>>> FULL RPM SYNC ACHIEVED at pulse 33! (tooth_no=1, trig2cnt=2, synch=0x01)
Simulation finished successfully: In full sync, outpc.syncreason=0
```
