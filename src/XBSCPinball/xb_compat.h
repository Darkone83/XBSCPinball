#pragma once

// Minimal SDL/ImGui compatibility surface for the RXDK build.
// This is intentionally not an SDL implementation. It only provides the
// legacy types/constants that leak into the original Space Cadet headers.

// IMPORTANT: do not include <xtl.h> here.
//
// This header is included by pch.h and therefore reaches almost every game
// translation unit. Mixing RXDK WinBase declarations from <xtl.h> with the
// modern MSVC STL in those units causes the _Interlocked* C-linkage conflict
// (intrin0.inl.h / atomic vs RXDK winbase.h). Keep this header platform-light
// and let the Xbox backend .cpp files include <xtl.h> only when they actually
// need it.
#include <stdint.h>
#include <stddef.h>

// D3D texture is opaque to the original game code. The complete RXDK type is
// only required inside the D3D backend translation unit.
struct D3DTexture;

// Keep this compatibility header completely free of Xbox platform imports.
//
// Timing, sleeping, debug output, D3D, input and audio are owned by the
// Xbox backend translation units that include <xtl.h> themselves.  Pulling
// even small WinBase declarations into this shared header recreates the
// RXDK winbase.h vs modern MSVC intrin0.inl.h/_Interlocked* collision.

typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef unsigned int Uint32;
typedef signed char Sint8;
typedef short Sint16;
typedef int Sint32;

typedef D3DTexture SDL_Texture;
struct SDL_Window {};
struct SDL_Renderer {};
struct SDL_GameController {};
union SDL_Event { Uint32 type; unsigned char data[64]; };
struct ImGuiIO {};
struct ImGuiContext {};
struct ImGuiSettingsHandler {};
struct ImGuiTextBuffer {};
typedef unsigned short ImWchar;
typedef unsigned int ImU32;
template <typename T> struct ImVector {};

#ifndef IM_COL32
#define IM_COL32(R,G,B,A) (((ImU32)(A)<<24) | ((ImU32)(B)<<16) | ((ImU32)(G)<<8) | (ImU32)(R))
#endif

struct SDL_Rect { int x, y, w, h; };
struct SDL_FRect { float x, y, w, h; };

enum
{
    SDL_MESSAGEBOX_ERROR = 0x10,
    SDL_MESSAGEBOX_WARNING = 0x20,
    SDL_TEXTUREACCESS_STATIC = 0,
    SDL_TEXTUREACCESS_STREAMING = 1,
    SDL_TEXTUREACCESS_TARGET = 2,
    SDL_BUTTON_LEFT = 1,
    SDL_BUTTON_MIDDLE = 2,
    SDL_BUTTON_RIGHT = 3,
    SDL_BUTTON_X1 = 4,
    SDL_BUTTON_X2 = 5,
};

// Match SDL2 GameController button numbering so the original option table can
// stay intact. The Xbox input backend emits these logical IDs directly.
enum
{
    SDL_CONTROLLER_BUTTON_A = 0,
    SDL_CONTROLLER_BUTTON_B = 1,
    SDL_CONTROLLER_BUTTON_X = 2,
    SDL_CONTROLLER_BUTTON_Y = 3,
    SDL_CONTROLLER_BUTTON_BACK = 4,
    SDL_CONTROLLER_BUTTON_GUIDE = 5,
    SDL_CONTROLLER_BUTTON_START = 6,
    SDL_CONTROLLER_BUTTON_LEFTSTICK = 7,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK = 8,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER = 9,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER = 10,
    SDL_CONTROLLER_BUTTON_DPAD_UP = 11,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN = 12,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT = 13,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT = 14,
    SDL_CONTROLLER_BUTTON_MAX = 21,
};

// Only the keyboard constants referenced by the original game/options code.
enum
{
    SDLK_UNKNOWN = 0,
    SDLK_BACKSPACE = 8,
    SDLK_RETURN = 13,
    SDLK_ESCAPE = 27,
    SDLK_SPACE = 32,
    SDLK_PERIOD = 46,
    SDLK_SLASH = 47,
    SDLK_a = 97,
    SDLK_b = 98,
    SDLK_h = 104,
    SDLK_i = 105,
    SDLK_j = 106,
    SDLK_p = 112,
    SDLK_r = 114,
    SDLK_s = 115,
    SDLK_x = 120,
    SDLK_z = 122,
    SDLK_UP = 273,
    SDLK_DOWN = 274,
    SDLK_RIGHT = 275,
    SDLK_LEFT = 276,
    SDLK_F2 = 283,
    SDLK_F3 = 284,
    SDLK_F4 = 285,
    SDLK_F5 = 286,
    SDLK_F6 = 287,
    SDLK_F8 = 289,
    SDLK_F9 = 290,
    SDLK_F12 = 293,
};

#define MIX_MAX_VOLUME 128

struct XbWave;
struct XbMidiTrack;
typedef XbWave Mix_Chunk;
typedef XbMidiTrack Mix_Music;

inline const char* SDL_GetHint(const char*) { return 0; }
inline int SDL_SetHint(const char*, const char*) { return 1; }

// No SDL timing shims on Xbox.  winmain.h/winmain_xbox.cpp use the
// Xbox-native timing path, and Xbox backend code owns GetTickCount/Sleep.

#define SDL_VERSION_ATLEAST(X,Y,Z) 1
