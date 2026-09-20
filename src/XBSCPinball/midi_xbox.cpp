#include <stdio.h>
#include <string.h>
#include "pch.h"
#include "midi.h"
#include "pb.h"
#include "xb_audio.h"

#ifdef _XBOX
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char*);
#endif

static void MidiDebug(const char* text)
{
#ifdef _XBOX
    OutputDebugStringA(text);
#else
    fputs(text, stderr);
#endif
}

/*
 * Xbox MIDI backend
 *
 * The Space Cadet core remains responsible for selecting MidiTracks::Track1,
 * Track2, or Track3.  This file only binds those logical slots to the real
 * assets and renders the selected MIDI through fmidi + TinySoundFont into the
 * existing 44.1 kHz stereo DirectSound mix callback.
 */

#define XB_ENABLE_FMIDI 1

#if XB_ENABLE_FMIDI
#define FMIDI_STATIC 1
#define FMIDI_DISABLE_DESCRIBE_API 1
#include "fmidi.h"

#define TSF_NO_STDIO 1
#define TSF_IMPLEMENTATION
#include "tsf.h"
#endif

struct XbMidiTrack
{
#if XB_ENABLE_FMIDI
    fmidi_smf_t* Smf;
    fmidi_player_t* Player;
#endif
    bool Valid;

    XbMidiTrack() :
#if XB_ENABLE_FMIDI
        Smf(NULL), Player(NULL),
#endif
        Valid(false)
    {
    }
};

std::vector<Mix_Music*> midi::LoadedTracks{};
Mix_Music* midi::track1 = NULL;
Mix_Music* midi::track2 = NULL;
Mix_Music* midi::track3 = NULL;
MidiTracks midi::active_track = MidiTracks::None;
MidiTracks midi::NextTrack = MidiTracks::None;
int midi::Volume = MIX_MAX_VOLUME;
bool midi::IsPlaying = false;
bool midi::MixOpen = false;

static int s_midiVolume = MIX_MAX_VOLUME;
static bool s_midiPlaying = false;
static XbMidiTrack* s_activeTrackPtr = NULL;

#if XB_ENABLE_FMIDI
static tsf* s_synth = NULL;
static bool s_loopRequested = false;
static bool s_loggedMix = false;
static bool s_loggedNote = false;

/*
 * Keep the SoundFont beside the Full Tilt music assets when possible.
 * Root fallbacks make existing test layouts continue to work.
 */
static bool LoadFileToMemory(const std::string& path, std::vector<unsigned char>& data)
{
    FILE* f = fopenu(path.c_str(), "rb");
    if (!f)
        return false;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return false;
    }

    const long size = ftell(f);
    if (size <= 0)
    {
        fclose(f);
        return false;
    }

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return false;
    }

    data.resize((size_t)size);
    const size_t readBytes = fread(&data[0], 1, (size_t)size, f);
    fclose(f);

    if (readBytes != (size_t)size)
    {
        data.clear();
        return false;
    }

    return true;
}

static tsf* LoadSoundFont()
{
    static const char* const candidates[] =
    {
        "SOUND\\gm.sf2",
        "SOUND\\GM.SF2",
        "gm.sf2",
        "GM.SF2"
    };

    std::vector<unsigned char> sf2Data;

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        const std::string path = pb::make_path_name(candidates[i]);
        sf2Data.clear();

        if (!LoadFileToMemory(path, sf2Data))
            continue;

        /*
         * TinySoundFont fully parses/copies the SoundFont during load, so the
         * temporary file buffer can be released when this function returns.
         */
        tsf* synth = tsf_load_memory(&sf2Data[0], (int)sf2Data.size());
        if (synth)
        {
            MidiDebug("SpaceCadetXB MIDI: SoundFont loaded\n");
            return synth;
        }
    }

    MidiDebug("SpaceCadetXB MIDI: SoundFont NOT FOUND / FAILED TO LOAD\n");
    return NULL;
}

static void ResetSynthChannels()
{
    if (!s_synth)
        return;

    tsf_reset(s_synth);

    /*
     * Pre-create all 16 channels.  Channel 10 (zero-based 9) is General MIDI
     * percussion.  Program changes from the MIDI stream replace these defaults.
     */
    for (int c = 0; c < 16; ++c)
        tsf_channel_set_presetnumber(s_synth, c, 0, c == 9);
}

