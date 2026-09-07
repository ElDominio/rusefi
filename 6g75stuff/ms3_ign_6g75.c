/*
 * ms3_ign_6g75.c - Mitsubishi 6G75 (36-2-1-1) Trigger Decoder for Megasquirt-3
 *
 * Reverse-engineered from MS3 Release 1.6.2 firmware (ms3.s19).
 *
 * Pattern Overview:
 *   - Crank Wheel: 36-slot base, 10.0 deg spacing (360 deg per crank rev).
 *     Physical teeth: 29 teeth, 3 missing-tooth gaps:
 *       - 10 teeth (10 deg each)
 *       - 1 missing tooth gap (20 deg) -> deg_per_tooth[10] = 200 (20.0 deg)
 *       - 10 teeth (10 deg each)
 *       - 1 missing tooth gap (20 deg) -> deg_per_tooth[21] = 200 (20.0 deg)
 *       - 9 teeth (10 deg each)
 *       - 2 missing teeth gap (30 deg) -> deg_per_tooth[31] = 300 (30.0 deg)
 *     Total: 10 + 2 + 10 + 2 + 9 + 3 = 36 slots = 360 degrees.
 *
 *   - Camshaft Sensor:
 *     Asymmetric pattern over 720 degrees (engine cycle):
 *       - Revolution 1 (Crank Phase 1): 2 cam pulses
 *       - Revolution 2 (Crank Phase 2): 1 cam pulse
 *
 * Modes Supported:
 *   1. Crank-only (Wasted spark / semi-sequential fuel):
 *      - cycle_deg = 3600 (360.0 deg)
 *      - no_teeth = 32, no_triggers = 3
 *      - Syncs purely to the 30 deg double-missing tooth gap.
 *
 *   2. Crank + Cam (Sequential fuel / COP ignition):
 *      - cycle_deg = 7200 (720.0 deg)
 *      - no_teeth = 64, no_triggers = 6
 *      - Uses the 30 deg crank gap for initial semi-sync, then checks
 *        the number of cam pulses (trig2cnt) across the next 10 teeth
 *        to identify phase:
 *          * trig2cnt == 2 -> Phase 1 (tooth_no = 1)
 *          * trig2cnt == 1 -> Phase 2 (tooth_no = 33)
 */

#ifndef STANDALONE_6G75_TEST
#include "ms3.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Standalone simulation mock structures */
#define SPKMODE_6G75        61
#define NUM_TRIGS           16
#define MAXNUMTEETH         128

#define SYNC_SYNCED         0x01
#define SYNC_SKIP           0x02
#define SYNC_SEMI           0x08
#define FLAGBYTE5_CAM       0x04

typedef struct {
    unsigned char spk_mode0;
    unsigned char spk_mode3;
    unsigned char spk_config;
    int adv_offset;
} ram4_t;

typedef struct {
    unsigned char syncreason;
    unsigned char status1;
} outpc_t;

ram4_t ram4;
outpc_t outpc;

unsigned char spkmode = SPKMODE_6G75;
unsigned int cycle_deg;
unsigned char flagbyte5;
unsigned char no_teeth;
unsigned char no_triggers;
unsigned char last_tooth;
unsigned int deg_per_tooth[MAXNUMTEETH];
unsigned int smallest_tooth_crk;
unsigned int smallest_tooth_cam;
unsigned char trigger_teeth[NUM_TRIGS];
int trig_angs[NUM_TRIGS];
unsigned char num_cyl = 6;
unsigned char conf_err;
int tmp_offset = 0;

unsigned char synch = 0;
unsigned char tooth_no = 0;
unsigned char trig2cnt = 0;
uint32_t tooth_diff_this = 0;
uint32_t tooth_diff_last = 0;
uint32_t tooth_diff_last_1 = 0;

static void loss_of_sync(void) {
    synch &= ~(SYNC_SYNCED | SYNC_SEMI);
    tooth_no = 0;
    trig2cnt = 0;
}
#endif

/*
 * ign_wheel_init_6g75:
 * Initializes trigger wheel parameters, tooth angle arrays, and trigger positions.
 * Reverse-engineered from address 0x7EEB59 in Flash page FB (textfb) of ms3.s19.
 */
