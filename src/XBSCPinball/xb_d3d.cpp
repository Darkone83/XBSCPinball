
#include "xb_xtl_compat.h"
#include "xb_d3d.h"


#ifndef D3DFMT_LIN_X8R8G8B8
#define D3DFMT_LIN_X8R8G8B8 ((D3DFORMAT)0x0000001e)
#endif

int g_bbWidth = 640;
int g_bbHeight = 480;

static IDirect3D8* s_d3d = NULL;
static IDirect3DDevice8* s_device = NULL;

struct XbBlitVertex { float x, y, z, rhw, u, v; };
#define XB_BLIT_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)

/*
 * Device creation is intentionally the same pattern used by USB2XB:
 *   - retain IDirect3D8 and IDirect3DDevice8
 *   - detect the console AV mode before CreateDevice
 *   - prefer the best enabled mode
 *   - try Xbox 2x linear AA, then the same mode without AA
 *   - if that mode is rejected, retry a safe SD mode
 */
static HRESULT CreateDeviceWithAA(D3DPRESENT_PARAMETERS* pp)
{
    if (!pp || !s_d3d)
        return E_FAIL;

    s_device = NULL;
    pp->MultiSampleType = D3DMULTISAMPLE_2_SAMPLES_MULTISAMPLE_LINEAR;

    HRESULT hr = s_d3d->CreateDevice(
        0,
        D3DDEVTYPE_HAL,
        NULL,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        pp,
        &s_device);

    if (FAILED(hr) || !s_device)
    {
        if (s_device)
        {
            s_device->Release();
            s_device = NULL;
        }

        pp->MultiSampleType = D3DMULTISAMPLE_NONE;
        hr = s_d3d->CreateDevice(
            0,
            D3DDEVTYPE_HAL,
            NULL,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            pp,
            &s_device);
    }

    return hr;
}

static void SetBlitState(bool linear)
{
    if (!s_device)
        return;

    s_device->SetVertexShader(XB_BLIT_FVF);
    s_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    s_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    s_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    s_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    s_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    s_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    s_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    s_device->SetTextureStageState(0, D3DTSS_MINFILTER,
        linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    s_device->SetTextureStageState(0, D3DTSS_MAGFILTER,
        linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    s_device->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    s_device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    s_device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
}

bool XbD3DInit()
{
    if (s_device)
        return true;

    s_d3d = Direct3DCreate8(D3D_SDK_VERSION);
    if (!s_d3d)
        return false;

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));

    const DWORD vflags = XGetVideoFlags();
    const DWORD vstd = XGetVideoStandard();

    const int palI = (vstd == XC_VIDEO_STANDARD_PAL_I) ? 1 : 0;
    const int pal60 = (vflags & XC_VIDEO_FLAGS_PAL_60Hz) ? 1 : 0;
    const int has480p = (vflags & XC_VIDEO_FLAGS_HDTV_480p) ? 1 : 0;
    const int has720p = (vflags & XC_VIDEO_FLAGS_HDTV_720p) ? 1 : 0;

    pp.FullScreen_RefreshRateInHz = 60;

    /* USB2XB's proven mode order: 720p -> 480p -> PAL 576i -> 480i. */
    if (has720p)
    {
        pp.BackBufferWidth = 1280;
        pp.BackBufferHeight = 720;
        pp.Flags = D3DPRESENTFLAG_PROGRESSIVE | D3DPRESENTFLAG_WIDESCREEN;
    }
    else if (has480p)
    {
        pp.BackBufferWidth = 640;
        pp.BackBufferHeight = 480;
        pp.Flags = D3DPRESENTFLAG_PROGRESSIVE;
        if (vflags & XC_VIDEO_FLAGS_WIDESCREEN)
            pp.Flags |= D3DPRESENTFLAG_WIDESCREEN;
    }
    else if (palI && !pal60)
    {
        pp.BackBufferWidth = 640;
        pp.BackBufferHeight = 576;
        pp.Flags = D3DPRESENTFLAG_INTERLACED;
        if (vflags & XC_VIDEO_FLAGS_WIDESCREEN)
            pp.Flags |= D3DPRESENTFLAG_WIDESCREEN;
        pp.FullScreen_RefreshRateInHz = 50;
    }
    else
    {
        pp.BackBufferWidth = 640;
        pp.BackBufferHeight = 480;
        pp.Flags = D3DPRESENTFLAG_INTERLACED;
        if (vflags & XC_VIDEO_FLAGS_WIDESCREEN)
            pp.Flags |= D3DPRESENTFLAG_WIDESCREEN;
    }

    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    HRESULT hr = CreateDeviceWithAA(&pp);

    /* USB2XB's safe fallback: never leave a rejected HD mode black. */
    if (FAILED(hr) || !s_device)
    {
        pp.BackBufferWidth = 640;
        pp.BackBufferHeight = (palI && !pal60) ? 576 : 480;
        pp.Flags = D3DPRESENTFLAG_INTERLACED;
        if (vflags & XC_VIDEO_FLAGS_WIDESCREEN)
            pp.Flags |= D3DPRESENTFLAG_WIDESCREEN;
        pp.FullScreen_RefreshRateInHz = (palI && !pal60) ? 50 : 60;

        hr = CreateDeviceWithAA(&pp);
    }

    if (FAILED(hr) || !s_device)
    {
        if (s_device)
        {
            s_device->Release();
            s_device = NULL;
        }
        s_d3d->Release();
        s_d3d = NULL;
        return false;
    }

    g_bbWidth = (int)pp.BackBufferWidth;
    g_bbHeight = (int)pp.BackBufferHeight;

    s_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    s_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    s_device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

    /* OpenJazzXB waits here before starting its RXDK/Darkone83 splashes. */
    Sleep(250);
    return true;
}

