// file start_stop.h

#pragma once

struct StartStopState {
  ButtonDebounce startStopButtonDebounce{"start_button"};
  Timer timeSinceIgnitionPower;
 	Timer startStopStateLastPush;

  bool isFirstTime = true;
  bool startDisabledByLua = false;

  // A start-button press while Lua (security) has not yet authorized cranking is
  // remembered briefly: if the authorization arrives within the latch window we
  // still engage the starter on that same press instead of dropping the request.
  bool hasPendingStartRequest = false;
  Timer pendingStartRequestTimer;

};

void doStartCranking();
void startStopButtonToggle();
void initStartStopButton();
