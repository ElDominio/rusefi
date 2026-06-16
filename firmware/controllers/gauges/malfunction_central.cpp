/**
 * @file malfunction_central.c
 * @brief This data structure holds current malfunction codes
 *
 * todo: make this a class
 *
 * @date Dec 20, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 */

#include "pch.h"

#include "malfunction_central.h"

static error_codes_set_s error_codes_set;

void clearWarnings(void) {
	error_codes_set.count = 0;
}

/**
 * Search if code is present
 * @return -1 if code not found
 */
static int find_position(ObdCode e_code) {
	// cycle for searching element equal seaching code
	for (int t = 0; t < error_codes_set.count; t++)
		if (error_codes_set.error_codes[t] == e_code)
			return t;			// we found position where this code is present
	return -1;														// -1 if code not found
}

void addError(ObdCode errorCode) {
	if (error_codes_set.count < MAX_ERROR_CODES_COUNT && find_position(errorCode) == -1) {
		error_codes_set.error_codes[error_codes_set.count] = errorCode;
		error_codes_set.count++;
	}
}

void removeError(ObdCode errorCode) {
	int pos = find_position(errorCode);
	if (pos >= 0) {
		// shift all right elements to one pos left
		for (int t = pos; t < error_codes_set.count - 1; t++) {
			error_codes_set.error_codes[t] = error_codes_set.error_codes[t + 1];
		}

		error_codes_set.error_codes[--error_codes_set.count] = (ObdCode)0;				// place 0
	}
}

uint8_t obdCodeSeverity(ObdCode errorCode) {
	// Programmer-defined fault weighting. Edit this table (not the tune) to decide how much
	// each fault class contributes toward latching Limp mode.
	switch (errorCode) {
		// Misfire: P0300 (random/multiple) and P0301..P0312 (per-cylinder). A latched misfire
		// is serious enough to get the car home in Limp on its own at the default threshold.
		case ObdCode::OBD_Random_Misfire:
		case ObdCode::OBD_Cylinder_1_Misfire:
		case ObdCode::OBD_Cylinder_2_Misfire:
		case ObdCode::OBD_Cylinder_3_Misfire:
		case ObdCode::OBD_Cylinder_4_Misfire:
		case ObdCode::OBD_Cylinder_5_Misfire:
		case ObdCode::OBD_Cylinder_6_Misfire:
		case ObdCode::OBD_Cylinder_7_Misfire:
		case ObdCode::OBD_Cylinder_8_Misfire:
		case ObdCode::OBD_Cylinder_9_Misfire:
		case ObdCode::OBD_Cylinder_10_Misfire:
		case ObdCode::OBD_Cylinder_11_Misfire:
		case ObdCode::OBD_Cylinder_12_Misfire:
			return 5;
		// Everything else is informational for now (does not push toward Limp). Add cases here
		// — e.g. a TPS calibration fault at severity 1 — as faults are wired up over time.
		default:
			return 0;
	}
}

uint16_t getErrorSeverityTotal(void) {
	uint16_t total = 0;
	for (int t = 0; t < error_codes_set.count; t++) {
		total += obdCodeSeverity(error_codes_set.error_codes[t]);
	}
	return total;
}

void getErrorCodes(error_codes_set_s * copy) {
	copy->count = error_codes_set.count;
	copyArray(copy->error_codes, error_codes_set.error_codes);
}

bool hasErrorCodes(void) {
	return error_codes_set.count > 0;
}
