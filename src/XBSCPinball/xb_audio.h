#pragma once

// Public audio bridge: deliberately RXDK/XTL-free.
// The implementation in xb_audio.cpp owns DirectSound, WinBase threading,
// synchronization, and all Xbox-specific types.
typedef void (*XbAudioCallback)(unsigned char* stream, int bytes);

bool XbAudioInit(int channels, int freq, int samples, XbAudioCallback callback);
void XbAudioDeinit();
void XbAudioLock();
void XbAudioUnlock();