void ign_wheel_init_6g75(void)
{
    unsigned int i;

    /*
     * Determine operating mode:
     * Check COP mode (ram4.spk_mode3 & 0xC0 == 0x80) or Cam enabled (ram4.spk_config & 0x02)
     */
    if (((ram4.spk_mode3 & 0xC0) == 0x80) || (ram4.spk_config & 0x02)) {
        /* 720.0 degree cycle (Crank + Cam: Sequential / COP) */
        cycle_deg = 7200;
        flagbyte5 |= FLAGBYTE5_CAM;
        no_teeth = 64;           /* 32 physical teeth per crank rev * 2 revs */
        no_triggers = 6;         /* 6 spark/injection trigger events per cycle */
    } else {
        /* 360.0 degree cycle (Crank only: Wasted Spark / Semi-sequential) */
        cycle_deg = 3600;
        no_teeth = 32;           /* 32 physical teeth per single crank revolution */
        no_triggers = 3;         /* 3 trigger events per crank revolution */
    }

    /* Initialize all base tooth angles to 10.0 degrees (100 tenths of a degree) */
    for (i = 0; i < no_teeth; i++) {
        deg_per_tooth[i] = 100;
    }

    /*
     * 1st Crank Revolution (0 - 360 degrees):
     * Physical tooth count per revolution = 32:
     *   Teeth 0..9  : 10 teeth @ 10.0 deg = 100.0 deg
     *   Tooth 10    : 1st gap (1 missing tooth) = 20.0 deg
     *   Teeth 11..20: 10 teeth @ 10.0 deg = 100.0 deg
     *   Tooth 21    : 2nd gap (1 missing tooth) = 20.0 deg
     *   Teeth 22..30:  9 teeth @ 10.0 deg =  90.0 deg
     *   Tooth 31    : 3rd gap (2 missing teeth) = 30.0 deg
     *   Total = 100 + 20 + 100 + 20 + 90 + 30 = 360.0 deg.
     */
    deg_per_tooth[10] = 200;     /* 1 missing tooth gap: 20.0 deg */
    deg_per_tooth[21] = 200;     /* 1 missing tooth gap: 20.0 deg */
    deg_per_tooth[31] = 300;     /* 2 missing teeth gap: 30.0 deg */

    smallest_tooth_crk = 100;    /* Smallest crank tooth period is 10.0 deg */
    smallest_tooth_cam = 0;

    /*
     * Trigger tooth assignments for 1st crank revolution:
     * Teeth 12, 23, and 1.
     * Base trigger angle offset is -35.0 degrees (-350 tenths of a deg BTDC).
     */
    trigger_teeth[0] = 12;
    trigger_teeth[1] = 23;
    trigger_teeth[2] = 1;

    trig_angs[0] = -350 + tmp_offset;
    trig_angs[1] = -350 + tmp_offset;
    trig_angs[2] = -350 + tmp_offset;

    /*
     * 2nd Crank Revolution (360 - 720 degrees, if 720 deg / COP mode):
     * Repeat identical missing tooth gap offsets shifted by 32 teeth.
     */
    if (no_teeth == 64) {
        deg_per_tooth[42] = 200; /* Tooth 10 + 32 = 42 */
        deg_per_tooth[53] = 200; /* Tooth 21 + 32 = 53 */
        deg_per_tooth[63] = 300; /* Tooth 31 + 32 = 63 */

        trigger_teeth[3] = 33;   /* Tooth 1 + 32  = 33 */
        trigger_teeth[4] = 44;   /* Tooth 12 + 32 = 44 */
        trigger_teeth[5] = 55;   /* Tooth 23 + 32 = 55 */

        trig_angs[3] = -350 + tmp_offset;
        trig_angs[4] = -350 + tmp_offset;
        trig_angs[5] = -350 + tmp_offset;
    }

    last_tooth = no_teeth;

    /* Engine validation: 6G75 is strictly a 6-cylinder engine */
    if (num_cyl != 6) {
        conf_err = 17;           /* Configuration error: requires 6 cylinders */
    }
}

/*
 * ISR_Ign_TimerIn_6g75:
 * Tach input interrupt handler for 6G75.
 * Reverse-engineered from address 0x7D5997 in Flash page F5 (textf5) of ms3.s19.
 *
 * Parameters:
 *   edge  - Primary tach input edge (crank)
 *   edge2 - Secondary tach input edge (cam, increments trig2cnt)
 *
 * Returns:
 *   1 on successful tooth processing, 0 when unsynced/seeking sync.
 */
