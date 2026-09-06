#pragma once

// file tc_4l6x.h

#include "tcu.h"
#include "tc_4.h"

#if EFI_TCU
class Gm4l6xTransmissionController: public Generic4TransmissionController {
public:
	void update(gear_e);
	void init();
	TransmissionControllerMode getMode() const {
		return TransmissionControllerMode::Gm4l6x;
	}
private:
	void set32State(gear_e gear);
};

Gm4l6xTransmissionController* getGm4l6xTransmissionController();

void configureTcu4R70W();

#endif // EFI_TCU
