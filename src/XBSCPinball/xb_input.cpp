#include "xb_xtl_compat.h"
#include "xb_input.h"
#include "xb_rumble.h"

// Implemented on the game/STL side in pb.cpp.  Keep this translation unit
// RXDK/XTL-only so WinBase never collides with the modern MSVC STL intrinsics.
extern "C" void XbGameInputEvent(int logicalButton, int down);
extern "C" void XbGameNewGame();

// SDL controller button values used by the existing options bindings.
// Kept local so this XTL-only translation unit does not need xb_compat.h.
enum
{
    XB_LOGICAL_A = 0,
    XB_LOGICAL_BACK = 4,
    XB_LOGICAL_START = 6,
    XB_LOGICAL_LEFTSHOULDER = 9,
    XB_LOGICAL_RIGHTSHOULDER = 10,
    XB_LOGICAL_DPAD_UP = 11,
    XB_LOGICAL_DPAD_LEFT = 13,
    XB_LOGICAL_DPAD_RIGHT = 14,
};

static HANDLE s_pad = NULL;
static WORD s_prevButtons = 0;
static bool s_exitRequested = false;

// -----------------------------------------------------------------------------
// Rumble test support - private to xb_input.cpp.
//
// Keep this deliberately simple:
//   - one XInputSetState when a pulse begins
//   - one XInputSetState when it expires
//   - no event handle
//   - no public/header dependency
// -----------------------------------------------------------------------------
static unsigned short s_rumbleLow = 0;
static unsigned short s_rumbleHigh = 0;
static unsigned long  s_rumbleStartMs = 0;
static unsigned long  s_rumbleDurationMs = 0;

static void SetPadMotors(unsigned short lowMotor, unsigned short highMotor)
{
    if (!s_pad)
        return;

    if (lowMotor == s_rumbleLow && highMotor == s_rumbleHigh)
        return;

    static XINPUT_FEEDBACK fb;
    ZeroMemory(&fb, sizeof(fb));

    fb.Rumble.wLeftMotorSpeed = lowMotor;
    fb.Rumble.wRightMotorSpeed = highMotor;

    // Synchronous OG Xbox XInput path. Do not attach/close an event handle.
    XInputSetState(s_pad, &fb);

    s_rumbleLow = lowMotor;
    s_rumbleHigh = highMotor;
}

static void ClearRumbleStateNoDevice()
{
    // Local cleanup only. Use this after XInputGetState has already failed;
    // do not issue another motor command through a dead/disconnected handle.
    s_rumbleLow = 0;
    s_rumbleHigh = 0;
    s_rumbleStartMs = 0;
    s_rumbleDurationMs = 0;
}

static void StopRumble()
{
    s_rumbleStartMs = 0;
    s_rumbleDurationMs = 0;
    SetPadMotors(0, 0);
}

static void StartRumblePulse(
    unsigned short lowMotor,
    unsigned short highMotor,
    unsigned long durationMs)
{
    if (!s_pad)
        return;

    SetPadMotors(lowMotor, highMotor);
    s_rumbleStartMs = GetTickCount();
    s_rumbleDurationMs = durationMs;
}

static void PumpRumble()
{
    if ((s_rumbleLow == 0 && s_rumbleHigh == 0) ||
        s_rumbleDurationMs == 0)
        return;

    const unsigned long elapsed = GetTickCount() - s_rumbleStartMs;
    if (elapsed >= s_rumbleDurationMs)
        StopRumble();
}

extern "C" void XbGameRumbleEvent(int eventId)
{
    switch (eventId)
    {
    case XB_RUMBLE_BUMPER:
        // Pop bumper: crisp high-frequency snap with a little cabinet weight.
        StartRumblePulse(0x3000, 0x7600, 45);
        break;

    case XB_RUMBLE_REBOUNDER:
        // Slingshot/rebounder: quick mechanical kick.
        StartRumblePulse(0x2800, 0x7000, 40);
        break;

    case XB_RUMBLE_TARGET:
        // Stand-up / drop target impact.
        StartRumblePulse(0x4800, 0x6000, 55);
        break;

    case XB_RUMBLE_KICKBACK:
        // Ball-saving kickback: stronger two-motor hit.
        StartRumblePulse(0x7800, 0x6800, 85);
        break;

    case XB_RUMBLE_DRAIN:
        // Ball drain: heavier low-frequency thump.
        StartRumblePulse(0x7800, 0x1C00, 150);
        break;

    case XB_RUMBLE_TILT:
        // Tilt should feel unmistakable, but still be a pulse rather than
        // continuous vibration.
        StartRumblePulse(0xFFFF, 0x9000, 300);
        break;

    default:
        break;
    }
}

