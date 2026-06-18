#pragma once

#include "cdv_controller_state_generated.h"
#include <rusefi/timer.h>

class CdvController : public cdv_controller_state_s, public EngineModule {
public:
    using interface_t = CdvController;

    void onSlowCallback() override;

private:
    Timer m_sinceExitTimer;
    Timer m_sinceActiveTimer;
    bool m_prevLaunchActive = false;
    bool m_prevClutchDown = false;
};
