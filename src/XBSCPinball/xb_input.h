#pragma once

void XbInputInit();
void XbInputShutdown();
void XbInputPump();
bool XbInputExitRequested();

// Used by the boot help splash. Waits for A or Start, and consumes the press/release.
bool XbInputWaitForContinue();
