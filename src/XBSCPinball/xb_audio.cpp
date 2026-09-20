#include "xb_xtl_compat.h"
#include <dsound.h>
#include "xb_audio.h"

#pragma comment(lib, "dsound.lib")

#define RING_BUFFER_COUNT 4
static int s_callbackBytes = 0;
static int s_ringBytes = 0;
static LPDIRECTSOUND s_ds = NULL;
static LPDIRECTSOUNDBUFFER s_buf = NULL;
static HANDLE s_thread = NULL;
static HANDLE s_stop = NULL;
static CRITICAL_SECTION s_cs;
static bool s_csInit = false;
static DWORD s_writeChunk = 0;
static XbAudioCallback s_callback = NULL;

static void Fill(void* p, DWORD bytes)
{
    if (!p || !bytes) return;
    if (s_callback) s_callback((unsigned char*)p, (int)bytes);
    else
    {
        unsigned char* dst = (unsigned char*)p;
        for (DWORD i = 0; i < bytes; ++i)
            dst[i] = 0;
    }
}

static DWORD WINAPI AudioThread(LPVOID)
{
    while (WaitForSingleObject(s_stop, 2) == WAIT_TIMEOUT)
    {
        DWORD playCursor = 0, writeCursor = 0;
        if (FAILED(s_buf->GetCurrentPosition(&playCursor, &writeCursor))) continue;
        const DWORD playChunk = playCursor / (DWORD)s_callbackBytes;
        while (s_writeChunk != playChunk)
        {
            void *p1 = NULL, *p2 = NULL;
            DWORD b1 = 0, b2 = 0;
            DWORD pos = s_writeChunk * (DWORD)s_callbackBytes;
            if (FAILED(s_buf->Lock(pos, (DWORD)s_callbackBytes, &p1, &b1, &p2, &b2, 0))) break;
            EnterCriticalSection(&s_cs);
            Fill(p1, b1); Fill(p2, b2);
            LeaveCriticalSection(&s_cs);
            s_buf->Unlock(p1, b1, p2, b2);
            s_writeChunk = (s_writeChunk + 1) % RING_BUFFER_COUNT;
        }
        Sleep(2);
    }
    return 0;
}

bool XbAudioInit(int channels, int freq, int samples, XbAudioCallback callback)
{
    s_callback = callback;
    s_callbackBytes = samples * channels * 2;
    s_ringBytes = s_callbackBytes * RING_BUFFER_COUNT;
    InitializeCriticalSection(&s_cs); s_csInit = true;

    if (FAILED(DirectSoundCreate(NULL, &s_ds, NULL))) return false;

    WAVEFORMATEX wfx;
    ZeroMemory(&wfx, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)channels;
    wfx.nSamplesPerSec = (DWORD)freq;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (WORD)(channels * 2);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    DSBUFFERDESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = 0;
    desc.dwBufferBytes = (DWORD)s_ringBytes;
    desc.lpwfxFormat = &wfx;
    if (FAILED(s_ds->CreateSoundBuffer(&desc, &s_buf, NULL))) return false;

    void *p1 = NULL, *p2 = NULL; DWORD b1 = 0, b2 = 0;
    if (SUCCEEDED(s_buf->Lock(0, (DWORD)s_ringBytes, &p1, &b1, &p2, &b2, 0)))
    {
        EnterCriticalSection(&s_cs); Fill(p1, b1); Fill(p2, b2); LeaveCriticalSection(&s_cs);
        s_buf->Unlock(p1, b1, p2, b2);
    }
    s_writeChunk = 0;
    s_buf->Play(0, 0, DSBPLAY_LOOPING);
    s_stop = CreateEvent(NULL, TRUE, FALSE, NULL);
    s_thread = CreateThread(NULL, 0, AudioThread, NULL, 0, NULL);
    if (s_thread) SetThreadPriority(s_thread, THREAD_PRIORITY_HIGHEST);
    return true;
}

void XbAudioDeinit()
{
    if (s_stop)
    {
        SetEvent(s_stop);
        if (s_thread) { WaitForSingleObject(s_thread, 2000); CloseHandle(s_thread); s_thread = NULL; }
        CloseHandle(s_stop); s_stop = NULL;
    }
    if (s_buf) { s_buf->Stop(); s_buf->Release(); s_buf = NULL; }
    if (s_ds) { s_ds->Release(); s_ds = NULL; }
    if (s_csInit) { DeleteCriticalSection(&s_cs); s_csInit = false; }
}
void XbAudioLock() { if (s_csInit) EnterCriticalSection(&s_cs); }
void XbAudioUnlock() { if (s_csInit) LeaveCriticalSection(&s_cs); }
