#include "pch.h"
#include "fullscrn.h"
#include "options.h"
#include "render.h"
#include "pb.h"
#include "xb_d3d.h"

int fullscrn::screen_mode = 1;
int fullscrn::display_changed = 0;
int fullscrn::resolution = 0;

const resolution_info fullscrn::resolution_array[3] =
{
    {640, 480, 600, 416, 501},
    {800, 600, 752, 520, 502},
    {1024, 768, 960, 666, 503},
};

float fullscrn::ScaleX = 1.0f;
float fullscrn::ScaleY = 1.0f;
int fullscrn::OffsetX = 0;
int fullscrn::OffsetY = 0;

void fullscrn::init()
{
    // Standard Space Cadet data uses the 600x416 resolution-0 table assets.
    resolution = 0;
    window_size_changed();
}

void fullscrn::shutdown()
{
}

int fullscrn::set_screen_mode(int)
{
    screen_mode = 1;
    return 1;
}

int fullscrn::enableFullscreen()
{
    return 1;
}

int fullscrn::disableFullscreen()
{
    return 1;
}

void fullscrn::activate(int)
{
}

int fullscrn::GetResolution()
{
    return 0;
}

void fullscrn::SetResolution(int)
{
    // Standard PINBALL.DAT only has the resolution-0 asset path.
    resolution = 0;
}

int fullscrn::GetMaxResolution()
{
    return 0;
}

void fullscrn::window_size_changed()
{
    const resolution_info& res = resolution_array[0];
    const int sourceW = (int)res.TableWidth;
    const int sourceH = (int)res.TableHeight;
    const int outputW = g_bbWidth > 0 ? g_bbWidth : 640;
    const int outputH = g_bbHeight > 0 ? g_bbHeight : 480;

    int destW = outputW;
    int destH = outputH;

    if (options::Options.IntegerScaling)
    {
        // Pixel-perfect mode. This deliberately does not invent fractional
        // pixels: at 720p the native 600x416 surface is therefore 1x and
        // centered; at a sufficiently large mode it can become 2x, 3x, etc.
        int scaleX = outputW / sourceW;
        int scaleY = outputH / sourceH;

        if (scaleX < 1) scaleX = 1;
        if (scaleY < 1) scaleY = 1;

        if (options::Options.UniformScaling)
        {
            const int scale = scaleX < scaleY ? scaleX : scaleY;
            destW = sourceW * scale;
            destH = sourceH * scale;
        }
        else
        {
            destW = sourceW * scaleX;
            destH = sourceH * scaleY;
        }
    }
    else if (options::Options.UniformScaling)
    {
        // Aspect-fit the native game surface into the real Xbox backbuffer.
        // Use integer ratio math so this path does not create another
        // float-to-int CRT dependency on RXDK.
        const long lhs = (long)outputW * (long)sourceH;
        const long rhs = (long)outputH * (long)sourceW;

        if (lhs <= rhs)
        {
            // Width limited.
            destW = outputW;
            destH = (outputW * sourceH) / sourceW;
        }
        else
        {
            // Height limited.
            destH = outputH;
            destW = (outputH * sourceW) / sourceH;
        }
    }
    else
    {
        // Stretch mode: fill the complete Xbox backbuffer. This is the
        // intentional 16:9 mode when output is 720p.
        destW = outputW;
        destH = outputH;
    }

    if (destW < 1) destW = 1;
    if (destH < 1) destH = 1;

    OffsetX = (outputW - destW) / 2;
    OffsetY = (outputH - destH) / 2;

    ScaleX = (float)destW / (float)sourceW;
    ScaleY = (float)destH / (float)sourceH;

    render::DestinationRect =
    {
        OffsetX,
        OffsetY,
        destW,
        destH
    };
}

SDL_Rect fullscrn::GetScreenRectFromPinballRect(SDL_Rect rect)
{
    SDL_Rect converted{};

    if (!render::vscreen ||
        render::vscreen->Width <= 0 ||
        render::vscreen->Height <= 0)
    {
        return converted;
    }

    converted.x =
        rect.x * render::DestinationRect.w / render::vscreen->Width +
        render::DestinationRect.x;

    converted.y =
        rect.y * render::DestinationRect.h / render::vscreen->Height +
        render::DestinationRect.y;

    converted.w =
        rect.w * render::DestinationRect.w / render::vscreen->Width;

    converted.h =
        rect.h * render::DestinationRect.h / render::vscreen->Height;

    return converted;
}

float fullscrn::GetScreenToPinballRatio()
{
    if (!render::vscreen || render::vscreen->Width <= 0)
        return 1.0f;

    return (float)render::DestinationRect.w /
        (float)render::vscreen->Width;
}
