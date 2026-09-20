/*
 * Xbox implementation of the desktop winmain facade.
 *
 * IMPORTANT: keep this translation unit STL-facing and XTL-free.  RXDK's
 * <xtl.h>/<winbase.h> interlocked declarations conflict with the modern
 * MSVC STL intrinsic layer pulled in by pch.h.  This file only needs two
 * Xbox kernel APIs, so import them directly instead of including <xtl.h>.
 * This is the same separation used by the known-good OpenJazzXB port.
 */
extern "C"
{
    __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
    __declspec(dllimport) void __stdcall OutputDebugStringA(const char* text);
}

#include "pch.h"
#include "winmain.h"
#include "options.h"
#include "pb.h"
#include "fullscrn.h"
#include "midi.h"
#include "Sound.h"
#include "render.h"

bool winmain::single_step = false;
SDL_Window* winmain::MainWindow = NULL;
SDL_Renderer* winmain::Renderer = NULL;
ImGuiIO* winmain::ImIO = NULL;
bool winmain::LaunchBallEnabled = true;
bool winmain::HighScoresEnabled = true;
bool winmain::DemoActive = false;
int winmain::MainMenuHeight = 0;
int winmain::return_value = 0;
int winmain::mouse_down = 0;
int winmain::last_mouse_x = 0;
int winmain::last_mouse_y = 0;
bool winmain::no_time_loss = false;
bool winmain::activated = true;
bool winmain::bQuit = false;
bool winmain::has_focus = true;
bool winmain::DispGRhistory = false;
bool winmain::DispFrameRate = false;
std::vector<float> winmain::gfrDisplay{};
std::string winmain::FpsDetails{};
std::string winmain::PrevSdlError{};
bool winmain::restart = false;
bool winmain::ShowAboutDialog = false;
bool winmain::ShowImGuiDemo = false;
bool winmain::ShowSpriteViewer = false;
bool winmain::ShowExitPopup = false;
double winmain::UpdateToFrameRatio = 2.0;
winmain::DurationMs winmain::TargetFrameTime = DurationMs(1000.0 / options::DefUps);
optionsStruct& winmain::Options = options::Options;
winmain::DurationMs winmain::SpinThreshold = DurationMs(0.005);
WelfordState winmain::SleepState{};
unsigned winmain::PrevSdlErrorCount = 0;
unsigned winmain::gfrOffset = 0;
float winmain::gfrWindow = 5.0f;
int winmain::CursorIdleCounter = 0;

void winmain::end_pause()
{
    if (single_step) pb::pause_continue();
}

void winmain::new_game()
{
    end_pause();
    pb::replay_level(false);
}

void winmain::pause(bool toggle)
{
    if (toggle || !single_step) pb::pause_continue();
}

void winmain::Restart()
{
    bQuit = true;
    restart = true;
}

void winmain::UpdateFrameRate()
{
    int fps = Options.FramesPerSecond.V;
    int ups = Options.UpdatesPerSecond.V;
    if (fps < 1) fps = 60;
    if (ups < fps) ups = fps;
    UpdateToFrameRatio = (double)ups / (double)fps;
    TargetFrameTime = DurationMs(1000.0 / (double)ups);
}

void winmain::HandleGameBinding(GameBindings binding, bool)
{
    switch (binding)
    {
    case GameBindings::TogglePause: pause(); break;
    case GameBindings::NewGame: new_game(); break;
    case GameBindings::ToggleSounds: options::toggle(Menu1::Sounds); break;
    case GameBindings::ToggleMusic: options::toggle(Menu1::Music); break;
    case GameBindings::Exit: bQuit = true; break;
    default: break;
    }
}

bool winmain::ExitRequested() { return bQuit; }
int winmain::ProcessWindowMessages() { return bQuit ? 0 : 1; }
int winmain::event_handler(const SDL_Event*) { return 0; }
void winmain::a_dialog() {}
void winmain::RenderUi() {}
void winmain::RenderFrameTimeDialog() {}
void winmain::HybridSleep(DurationMs ms) { if (ms.count() > 0.0) Sleep((unsigned long)ms.count()); }
void winmain::MainLoop() {}
void winmain::ImGuiMenuItemWShortcut(GameBindings, bool) {}

[[noreturn]] void winmain::memalloc_failure()
{
    OutputDebugStringA("SpaceCadetXB: out of memory\n");
    for (;;) Sleep(1000);
}

int winmain::WinMain(LPCSTR)
{
    return 0; // Xbox entry point lives in xbox_main.cpp.
}