unsigned char ISR_Ign_TimerIn_6g75(unsigned char edge, unsigned char edge2)
{
    (void)edge;

    /* Handle secondary (cam) pulse event: increment cam trigger counter */
    if (edge2) {
        trig2cnt++;
    }

    /* =========================================================================
     * STAGE 1: NOT FULLY SYNCED - SEEKING INITIAL SYNC
     * ========================================================================= */
    if (!(synch & SYNC_SYNCED)) {
        /* Must have at least 3 valid non-zero intervals before computing ratios */
        if ((!tooth_diff_this) || (!tooth_diff_last) || (!tooth_diff_last_1)) {
            return 0;
        }

        /* Sub-stage 1A: Hunting for initial reference (double-missing tooth gap) */
        if (!(synch & SYNC_SEMI)) {
            /*
             * Look for the large 30 deg gap (3x normal tooth time):
             * Check: tooth_diff_last > (2 * tooth_diff_this)
             * Also verify previous interval was not larger: tooth_diff_last_1 <= tooth_diff_last
             */
            if ((tooth_diff_last > (tooth_diff_this << 1)) &&
                (tooth_diff_last_1 <= tooth_diff_last)) {
                tooth_no = 0;
                trig2cnt = 0;
                synch |= SYNC_SEMI;  /* Started semi-sync sequence */
            }
            return 0;
        }

        /* Sub-stage 1B: Semi-synced, counting teeth to verify and lock sync */
        tooth_no++;

        /*
         * Check for next gap signature:
         * tooth_diff_last > 2 * tooth_diff_this and acceleration check
         */
        if ((tooth_diff_last > (tooth_diff_this << 1)) &&
            (tooth_diff_last_1 <= tooth_diff_last)) {

            /*
             * Exactly 10 teeth must exist between the 30 deg gap and the first 20 deg gap!
             * If tooth_no == 10, we have verified the 36-2-1-1 pattern geometry!
             */
            if (tooth_no == 10) {
                if (cycle_deg == 7200) {
                    /*
                     * 720 degree mode (Crank + Cam):
                     * Inspect cam pulses recorded during the first 10 teeth:
                     *   trig2cnt == 1 -> Revolution 2 (Phase 2, tooth_no = 33)
                     *   trig2cnt == 2 -> Revolution 1 (Phase 1, tooth_no = 1)
                     */
                    if (trig2cnt == 1) {
                        tooth_no = 33;
                    } else if (trig2cnt == 2) {
                        tooth_no = 1;
                    } else {
                        /* Inconsistent cam pulses: reset and retry */
                        tooth_no = 0;
                        trig2cnt = 0;
                        return 0;
                    }
                } else {
                    /* Crank-only mode: lock directly into 360 deg cycle at tooth 1 */
                    tooth_no = 1;
                }

                /* Transition from semi-synced to FULL RPM SYNC */
                synch &= ~SYNC_SEMI;
                synch |= SYNC_SYNCED;
            } else {
                /* Tooth count did not match expected pattern: reset semi-sync */
                tooth_no = 0;
                trig2cnt = 0;
                return 0;
            }
        }
        return 0;
    }

    /* =========================================================================
     * STAGE 2: FULL RPM SYNC ACTIVE - RUNTIME POSITION & LOSS-OF-SYNC CHECKS
     * ========================================================================= */
    /*
     * At the end of each revolution (tooth 32 or tooth 64):
     * The double missing tooth gap occurs here.
     * Verify the timing ratio: if tooth_diff_this > 2 * tooth_diff_last unexpectedly,
     * signal loss of sync (Reason 100).
     */
    if ((tooth_no == 32) || (tooth_no == 64)) {
        if (tooth_diff_this > (tooth_diff_last << 1)) {
            outpc.syncreason = 100;  /* MS3 sync loss code 100: 6G75 tooth error */
            loss_of_sync();
            return 0;
        }

        /* 720-degree cycle roll-over */
        if (tooth_no == 64) {
            tooth_no = 0;
        }
    }

    return 1;
}

#ifdef STANDALONE_6G75_TEST
/* =========================================================================
 * STANDALONE TEST DRIVER & SIMULATION
 * ========================================================================= */