static void MidiEvent(const fmidi_event_t* evt, void*)
{
    if (!s_synth || !evt || evt->type != fmidi_event_message || evt->datalen < 1)
        return;

    const unsigned status = evt->data[0];

    /* Ignore realtime/system messages here. */
    if ((status & 0x80) == 0 || status >= 0xF0)
        return;

    const int type = (status >> 4) & 0x0F;
    const int channel = status & 0x0F;
    const int data1 = evt->datalen > 1 ? (evt->data[1] & 0x7F) : 0;
    const int data2 = evt->datalen > 2 ? (evt->data[2] & 0x7F) : 0;

    switch (type)
    {
    case 0x8: /* Note Off */
        tsf_channel_note_off(s_synth, channel, data1);
        break;

    case 0x9: /* Note On; velocity zero means Note Off */
        if (data2 != 0)
        {
            if (!s_loggedNote)
            {
                s_loggedNote = true;
                MidiDebug("SpaceCadetXB MIDI: first Note On received\n");
            }
            tsf_channel_note_on(s_synth, channel, data1, data2 / 127.0f);
        }
        else
            tsf_channel_note_off(s_synth, channel, data1);
        break;

    case 0xB: /* Control Change */
        tsf_channel_midi_control(s_synth, channel, data1, data2);
        break;

    case 0xC: /* Program Change */
        tsf_channel_set_presetnumber(s_synth, channel, data1, channel == 9);
        break;

    case 0xE: /* Pitch Bend, 14-bit little-endian MIDI value */
        tsf_channel_set_pitchwheel(s_synth, channel, data1 | (data2 << 7));
        break;

    default:
        break;
    }
}

static void MidiFinished(void*)
{
    /* Desktop Space Cadet uses Mix_PlayMusic(..., -1): loop same track. */
    s_loopRequested = true;
}
#endif

void XbMidiMix(short* dstStereo, int frames)
{
#if XB_ENABLE_FMIDI
    if (!dstStereo || frames <= 0 || !s_synth || !s_midiPlaying || !s_activeTrackPtr)
        return;

    XbMidiTrack* track = s_activeTrackPtr;
    if (!track->Player)
        return;

    if (!s_loggedMix)
    {
        s_loggedMix = true;
        MidiDebug("SpaceCadetXB MIDI: DirectSound mixer is rendering MIDI\n");
    }

    /*
     * fmidi consumes elapsed seconds.  This callback is driven by the same
     * 44.1 kHz DirectSound stream as the SFX mixer, so audio time is the clock.
     */
    fmidi_player_tick(track->Player, (double)frames / 44100.0);

    if (s_loopRequested || !fmidi_player_running(track->Player))
    {
        s_loopRequested = false;
        fmidi_player_rewind(track->Player);
        ResetSynthChannels();
        fmidi_player_start(track->Player);
    }

    /* Avoid heap allocation inside the audio callback. */
    static short midiBuffer[1024 * 2];
    const int volume = s_midiVolume;
    int rendered = 0;

    while (rendered < frames)
    {
        int count = frames - rendered;
        if (count > 1024)
            count = 1024;

        tsf_render_short(s_synth, midiBuffer, count, 0);

        short* output = dstStereo + rendered * 2;
        const int sampleCount = count * 2;

        for (int i = 0; i < sampleCount; ++i)
        {
            int value = (int)output[i] + ((int)midiBuffer[i] * volume / MIX_MAX_VOLUME);
            if (value > 32767)
                value = 32767;
            else if (value < -32768)
                value = -32768;
            output[i] = (short)value;
        }

        rendered += count;
    }
#else
    (void)dstStereo;
    (void)frames;
#endif
}

void midi::music_play()
{
    /* Mirror the core/desktop behavior: do not invent a default track here. */
    if (!IsPlaying)
    {
        IsPlaying = true;
        s_midiPlaying = true;
        play_track(NextTrack, true);
        NextTrack = MidiTracks::None;
    }
}

