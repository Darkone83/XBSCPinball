#ifndef PCH_H
#define PCH_H

#define _CRT_SECURE_NO_WARNINGS

#ifdef _XBOX
#ifndef _USE_STD_VECTOR_ALGORITHMS
#define _USE_STD_VECTOR_ALGORITHMS 0
#endif
#endif

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cctype>
#include <type_traits>
#ifndef _XBOX
#include <chrono>
#include <iostream>
#endif
#include <vector>
#include <limits>
#include <list>
#include <array>
#ifndef _XBOX
#include <memory>
#endif
#include <utility>
#include <algorithm>
#include <cstring>
#include <string>
#ifndef _XBOX
#include <thread>
#endif
#include <map>
#include <unordered_map>
#include <initializer_list>
#include <new>

#ifdef _XBOX
// Keep the Xbox build free of desktop-only STL facilities that drag the
// modern MSVC interlocked/locale implementation into the legacy XDK headers.
// The engine does not use iostreams, std::thread, or smart pointers on Xbox.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "xb_compat.h"

// The XDK headers may still publish Win32-style min/max macros.  The engine
// uses std::min/std::max throughout, so remove the macros at the common
// boundary rather than touching every gameplay source file.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace xb_fmt
{
    struct Writer
    {
        char* Buffer;
        size_t Capacity;
        size_t Length;

        void Put(char c)
        {
            if (Capacity && Length + 1 < Capacity)
                Buffer[Length] = c;
            ++Length;
        }

        void Finish()
        {
            if (!Capacity || !Buffer)
                return;
            size_t pos = Length < Capacity ? Length : Capacity - 1;
            Buffer[pos] = '\0';
        }
    };

    inline size_t StrLen(const char* text)
    {
        if (!text) return 0;
        const char* p = text;
        while (*p) ++p;
        return static_cast<size_t>(p - text);
    }

    inline void PutString(Writer& w, const char* text, int width, bool left)
    {
        if (!text) text = "(null)";
        const size_t len = StrLen(text);
        int pad = width > static_cast<int>(len) ? width - static_cast<int>(len) : 0;
        if (!left) while (pad-- > 0) w.Put(' ');
        while (*text) w.Put(*text++);
        if (left) while (pad-- > 0) w.Put(' ');
    }

    inline void PutUnsigned(Writer& w, unsigned long long value, unsigned base,
                            bool upper, int width, char pad, bool negative)
    {
        char digits[32];
        unsigned count = 0;
        do
        {
            const unsigned digit = static_cast<unsigned>(value % base);
            digits[count++] = static_cast<char>(digit < 10 ? ('0' + digit) : ((upper ? 'A' : 'a') + digit - 10));
            value /= base;
        } while (value && count < sizeof(digits));

        int total = static_cast<int>(count) + (negative ? 1 : 0);
        int padding = width > total ? width - total : 0;

        if (negative && pad == '0')
        {
            w.Put('-');
            negative = false;
        }
        while (padding-- > 0) w.Put(pad);
        if (negative) w.Put('-');
        while (count) w.Put(digits[--count]);
    }

    inline void PutSigned(Writer& w, long long value, int width, char pad)
    {
        const bool negative = value < 0;
        unsigned long long magnitude;
        if (negative)
            magnitude = static_cast<unsigned long long>(-(value + 1)) + 1ULL;
        else
            magnitude = static_cast<unsigned long long>(value);
        PutUnsigned(w, magnitude, 10, false, width, pad, negative);
    }

    inline void PutFloatGeneral(Writer& w, double value, int precision)
    {
        if (precision <= 0) precision = 6;
        if (precision > 9) precision = 9;

        if (value < 0.0)
        {
            w.Put('-');
            value = -value;
        }

        // Settings values in Space Cadet are small, ordinary decimal values.
        // Keep this deliberately simple and deterministic rather than pulling
        // in the desktop UCRT printf/locale stack.
        unsigned long whole = static_cast<unsigned long>(value);
        PutUnsigned(w, whole, 10, false, 0, ' ', false);

        double frac = value - static_cast<double>(whole);
        if (frac <= 0.0)
            return;

        char tmp[10];
        int used = 0;
        for (int i = 0; i < precision && frac > 0.0; ++i)
        {
            frac *= 10.0;
            int digit = static_cast<int>(frac);
            if (digit < 0) digit = 0;
            if (digit > 9) digit = 9;
            tmp[used++] = static_cast<char>('0' + digit);
            frac -= static_cast<double>(digit);
        }
        while (used > 0 && tmp[used - 1] == '0') --used;
        if (used == 0) return;

        w.Put('.');
        for (int i = 0; i < used; ++i) w.Put(tmp[i]);
    }

    inline int VSnprintf(char* buffer, size_t count, const char* format, va_list args)
    {
        if (!buffer || count == 0 || !format)
            return -1;

        Writer w = { buffer, count, 0 };
        const char* p = format;
        while (*p)
        {
            if (*p != '%')
            {
                w.Put(*p++);
                continue;
            }

            ++p;
            if (*p == '%')
            {
                w.Put('%');
                ++p;
                continue;
            }

            bool left = false;
            char pad = ' ';
            if (*p == '-') { left = true; ++p; }
            if (*p == '0') { pad = '0'; ++p; }

            int width = 0;
            while (*p >= '0' && *p <= '9')
            {
                width = width * 10 + (*p - '0');
                ++p;
            }

            int precision = -1;
            if (*p == '.')
            {
                ++p;
                precision = 0;
                while (*p >= '0' && *p <= '9')
                {
                    precision = precision * 10 + (*p - '0');
                    ++p;
                }
            }

            bool longArg = false;
            if (*p == 'l')
            {
                longArg = true;
                ++p;
                if (*p == 'l') ++p;
            }

            const char spec = *p ? *p++ : '\0';
            switch (spec)
            {
            case 'd':
            case 'i':
                PutSigned(w, longArg ? static_cast<long long>(va_arg(args, long))
                                     : static_cast<long long>(va_arg(args, int)), width, pad);
                break;
            case 'u':
                PutUnsigned(w, longArg ? static_cast<unsigned long long>(va_arg(args, unsigned long))
                                        : static_cast<unsigned long long>(va_arg(args, unsigned int)),
                            10, false, width, pad, false);
                break;
            case 'x':
            case 'X':
                PutUnsigned(w, longArg ? static_cast<unsigned long long>(va_arg(args, unsigned long))
                                        : static_cast<unsigned long long>(va_arg(args, unsigned int)),
                            16, spec == 'X', width, pad, false);
                break;
            case 'c':
                w.Put(static_cast<char>(va_arg(args, int)));
                break;
            case 's':
                PutString(w, va_arg(args, const char*), width, left);
                break;
            case 'p':
                PutUnsigned(w, static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(va_arg(args, void*))),
                            16, false, width ? width : static_cast<int>(sizeof(void*) * 2), '0', false);
                break;
            case 'f':
            case 'g':
            case 'G':
                PutFloatGeneral(w, va_arg(args, double), precision < 0 ? 6 : precision);
                break;
            default:
                // Preserve unknown format sequences visibly instead of failing.
                w.Put('%');
                if (spec) w.Put(spec);
                break;
            }
        }

        w.Finish();
        return static_cast<int>(w.Length);
    }
}