static WORD BuildLogicalButtons(const XINPUT_GAMEPAD& gp)
{
    WORD b = gp.wButtons & 0x00FF;
    const BYTE* a = gp.bAnalogButtons;

    // Preserve the original Xbox mapping exactly:
    // LT/RT -> flippers, A -> plunger, D-pad -> nudges.
    // Also accept pads that mirror analog buttons into upper wButtons bits.
    const WORD raw = gp.wButtons;
    if (a[XINPUT_GAMEPAD_LEFT_TRIGGER] > 32 || (raw & 0x0400))  b |= 0x0100;
    if (a[XINPUT_GAMEPAD_RIGHT_TRIGGER] > 32 || (raw & 0x0800)) b |= 0x0200;
    if (a[XINPUT_GAMEPAD_A] > 32 || (raw & 0x1000))             b |= 0x0400;
    if (a[XINPUT_GAMEPAD_B] > 32)                               b |= 0x0800;
    if (a[XINPUT_GAMEPAD_X] > 32)                               b |= 0x1000;
    if (a[XINPUT_GAMEPAD_Y] > 32)                               b |= 0x2000;
    return b;
}

static void EmitController(int logicalButton, bool down)
{
    XbGameInputEvent(logicalButton, down ? 1 : 0);
}

void XbInputInit()
{
    XDEVICE_PREALLOC_TYPE prealloc[] = {{XDEVICE_TYPE_GAMEPAD, 4}};
    XInitDevices(1, prealloc);

    DWORD devices = XGetDevices(XDEVICE_TYPE_GAMEPAD);
    for (DWORD port = 0; port < 4; ++port)
    {
        if (devices & (1u << port))
        {
            s_pad = XInputOpen(XDEVICE_TYPE_GAMEPAD, port, XDEVICE_NO_SLOT, NULL);
            if (s_pad)
                break;
        }
    }

    s_prevButtons = 0;
    s_exitRequested = false;
    s_rumbleLow = 0;
    s_rumbleHigh = 0;
    s_rumbleStartMs = 0;
    s_rumbleDurationMs = 0;
}

void XbInputShutdown()
{
    if (s_pad)
    {
        StopRumble();
        XInputClose(s_pad);
        s_pad = NULL;
    }

    s_rumbleLow = 0;
    s_rumbleHigh = 0;
    s_rumbleStartMs = 0;
    s_rumbleDurationMs = 0;
}

