#pragma once

// Xbox game-side rumble events.
// This header is intentionally XTL-free so normal Space Cadet engine
// translation units can include it without pulling RXDK/WinBase headers in.
enum XbRumbleEvent
{
    XB_RUMBLE_BUMPER = 1,
    XB_RUMBLE_REBOUNDER,
    XB_RUMBLE_TARGET,
    XB_RUMBLE_KICKBACK,
    XB_RUMBLE_DRAIN,
    XB_RUMBLE_TILT,
};

#ifdef _XBOX
extern "C" void XbGameRumbleEvent(int eventId);
#endif
