#include "xb_xtl_compat.h"
#include "xb_d3d.h"
#include "xb_input.h"
#include "xb_splash.h"
#include "spacecadet_help_splash.h"

#if defined(__has_include)
#  if __has_include("rxdk_splash.h") && __has_include("darkone83_splash.h")
#    define XB_HAVE_OPENJAZZ_SPLASH 1
#  endif
#endif
#ifndef XB_HAVE_OPENJAZZ_SPLASH
#define XB_HAVE_OPENJAZZ_SPLASH 0
#endif

#if XB_HAVE_OPENJAZZ_SPLASH
#include "rxdk_splash.h"
#include "darkone83_splash.h"
#endif

struct SplashVtx
{
    float x, y, z, rhw;
    float u, v;
};

#define SPLASH_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)

static void SetSplashState(bool linear)
{
    D3DDevice_SetRenderState(D3DRS_ZENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_LIGHTING, FALSE);
    D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    const DWORD filter = linear ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, filter);
    D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, filter);
    D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

    D3DDevice_SetVertexShader(SPLASH_FVF);
}

static void BuildFullQuad(SplashVtx q[4], int srcW, int srcH)
{
    const float x1 = (float)g_bbWidth;
    const float y1 = (float)g_bbHeight;
    const float u1 = (float)srcW;
    const float v1 = (float)srcH;

    q[0] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    q[1] = { x1,   0.0f, 0.0f, 1.0f, u1,   0.0f };
    q[2] = { 0.0f, y1,   0.0f, 1.0f, 0.0f, v1 };
    q[3] = { x1,   y1,   0.0f, 1.0f, u1,   v1 };
}

static void BuildAspectFitQuad(SplashVtx q[4], int srcW, int srcH)
{
    float dstW = (float)g_bbWidth;
    float dstH = dstW * (float)srcH / (float)srcW;

    if (dstH > (float)g_bbHeight)
    {
        dstH = (float)g_bbHeight;
        dstW = dstH * (float)srcW / (float)srcH;
    }

    const float x0 = ((float)g_bbWidth - dstW) * 0.5f;
    const float y0 = ((float)g_bbHeight - dstH) * 0.5f;
    const float x1 = x0 + dstW;
    const float y1 = y0 + dstH;
    const float u1 = (float)srcW;
    const float v1 = (float)srcH;

    q[0] = { x0, y0, 0.0f, 1.0f, 0.0f, 0.0f };
    q[1] = { x1, y0, 0.0f, 1.0f, u1,   0.0f };
    q[2] = { x0, y1, 0.0f, 1.0f, 0.0f, v1 };
    q[3] = { x1, y1, 0.0f, 1.0f, u1,   v1 };
}

static void RenderTexture(D3DTexture* tex, SplashVtx q[4], bool linear)
{
    D3DDevice_BeginScene();
    D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
    SetSplashState(linear);
    D3DDevice_SetTexture(0, tex);
    D3DDevice_DrawVerticesUP(
        D3DPT_TRIANGLESTRIP,
        4,
        q,
        sizeof(SplashVtx));
    D3DDevice_EndScene();
    D3DDevice_Swap(0);
}