void XbInputPump()
{
    if (!s_pad)
    {
        DWORD devices = XGetDevices(XDEVICE_TYPE_GAMEPAD);
        for (DWORD port = 0; port < 4; ++port)
        {
            if (devices & (1u << port))
            {
                s_pad = XInputOpen(XDEVICE_TYPE_GAMEPAD, port, XDEVICE_NO_SLOT, NULL);
                if (s_pad)
                    break;
            }
        }

        if (!s_pad)
            return;

        // A freshly opened pad starts with no rumble state.
        s_rumbleLow = 0;
        s_rumbleHigh = 0;
        s_rumbleStartMs = 0;
        s_rumbleDurationMs = 0;
    }

    // Stop an expired pulse before processing the next input edge.
    PumpRumble();

    XINPUT_STATE st;
    ZeroMemory(&st, sizeof(st));
    if (XInputGetState(s_pad, &st) != ERROR_SUCCESS)
    {
        ClearRumbleStateNoDevice();
        XInputClose(s_pad);
        s_pad = NULL;
        s_prevButtons = 0;
        return;
    }

    const WORD now = BuildLogicalButtons(st.Gamepad);
    const WORD pressed = now & ~s_prevButtons;
    const WORD released = ~now & s_prevButtons;

    struct Map
    {
        WORD mask;
        int logical;
    };

    static const Map map[] =
    {
        {0x0100, XB_LOGICAL_LEFTSHOULDER},
        {0x0200, XB_LOGICAL_RIGHTSHOULDER},
        {0x0400, XB_LOGICAL_A},
        {XINPUT_GAMEPAD_DPAD_LEFT, XB_LOGICAL_DPAD_LEFT},
        {XINPUT_GAMEPAD_DPAD_RIGHT, XB_LOGICAL_DPAD_RIGHT},
        {XINPUT_GAMEPAD_DPAD_UP, XB_LOGICAL_DPAD_UP},
        {XINPUT_GAMEPAD_START, XB_LOGICAL_START},
        {XINPUT_GAMEPAD_BACK, XB_LOGICAL_BACK},
    };

    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); ++i)
    {
        if (pressed & map[i].mask)
            EmitController(map[i].logical, true);
        if (released & map[i].mask)
            EmitController(map[i].logical, false);
    }

    // Y is a fixed Xbox command, intentionally independent of pinball.ini.
    // This guarantees New Game even when an older settings file persisted the
    // desktop New Game binding as "unbound".
    if (pressed & 0x2000)
        XbGameNewGame();

    // -------------------------------------------------------------------------
    // Initial rumble test mapping.
    //
    // Flippers: short/light "solenoid click".
    // Plunger:  medium/heavy launch pulse.
    // Nudges:    stronger low-frequency table thump.
    //
    // These fire only on PRESS edges, never while a control is held.
    // -------------------------------------------------------------------------
    if (pressed & 0x0100) // LT -> left flipper
        StartRumblePulse(0x1400, 0x5800, 35);

    if (pressed & 0x0200) // RT -> right flipper
        StartRumblePulse(0x1400, 0x5800, 35);

    if (pressed & 0x0400) // A -> plunger / launch
        StartRumblePulse(0x6800, 0x2400, 90);

    if (pressed & (XINPUT_GAMEPAD_DPAD_LEFT |
                   XINPUT_GAMEPAD_DPAD_RIGHT |
                   XINPUT_GAMEPAD_DPAD_UP))
        StartRumblePulse(0x7800, 0x1800, 75);

    // Back + Start together is the hard dashboard escape used during bring-up.
    if ((now & XINPUT_GAMEPAD_BACK) && (now & XINPUT_GAMEPAD_START))
        s_exitRequested = true;

    s_prevButtons = now;
}


bool XbInputWaitForContinue()
{
    for (;;)
    {
        // Support a controller that was not present when XbInputInit ran.
        if (!s_pad)
        {
            DWORD devices = XGetDevices(XDEVICE_TYPE_GAMEPAD);
            for (DWORD port = 0; port < 4; ++port)
            {
                if (devices & (1u << port))
                {
                    s_pad = XInputOpen(
                        XDEVICE_TYPE_GAMEPAD,
                        port,
                        XDEVICE_NO_SLOT,
                        NULL);

                    if (s_pad)
                        break;
                }
            }

            if (!s_pad)
            {
                Sleep(50);
                continue;
            }
        }

        XINPUT_STATE st;
        ZeroMemory(&st, sizeof(st));

        if (XInputGetState(s_pad, &st) != ERROR_SUCCESS)
        {
            ClearRumbleStateNoDevice();
            XInputClose(s_pad);
            s_pad = NULL;
            s_prevButtons = 0;
            Sleep(50);
            continue;
        }

        const WORD now = BuildLogicalButtons(st.Gamepad);

        // Preserve the bring-up/dashboard escape even while the help splash is up.
        if ((now & XINPUT_GAMEPAD_BACK) && (now & XINPUT_GAMEPAD_START))
        {
            s_exitRequested = true;
            return false;
        }

        const bool continueDown =
            (now & 0x0400) != 0 ||                     // A
            (now & XINPUT_GAMEPAD_START) != 0;         // Start

        if (continueDown)
        {
            // Consume the press completely so A does not immediately launch the
            // ball and Start does not immediately pause when gameplay begins.
            for (;;)
            {
                ZeroMemory(&st, sizeof(st));

                if (XInputGetState(s_pad, &st) != ERROR_SUCCESS)
                {
                    ClearRumbleStateNoDevice();
                    XInputClose(s_pad);
                    s_pad = NULL;
                    break;
                }

                const WORD held = BuildLogicalButtons(st.Gamepad);
                if ((held & 0x0400) == 0 &&
                    (held & XINPUT_GAMEPAD_START) == 0)
                    break;

                Sleep(10);
            }

            s_prevButtons = 0;
            return true;
        }

        Sleep(10);
    }
}

bool XbInputExitRequested()
{
    return s_exitRequested;
}