void midi::music_stop()
{
    if (IsPlaying)
    {
        /*
         * The DirectSound callback renders fmidi/TSF under XbAudio's critical
         * section. Serialize stop/reset state against that callback so pause or
         * a core-requested track transition cannot touch the player mid-render.
         */
        XbAudioLock();

        IsPlaying = false;
        s_midiPlaying = false;
        NextTrack = active_track;
        StopPlayback();

        XbAudioUnlock();
    }
}

void midi::StopPlayback()
{
    if (active_track != MidiTracks::None)
    {
#if XB_ENABLE_FMIDI
        XbMidiTrack* track = (XbMidiTrack*)TrackToMidi(active_track);
        if (track && track->Player)
            fmidi_player_stop(track->Player);

        ResetSynthChannels();
        s_loopRequested = false;
#endif
        active_track = MidiTracks::None;
        s_activeTrackPtr = NULL;
    }
}

int midi::music_init(bool mixOpen, int volume)
{
    MixOpen = mixOpen;
    SetVolume(volume);

    s_midiPlaying = false;
    s_activeTrackPtr = NULL;
    active_track = MidiTracks::None;
    NextTrack = MidiTracks::None;
    IsPlaying = false;
    track1 = track2 = track3 = NULL;

    if (!MixOpen)
        return 0;

#if XB_ENABLE_FMIDI
    s_synth = LoadSoundFont();
    if (!s_synth)
        return 0;

    tsf_set_output(s_synth, TSF_STEREO_INTERLEAVED, 44100, 0.0f);

    /* Pre-allocation avoids voice heap growth in the DirectSound thread. */
    if (!tsf_set_max_voices(s_synth, 64))
    {
        tsf_close(s_synth);
        s_synth = NULL;
        return 0;
    }

    ResetSynthChannels();
#endif

    /*
     * Xbox package contract:
     *   Track1 -> SOUND\\TABA1.MID
     *   Track2 -> SOUND\\TABA2.MID
     *   Track3 -> SOUND\\TABA3.MID
     *
     * The core game still decides WHICH MidiTracks slot to play and when.
     * Do not make asset binding depend on DAT filename / FullTiltMode here:
     * the Xbox package has explicitly standardized on the TABA set.
     */
    track1 = load_track("TABA1");
    track2 = load_track("TABA2");
    track3 = load_track("TABA3");

    if (!track1)
    {
        MidiDebug("SpaceCadetXB MIDI: TABA1.MID failed to load\n");
#if XB_ENABLE_FMIDI
        tsf_close(s_synth);
        s_synth = NULL;
#endif
        return 0;
    }

    if (!track2)
        MidiDebug("SpaceCadetXB MIDI: warning - TABA2.MID failed to load\n");
    if (!track3)
        MidiDebug("SpaceCadetXB MIDI: warning - TABA3.MID failed to load\n");

    MidiDebug("SpaceCadetXB MIDI: backend initialized\n");
    return 1;
}

void midi::music_shutdown()
{
    music_stop();

    for (size_t i = 0; i < LoadedTracks.size(); ++i)
    {
        XbMidiTrack* track = (XbMidiTrack*)LoadedTracks[i];
#if XB_ENABLE_FMIDI
        if (track->Player)
            fmidi_player_free(track->Player);
        if (track->Smf)
            fmidi_smf_free(track->Smf);
#endif
        delete track;
    }

    LoadedTracks.clear();

#if XB_ENABLE_FMIDI
    if (s_synth)
    {
        tsf_close(s_synth);
        s_synth = NULL;
    }
#endif

    track1 = track2 = track3 = NULL;
    active_track = MidiTracks::None;
    NextTrack = MidiTracks::None;
    s_activeTrackPtr = NULL;
    s_midiPlaying = false;
    IsPlaying = false;
    MixOpen = false;
}

void midi::SetVolume(int volume)
{
    if (volume < 0)
        volume = 0;
    if (volume > MIX_MAX_VOLUME)
        volume = MIX_MAX_VOLUME;

    Volume = volume;
    s_midiVolume = volume;
}

Mix_Music* midi::load_track(std::string fileName)
{
    if (!MixOpen || pb::quickFlag)
        return NULL;

    /*
     * The Xbox runtime package always keeps MIDI under SOUND, independent of
     * how the DAT itself was named or which compatibility flag was selected.
     */
    if (fileName.compare(0, 6, "SOUND\\") != 0 &&
        fileName.compare(0, 6, "sound\\") != 0)
    {
        fileName.insert(0, 1, PathSeparator);
        fileName.insert(0, "SOUND");
    }

    return load_track_sub(fileName, true);
}

