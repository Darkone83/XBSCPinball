#ifdef _XBOX
extern "C" {
    __declspec(dllimport) unsigned long __stdcall GetTickCount(void);
    __declspec(dllimport) void __stdcall Sleep(unsigned long);
    __declspec(dllimport) void __stdcall OutputDebugStringA(const char*);
}
#endif

#include "pch.h"
#include "pb.h"
#include "options.h"
#include "fullscrn.h"
#include "midi.h"
#include "Sound.h"
#include "render.h"
#include "winmain.h"
#include "xb_d3d.h"
#include "xb_input.h"
#include "xb_splash.h"
#include "xb_exit.h"

static void Debug(const char* s) { OutputDebugStringA(s); }

void __cdecl main()
{
    Debug("SpaceCadetXB: boot\n");
    if (!XbD3DInit())
    {
        Debug("SpaceCadetXB: D3D init failed\n");
        XbReturnToDashboard();
    }

    // Input is initialized before the splash sequence so the final controller
    // help screen can wait for A/Start without initializing XInput twice.
    XbInputInit();
    XbShowSplashes();

    if (XbInputExitRequested())
    {
        XbInputShutdown();
        XbD3DShutdown();
        XbReturnToDashboard();
    }

    options::InitPrimary();
    std::vector<const char*> searchPaths;
    searchPaths.push_back("D:\\");
    searchPaths.push_back("");
    pb::SelectDatFile(searchPaths);
    options::InitSecondary();

    Sound::Init(true, options::Options.SoundChannels, options::Options.Sounds, options::Options.SoundVolume);

    const bool midiReady = midi::music_init(true, options::Options.MusicVolume) != 0;
    if (!midiReady)
    {
        Debug("SpaceCadetXB: MIDI initialization failed\n");
    }
    else
    {
        /*
         * Earlier builds persisted Music=0 to pinball.ini whenever MIDI init
         * failed.  With no runtime options menu in this build, that stale value
         * can permanently suppress the now-working backend.  Re-enable music
         * once the backend and assets validate successfully.
         */
        if (!options::Options.Music)
            Debug("SpaceCadetXB: re-enabling music after successful MIDI init\n");
        options::Options.Music = true;
    }

    if (pb::init())
    {
        pb::ShowMessageBox(SDL_MESSAGEBOX_ERROR, "Could not load Space Cadet game data",
            "Verify the Full Tilt Space Cadet DAT/WAV assets and SOUND folder are installed beside default.xbe.");
        Sound::Close();
        midi::music_shutdown();
        XbInputShutdown();
        XbD3DShutdown();
        XbReturnToDashboard();
    }

    fullscrn::init();
    pb::reset_table();
    pb::firsttime_setup();
    pb::replay_level(false);
    render::PresentVScreen();

    const double updateMs = 1000.0 / (double)options::Options.UpdatesPerSecond.V;
    const double presentMs = 1000.0 / (double)options::Options.FramesPerSecond.V;
    unsigned long previous = GetTickCount();
    unsigned long lastPresent = previous;
    double accumulator = 0.0;

    while (!XbInputExitRequested() && !winmain::ExitRequested())
    {
        XbInputPump();

        unsigned long now = GetTickCount();
        unsigned long elapsed = now - previous;
        previous = now;
        if (elapsed > 100) elapsed = 100;
        accumulator += (double)elapsed;

        int catchup = 0;
        while (accumulator >= updateMs && catchup < 12)
        {
            if (!winmain::single_step)
                pb::frame((float)updateMs);
            accumulator -= updateMs;
            ++catchup;
        }
        if (catchup == 12 && accumulator > updateMs * 12.0)
            accumulator = 0.0;

        if ((double)(now - lastPresent) >= presentMs)
        {
            render::PresentVScreen();
            lastPresent = now;
        }
        Sleep(1);
    }

    Debug("SpaceCadetXB: shutdown\n");
    // pb::uninit() flushes game state/high scores first; options then saves
    // the remaining persistent Xbox settings.
    pb::uninit();
    options::uninit();
    Sound::Close();
    midi::music_shutdown();
    fullscrn::shutdown();
    XbInputShutdown();
    XbD3DShutdown();
    XbReturnToDashboard();
}
