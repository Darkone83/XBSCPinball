#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include "pch.h"
#include "Sound.h"
#include "options.h"
#include "xb_audio.h"

struct XbWave
{
    std::vector<short> Samples; // 44.1kHz stereo interleaved
};

struct ActiveWave
{
    XbWave* Wave;
    unsigned Frame;
    bool Active;
    ActiveWave() : Wave(NULL), Frame(0), Active(false) {}
};

int Sound::num_channels = 8;
bool Sound::enabled_flag = false;
std::vector<ChannelInfo> Sound::Channels{};
int Sound::Volume = MIX_MAX_VOLUME;
bool Sound::MixOpen = false;
static std::vector<ActiveWave> s_active;
static int s_soundVolume = MIX_MAX_VOLUME;
static bool s_soundEnabled = false;
static bool s_soundPaused = false;

extern void XbMidiMix(short* dstStereo, int frames);

static short Clamp16(int v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (short)v;
}

static void AudioCallback(unsigned char* bytes, int byteCount)
{
    short* out = (short*)bytes;
    const int frames = byteCount / 4;
    memset(out, 0, byteCount);

    // Music first, SFX are mixed over it.
    XbMidiMix(out, frames);

    if (s_soundEnabled && !s_soundPaused && !Sound::Channels.empty())
    {
        const int master = s_soundVolume;
        for (int c = 0; c < (int)s_active.size(); ++c)
        {
            ActiveWave& ch = s_active[c];
            if (!ch.Active || !ch.Wave) continue;
            const unsigned totalFrames = (unsigned)(ch.Wave->Samples.size() / 2);
            for (int f = 0; f < frames && ch.Frame < totalFrames; ++f, ++ch.Frame)
            {
                int sl = ch.Wave->Samples[ch.Frame * 2 + 0];
                int sr = ch.Wave->Samples[ch.Frame * 2 + 1];
                sl = sl * master / MIX_MAX_VOLUME;
                sr = sr * master / MIX_MAX_VOLUME;
                out[f * 2 + 0] = Clamp16((int)out[f * 2 + 0] + sl);
                out[f * 2 + 1] = Clamp16((int)out[f * 2 + 1] + sr);
            }
            if (ch.Frame >= totalFrames) ch.Active = false;
        }
    }
}

void Sound::Init(bool, int channels, bool enableFlag, int volume)
{
    Volume = volume;
    s_soundVolume = volume;
    SetChannels(channels);
    enabled_flag = enableFlag;
    s_soundEnabled = enableFlag;
    s_soundPaused = false;
    MixOpen = XbAudioInit(2, 44100, 1024, AudioCallback);
}

void Sound::Enable(bool enableFlag)
{
    XbAudioLock();
    enabled_flag = enableFlag;
    s_soundEnabled = enableFlag;
    if (!enabled_flag)
        for (size_t i = 0; i < s_active.size(); ++i) s_active[i].Active = false;
    XbAudioUnlock();
}

void Sound::Activate() { s_soundPaused = false; }
void Sound::Deactivate() { s_soundPaused = true; }

void Sound::Close()
{
    Enable(false);
    XbAudioDeinit();
    MixOpen = false;
    Channels.clear();
    s_active.clear();
}

void Sound::PlaySound(Mix_Chunk* wavePtr, int time, TPinballComponent*, const char*)
{
    if (!MixOpen || !enabled_flag || !wavePtr || s_active.empty()) return;
    XbAudioLock();
    int slot = -1;
    for (int i = 0; i < (int)s_active.size(); ++i)
        if (!s_active[i].Active) { slot = i; break; }
    if (slot < 0)
    {
        slot = 0;
        for (int i = 1; i < (int)Channels.size(); ++i)
            if (Channels[i].TimeStamp < Channels[slot].TimeStamp) slot = i;
    }
    s_active[slot].Wave = (XbWave*)wavePtr;
    s_active[slot].Frame = 0;
    s_active[slot].Active = true;
    Channels[slot].TimeStamp = time;
    XbAudioUnlock();
}