Mix_Music* midi::load_track_sub(std::string fileName, bool isMidi)
{
    /* The Xbox package uses converted Standard MIDI files only. */
    if (!isMidi)
        return NULL;

    fileName += ".MID";

    std::string path;
    FILE* file = NULL;

    for (int pass = 0; pass < 2; ++pass)
    {
        if (pass == 1)
        {
            std::transform(
                fileName.begin(),
                fileName.end(),
                fileName.begin(),
                [](unsigned char c) { return (char)tolower(c); });
        }

        path = pb::make_path_name(fileName);
        file = fopenu(path.c_str(), "rb");
        if (file)
        {
            fclose(file);
            file = NULL;
            break;
        }
    }

    if (path.empty())
        return NULL;

    /* Re-check after the two path attempts; the last failed path must not load. */
    file = fopenu(path.c_str(), "rb");
    if (!file)
        return NULL;
    fclose(file);

    XbMidiTrack* track = new XbMidiTrack();

#if XB_ENABLE_FMIDI
    std::vector<unsigned char> midiData;
    if (!LoadFileToMemory(path, midiData))
    {
        delete track;
        return NULL;
    }

    track->Smf = fmidi_smf_mem_read(&midiData[0], midiData.size());
    if (!track->Smf)
    {
        MidiDebug("SpaceCadetXB MIDI: fmidi failed to parse MIDI file\n");
        delete track;
        return NULL;
    }

    track->Player = fmidi_player_new(track->Smf);
    if (!track->Player)
    {
        fmidi_smf_free(track->Smf);
        track->Smf = NULL;
        delete track;
        return NULL;
    }

    fmidi_player_event_callback(track->Player, MidiEvent, NULL);
    fmidi_player_finish_callback(track->Player, MidiFinished, NULL);
    track->Valid = true;
#else
    track->Valid = false;
#endif

    LoadedTracks.push_back((Mix_Music*)track);
    return (Mix_Music*)track;
}

bool midi::play_track(MidiTracks track, bool replay)
{
    Mix_Music* music = TrackToMidi(track);
    if (!music || (!replay && active_track == track))
        return false;

    /*
     * XbAudio's callback owns fmidi_player_tick() + tsf_render_short().
     * Hold the same lock while rewinding/stopping/resetting/starting.
     */
    XbAudioLock();

    StopPlayback();

    if (!IsPlaying)
    {
        NextTrack = track;
        XbAudioUnlock();
        return false;
    }

#if XB_ENABLE_FMIDI
    XbMidiTrack* xbTrack = (XbMidiTrack*)music;
    if (!xbTrack->Player || !s_synth)
    {
        XbAudioUnlock();
        return false;
    }

    ResetSynthChannels();
    s_loopRequested = false;
    fmidi_player_rewind(xbTrack->Player);
    fmidi_player_start(xbTrack->Player);
#endif

    active_track = track;
    s_activeTrackPtr = (XbMidiTrack*)music;

    XbAudioUnlock();

    switch (track)
    {
    case MidiTracks::Track1: MidiDebug("SpaceCadetXB MIDI: playing Track1 / TABA1\n"); break;
    case MidiTracks::Track2: MidiDebug("SpaceCadetXB MIDI: playing Track2 / TABA2\n"); break;
    case MidiTracks::Track3: MidiDebug("SpaceCadetXB MIDI: playing Track3 / TABA3\n"); break;
    default: break;
    }

    return true;
}

MidiTracks midi::get_active_track()
{
    return IsPlaying ? active_track : NextTrack;
}

Mix_Music* midi::TrackToMidi(MidiTracks track)
{
    switch (track)
    {
    default:
    case MidiTracks::None:
        return NULL;
    case MidiTracks::Track1:
        return track1;
    case MidiTracks::Track2:
        return track2;
    case MidiTracks::Track3:
        return track3;
    }
}

/* Xbox package uses .MID directly; MDS conversion remains a desktop concern. */
std::vector<uint8_t>* midi::MdsToMidi(std::string)
{
    return NULL;
}