#if XB_HAVE_OPENJAZZ_SPLASH
static void ShowRXDK(DWORD holdMs)
{
    D3DTexture* tex =
        XbCreateLinearTexture(RXDK_SPLASH_WIDTH, RXDK_SPLASH_HEIGHT);

    if (!tex)
        return;

    SplashVtx q[4];
    BuildFullQuad(q, RXDK_SPLASH_WIDTH, RXDK_SPLASH_HEIGHT);

    for (int phase = 0; phase < 3; ++phase)
    {
        const int frames = phase == 1 ? 1 : 16;

        for (int frame = 0; frame < frames; ++frame)
        {
            int brightness =
                phase == 0 ? frame * 16 :
                (phase == 1 ? 255 : 255 - frame * 16);

            if (brightness < 0) brightness = 0;
            if (brightness > 255) brightness = 255;

            D3DLOCKED_RECT lr;
            if (FAILED(tex->LockRect(0, &lr, NULL, 0)))
            {
                tex->Release();
                return;
            }

            unsigned int* dst = (unsigned int*)lr.pBits;
            const int pitch = lr.Pitch / 4;
            int x = 0;
            int y = 0;

            for (unsigned int i = 0; i < RXDK_SPLASH_RLE_COUNT; ++i)
            {
                unsigned int count = rxdk_splash_rle[i].count;
                const unsigned int c = rxdk_splash_rle[i].color;

                unsigned int r = ((c >> 11) & 31);
                unsigned int g = ((c >> 5) & 63);
                unsigned int b = (c & 31);

                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);

                r = (r * brightness) >> 8;
                g = (g * brightness) >> 8;
                b = (b * brightness) >> 8;

                const unsigned int out =
                    (r << 16) | (g << 8) | b;

                while (count-- && y < RXDK_SPLASH_HEIGHT)
                {
                    dst[y * pitch + x++] = out;

                    if (x >= RXDK_SPLASH_WIDTH)
                    {
                        x = 0;
                        ++y;
                    }
                }
            }

            tex->UnlockRect(0);
            RenderTexture(tex, q, false);
            Sleep(phase == 1 ? holdMs : 33);
        }
    }

    tex->Release();
    D3DDevice_SetTexture(0, NULL);
}

static void ShowDarkone(DWORD holdMs)
{
    D3DTexture* tex =
        XbCreateLinearTexture(
            DARKONE83_SPLASH_WIDTH,
            DARKONE83_SPLASH_HEIGHT);

    if (!tex)
        return;

    SplashVtx q[4];
    BuildFullQuad(
        q,
        DARKONE83_SPLASH_WIDTH,
        DARKONE83_SPLASH_HEIGHT);

    for (int phase = 0; phase < 3; ++phase)
    {
        const int frames = phase == 1 ? 1 : 16;

        for (int frame = 0; frame < frames; ++frame)
        {
            int brightness =
                phase == 0 ? frame * 16 :
                (phase == 1 ? 255 : 255 - frame * 16);

            if (brightness < 0) brightness = 0;
            if (brightness > 255) brightness = 255;

            D3DLOCKED_RECT lr;
            if (FAILED(tex->LockRect(0, &lr, NULL, 0)))
            {
                tex->Release();
                return;
            }

            unsigned int* dst = (unsigned int*)lr.pBits;
            const int pitch = lr.Pitch / 4;
            int x = 0;
            int y = 0;

            for (unsigned int i = 0;
                i < DARKONE83_SPLASH_RLE_COUNT;
                ++i)
            {
                unsigned int count =
                    darkone83_splash_rle[i].count;

                const unsigned int c =
                    darkone83_splash_rle[i].color;

                unsigned int r = ((c >> 11) & 31);
                unsigned int g = ((c >> 5) & 63);
                unsigned int b = (c & 31);

                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);

                r = (r * brightness) >> 8;
                g = (g * brightness) >> 8;
                b = (b * brightness) >> 8;

                const unsigned int out =
                    (r << 16) | (g << 8) | b;

                while (count-- &&
                    y < DARKONE83_SPLASH_HEIGHT)
                {
                    dst[y * pitch + x++] = out;

                    if (x >= DARKONE83_SPLASH_WIDTH)
                    {
                        x = 0;
                        ++y;
                    }
                }
            }

            tex->UnlockRect(0);
            RenderTexture(tex, q, false);
            Sleep(phase == 1 ? holdMs : 33);
        }
    }

    tex->Release();
    D3DDevice_SetTexture(0, NULL);
}
#endif