static bool ReadU32(FILE* f, unsigned& value)
{
    unsigned char b[4]; if (fread(b, 1, 4, f) != 4) return false;
    value = (unsigned)b[0] | ((unsigned)b[1] << 8) | ((unsigned)b[2] << 16) | ((unsigned)b[3] << 24);
    return true;
}
static bool ReadU16(FILE* f, unsigned short& value)
{
    unsigned char b[2]; if (fread(b, 1, 2, f) != 2) return false;
    value = (unsigned short)((unsigned)b[0] | ((unsigned)b[1] << 8)); return true;
}

Mix_Chunk* Sound::LoadWaveFile(const std::string& path)
{
    FILE* f = fopenu(path.c_str(), "rb");
    if (!f) return NULL;
    char riff[4], wave[4]; unsigned riffSize = 0;
    if (fread(riff, 1, 4, f) != 4 || memcmp(riff, "RIFF", 4) || !ReadU32(f, riffSize) ||
        fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4)) {
        fclose(f); return NULL;
    }

    unsigned short format = 0, channels = 0, bits = 0;
    unsigned sampleRate = 0;
    std::vector<unsigned char> pcm;
    while (!feof(f))
    {
        char id[4]; unsigned size = 0;
        if (fread(id, 1, 4, f) != 4 || !ReadU32(f, size)) break;
        if (!memcmp(id, "fmt ", 4))
        {
            unsigned short blockAlign = 0; unsigned avg = 0;
            ReadU16(f, format); ReadU16(f, channels); ReadU32(f, sampleRate); ReadU32(f, avg);
            ReadU16(f, blockAlign); ReadU16(f, bits);
            if (size > 16) fseek(f, size - 16, SEEK_CUR);
        }
        else if (!memcmp(id, "data", 4))
        {
            pcm.resize(size);
            if (size) fread(&pcm[0], 1, size, f);
        }
        else fseek(f, size, SEEK_CUR);
        if (size & 1) fseek(f, 1, SEEK_CUR);
    }
    fclose(f);
    if (format != 1 || (channels != 1 && channels != 2) || (bits != 8 && bits != 16) || !sampleRate || pcm.empty())
        return NULL;

    const unsigned bytesPerSample = bits / 8;
    const unsigned srcFrames = (unsigned)pcm.size() / (channels * bytesPerSample);
    const unsigned dstFrames = (unsigned)(((unsigned long long)srcFrames * 44100ULL + sampleRate - 1) / sampleRate);
    XbWave* out = new XbWave();
    out->Samples.resize(dstFrames * 2);
    for (unsigned d = 0; d < dstFrames; ++d)
    {
        unsigned s = (unsigned)(((unsigned long long)d * sampleRate) / 44100ULL);
        if (s >= srcFrames) s = srcFrames - 1;
        for (unsigned ch = 0; ch < 2; ++ch)
        {
            unsigned srcCh = channels == 1 ? 0 : ch;
            short sample;
            if (bits == 16)
            {
                const unsigned off = (s * channels + srcCh) * 2;
                sample = (short)((unsigned)pcm[off] | ((unsigned)pcm[off + 1] << 8));
            }
            else
            {
                sample = (short)(((int)pcm[s * channels + srcCh] - 128) << 8);
            }
            out->Samples[d * 2 + ch] = sample;
        }
    }
    return (Mix_Chunk*)out;
}

void Sound::FreeSound(Mix_Chunk* wave)
{
    if (!wave) return;
    XbAudioLock();
    for (size_t i = 0; i < s_active.size(); ++i)
        if (s_active[i].Wave == (XbWave*)wave) s_active[i].Active = false;
    XbAudioUnlock();
    delete (XbWave*)wave;
}

void Sound::SetChannels(int channels)
{
    if (channels <= 0) channels = 8;
    if (channels > 32) channels = 32;
    num_channels = channels;
    Channels.resize(num_channels);
    s_active.resize(num_channels);
}

void Sound::SetVolume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > MIX_MAX_VOLUME) volume = MIX_MAX_VOLUME;
    Volume = volume;
    s_soundVolume = volume;
}