inline int XbSnprintf(char* buffer, size_t count, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int result = xb_fmt::VSnprintf(buffer, count, format, args);
    va_end(args);
    return result;
}
#define snprintf XbSnprintf

constexpr const char* ImGuiRender = "Xbox";
#else
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include <SDL_mixer.h>

constexpr int MIX_INIT_MID_Proxy =
#if SDL_VERSIONNUM(SDL_MIXER_MAJOR_VERSION, SDL_MIXER_MINOR_VERSION, SDL_MIXER_PATCHLEVEL) >= SDL_VERSIONNUM(2, 0, 2)
    MIX_INIT_MID;
#else
    MIX_INIT_FLUIDSYNTH;
#endif

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl.h"
#if SDL_VERSION_ATLEAST(2, 0, 17)
#include "imgui_impl_sdlrenderer.h"
constexpr const char* ImGuiRender = "HW";
inline void ImGui_Render_Init(SDL_Renderer* renderer) { ImGui_ImplSDLRenderer_Init(renderer); }
inline void ImGui_Render_Shutdown() { ImGui_ImplSDLRenderer_Shutdown(); }
inline void ImGui_Render_NewFrame() { ImGui_ImplSDLRenderer_NewFrame(); }
inline void ImGui_Render_RenderDrawData(ImDrawData* draw_data) { ImGui_ImplSDLRenderer_RenderDrawData(draw_data); }
#else
#include "imgui_sdl.h"
constexpr const char* ImGuiRender = "SW";
inline void ImGui_Render_Init(SDL_Renderer* renderer) { ImGuiSDL::Initialize(renderer, 0, 0); }
inline void ImGui_Render_Shutdown() { ImGuiSDL::Deinitialize(); }
inline void ImGui_Render_NewFrame() { }
inline void ImGui_Render_RenderDrawData(ImDrawData* draw_data) { ImGuiSDL::Render(draw_data); }
#endif
#endif

typedef char* LPSTR;
typedef const char* LPCSTR;

constexpr char PathSeparator =
#if defined(_WIN32) || defined(_XBOX)
'\\';
#else
'/';
#endif

#define assertm(exp, msg) assert(((void)msg, exp))

inline size_t pgm_save(int width, int height, char* data, FILE* outfile)
{
    size_t n = 0;
#ifdef _XBOX
    char header[64];
    int headerLen = snprintf(header, sizeof(header), "P5\n%d %d\n%d\n", width, height, 0xFF);
    if (headerLen > 0)
        n += fwrite(header, 1, static_cast<size_t>(headerLen), outfile);
#else
    n += fprintf(outfile, "P5\n%d %d\n%d\n", width, height, 0xFF);
#endif
    n += fwrite(data, 1, width * height, outfile);
    return n;
}

inline float RandFloat() { return static_cast<float>(std::rand() / static_cast<double>(RAND_MAX)); }

template <typename T> constexpr int Sign(T val) { return (T(0) < val) - (val < T(0)); }
template <typename T> const T& Clamp(const T& n, const T& lower, const T& upper)
{
    return std::max(lower, std::min(n, upper));
}

#if defined(_WIN32) && !defined(_XBOX)
extern FILE* fopenu(const char* path, const char* opt);
#else
inline FILE* fopenu(const char* path, const char* opt) { return fopen(path, opt); }
#endif

constexpr const char* PlatformDataPaths[2] =
{
#ifdef _XBOX
    nullptr, nullptr
#elif defined(_WIN32)
    nullptr, nullptr
#else
    "/usr/local/share/SpaceCadetPinball/",
    "/usr/share/SpaceCadetPinball/"
#endif
};

constexpr float Pi = 3.14159265358979323846f;

#endif