void XbD3DShutdown()
{
    if (s_device)
    {
        s_device->SetTexture(0, NULL);
        s_device->Release();
        s_device = NULL;
    }

    if (s_d3d)
    {
        s_d3d->Release();
        s_d3d = NULL;
    }
}

D3DTexture* XbCreateLinearTexture(int width, int height)
{
    if (!s_device)
        return NULL;

    /* Same proven CPU-writable linear texture path used by OpenJazzXB/XbTyrian. */
    return D3DDevice_CreateTexture2(
        width, height,
        1, 1, 0,
        D3DFMT_LIN_X8R8G8B8,
        D3DRTYPE_TEXTURE);
}

void XbReleaseTexture(D3DTexture* texture)
{
    if (texture)
        texture->Release();
}

void XbUploadTexture(D3DTexture* texture,
    const void* pixels,
    int width,
    int height,
    int srcPitchBytes)
{
    if (!texture || !pixels)
        return;

    D3DLOCKED_RECT lr;
    if (FAILED(texture->LockRect(0, &lr, NULL, 0)))
        return;

    const unsigned char* src = (const unsigned char*)pixels;
    unsigned char* dst = (unsigned char*)lr.pBits;
    const int rowBytes = width * 4;

    for (int y = 0; y < height; ++y)
    {
        const unsigned char* srcRow = src + y * srcPitchBytes;
        unsigned char* dstRow = dst + y * lr.Pitch;
        for (int x = 0; x < rowBytes; ++x)
            dstRow[x] = srcRow[x];
    }

    texture->UnlockRect(0);
}

void XbBeginFrame()
{
    if (!s_device)
        return;

    /* Match USB2XB frame ownership: clear first, then open the scene. */
    s_device->Clear(
        0, NULL,
        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        0xFF000000,
        1.0f,
        0);
    s_device->BeginScene();
}

void XbDrawTexture(D3DTexture* texture,
    float sx0, float sy0, float sx1, float sy1,
    float dx0, float dy0, float dx1, float dy1,
    bool linearFilter)
{
    if (!s_device || !texture)
        return;

    SetBlitState(linearFilter);

    XbBlitVertex q[4] =
    {
        {dx0, dy0, 0.0f, 1.0f, sx0, sy0},
        {dx1, dy0, 0.0f, 1.0f, sx1, sy0},
        {dx0, dy1, 0.0f, 1.0f, sx0, sy1},
        {dx1, dy1, 0.0f, 1.0f, sx1, sy1},
    };

    s_device->SetTexture(0, texture);
    s_device->DrawPrimitiveUP(
        D3DPT_TRIANGLESTRIP,
        2,
        q,
        sizeof(XbBlitVertex));
}

void XbEndFrame()
{
    if (!s_device)
        return;

    s_device->EndScene();
    s_device->Present(NULL, NULL, NULL, NULL);
}
