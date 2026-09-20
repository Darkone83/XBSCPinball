#pragma once

// Keep RXDK/XTL out of this public interface.
// STL-facing game code includes this header, and pulling <xtl.h> here mixes
// RXDK winbase.h interlocked declarations with the modern MSVC STL intrinsic
// layer. The concrete D3DTexture definition is only needed by xb_d3d.cpp and
// other XTL-only backend translation units.
struct D3DTexture;

extern int g_bbWidth;
extern int g_bbHeight;

bool XbD3DInit();
void XbD3DShutdown();
D3DTexture* XbCreateLinearTexture(int width, int height);
void XbReleaseTexture(D3DTexture* texture);
void XbUploadTexture(D3DTexture* texture, const void* pixels, int width, int height, int srcPitchBytes);
void XbBeginFrame();
void XbDrawTexture(D3DTexture* texture,
    float sx0, float sy0, float sx1, float sy1,
    float dx0, float dy0, float dx1, float dy1,
    bool linearFilter);
void XbEndFrame();