static bool DecodeHelpTexture(D3DTexture* tex, int brightness)
{
    D3DLOCKED_RECT lr;

    if (!tex ||
        FAILED(tex->LockRect(0, &lr, NULL, 0)))
        return false;

    unsigned int* dst =
        (unsigned int*)lr.pBits;

    const int pitch = lr.Pitch / 4;
    int x = 0;
    int y = 0;

    for (unsigned int i = 0;
        i < SPACECADET_HELP_SPLASH_RLE_COUNT;
        ++i)
    {
        unsigned int count =
            spacecadet_help_splash_rle[i].count;

        const unsigned int c =
            spacecadet_help_splash_rle[i].color;

        unsigned int r = ((c >> 11) & 31);
        unsigned int g = ((c >> 5) & 63);
        unsigned int b = (c & 31);

        r = (r << 3) | (r >> 2);
        g = (g << 2) | (g >> 4);
        b = (b << 3) | (b >> 2);

        r = (r * (unsigned int)brightness) >> 8;
        g = (g * (unsigned int)brightness) >> 8;
        b = (b * (unsigned int)brightness) >> 8;

        const unsigned int out =
            (r << 16) | (g << 8) | b;

        while (count-- &&
            y < SPACECADET_HELP_SPLASH_HEIGHT)
        {
            dst[y * pitch + x++] = out;

            if (x >= SPACECADET_HELP_SPLASH_WIDTH)
            {
                x = 0;
                ++y;
            }
        }
    }

    tex->UnlockRect(0);
    return true;
}

static void ShowControlsHelp()
{
    D3DTexture* tex =
        XbCreateLinearTexture(
            SPACECADET_HELP_SPLASH_WIDTH,
            SPACECADET_HELP_SPLASH_HEIGHT);

    if (!tex)
        return;

    SplashVtx q[4];

    // The controls image is authored at 16:9. Fill a 720p backbuffer exactly,
    // but letterbox it on 480i/480p/PAL so the controller and labels stay round.
    BuildAspectFitQuad(
        q,
        SPACECADET_HELP_SPLASH_WIDTH,
        SPACECADET_HELP_SPLASH_HEIGHT);

    // Short fade-in. No timed hold: the screen itself says Press A to Continue.
    for (int frame = 1; frame <= 12; ++frame)
    {
        int brightness = frame * 255 / 12;

        if (!DecodeHelpTexture(tex, brightness))
        {
            tex->Release();
            return;
        }

        RenderTexture(tex, q, true);
        Sleep(25);
    }

    // Make sure the final frame is exactly full brightness before waiting.
    if (DecodeHelpTexture(tex, 255))
        RenderTexture(tex, q, true);

    XbInputWaitForContinue();

    // Brief fade to black after the press so game initialization does not cut
    // directly from the help art to a partially constructed first game frame.
    for (int frame = 11; frame >= 0; --frame)
    {
        const int brightness = frame * 255 / 12;

        if (!DecodeHelpTexture(tex, brightness))
            break;

        RenderTexture(tex, q, true);
        Sleep(20);
    }

    tex->Release();
    D3DDevice_SetTexture(0, NULL);

    D3DDevice_Clear(
        0,
        NULL,
        D3DCLEAR_TARGET,
        0x00000000,
        1.0f,
        0);

    D3DDevice_Swap(0);
}

void XbShowSplashes(void)
{
#if XB_HAVE_OPENJAZZ_SPLASH
    ShowRXDK(950);
    ShowDarkone(950);
#else
    // Keep a deterministic fallback when the two OpenJazz-generated headers are
    // not present. The baked controls/help splash below is always available.
    D3DDevice_Clear(
        0,
        NULL,
        D3DCLEAR_TARGET,
        0x00002050,
        1.0f,
        0);

    D3DDevice_Swap(0);
    Sleep(250);

    D3DDevice_Clear(
        0,
        NULL,
        D3DCLEAR_TARGET,
        0x00000000,
        1.0f,
        0);

    D3DDevice_Swap(0);
    Sleep(100);
#endif

    ShowControlsHelp();
}