static void simulate_engine(bool use_cam, int total_cycles)
{
    printf("\n=== SIMULATING 6G75 DECODER: %s ===\n",
           use_cam ? "CRANK + CAM (720 deg / COP)" : "CRANK ONLY (360 deg / Wasted Spark)");

    /* Reset state */
    synch = 0;
    tooth_no = 0;
    trig2cnt = 0;
    tooth_diff_this = 0;
    tooth_diff_last = 0;
    tooth_diff_last_1 = 0;
    outpc.syncreason = 0;

    if (use_cam) {
        ram4.spk_mode3 = 0x80;   /* COP mode */
        ram4.spk_config = 0x02;  /* Cam enabled */
    } else {
        ram4.spk_mode3 = 0x00;
        ram4.spk_config = 0x00;  /* Crank only */
    }

    ign_wheel_init_6g75();

    printf("Wheel Init: cycle_deg=%u, no_teeth=%u, no_triggers=%u, last_tooth=%u\n",
           cycle_deg, no_teeth, no_triggers, last_tooth);

    /* Simulated nominal tooth time = 1000 microseconds (10.0 deg @ 1666 RPM) */
    const uint32_t BASE_T = 1000;
    int total_pulses = 0;
    int sync_pulse_num = -1;

    for (int cycle = 0; cycle < total_cycles; cycle++) {
        /*
         * Each engine cycle has 64 crank tooth intervals (2 revs * 32 teeth).
         * Physical tooth spacing:
         *   Teeth 0..9  : nominal (10.0 deg -> 1x BASE_T)
         *   Tooth 10    : gap 1 (20.0 deg -> 2x BASE_T)
         *   Teeth 11..20: nominal (10.0 deg -> 1x BASE_T)
         *   Tooth 21    : gap 2 (20.0 deg -> 2x BASE_T)
         *   Teeth 22..30: nominal (10.0 deg -> 1x BASE_T)
         *   Tooth 31    : gap 3 (30.0 deg -> 3x BASE_T)
         *   (repeated for rev 2: teeth 32..63)
         */
        for (int t = 0; t < 64; t++) {
            total_pulses++;

            /*
             * 6G75 36-2-1-1 tooth intervals:
             * Normal tooth interval = 10 deg (BASE_T)
             * Gap 1 (tooth 10): 20 deg (~2.05x BASE_T in real engine)
             * Gap 2 (tooth 21): 20 deg (~2.05x BASE_T in real engine)
             * Gap 3 (tooth 31): 30 deg (~3.05x BASE_T in real engine)
             */
            uint32_t dt = BASE_T;
            int tooth_in_rev = t % 32;
            if (tooth_in_rev == 10 || tooth_in_rev == 21) {
                dt = (uint32_t)(BASE_T * 2.05); /* 20 deg missing tooth */
            } else if (tooth_in_rev == 31) {
                dt = (uint32_t)(BASE_T * 3.05); /* 30 deg double missing tooth */
            }

            /* Shift history */
            tooth_diff_last_1 = tooth_diff_last;
            tooth_diff_last = tooth_diff_this;
            tooth_diff_this = dt;

            /*
             * Cam sensor pulses:
             * Rev 1 (t < 32): 2 cam pulses (e.g. at t=25 and t=28)
             * Rev 2 (t >= 32): 1 cam pulse (e.g. at t=57)
             */
            unsigned char edge2 = 0;
            if (use_cam) {
                if (t == 25 || t == 28 || t == 57) {
                    edge2 = 1;
                }
            }

            (void)ISR_Ign_TimerIn_6g75(1, edge2);

            if ((synch & SYNC_SYNCED) && sync_pulse_num < 0) {
                sync_pulse_num = total_pulses;
                printf(">>> FULL RPM SYNC ACHIEVED at pulse %d! (tooth_no=%u, trig2cnt=%u, synch=0x%02X)\n",
                       total_pulses, tooth_no, trig2cnt, synch);
            }
        }
    }

    if (synch & SYNC_SYNCED) {
        printf("Simulation finished successfully: In full sync, outpc.syncreason=%u\n", outpc.syncreason);
    } else {
        printf("FAILED to achieve sync! outpc.syncreason=%u\n", outpc.syncreason);
    }
}

int main(void)
{
    printf("==================================================================\n");
    printf("   Mitsubishi 6G75 (36-2-1-1) Decoder Standalone Test Suite\n");
    printf("==================================================================\n");

    /* Test 1: Crank Only (Wasted Spark / 360 deg) */
    simulate_engine(false, 3);

    /* Test 2: Crank + Cam (Sequential COP / 720 deg) */
    simulate_engine(true, 3);

    return 0;
}
#endif
