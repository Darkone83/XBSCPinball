#include "pch.h"
#include "options.h"
#include "fullscrn.h"
#include "midi.h"
#include "render.h"
#include "Sound.h"
#include "translations.h"
#include "winmain.h"

constexpr int options::MaxUps, options::MaxFps, options::MinUps, options::MinFps, options::DefUps, options::DefFps;
constexpr int options::MaxSoundChannels, options::MinSoundChannels, options::DefSoundChannels;
constexpr int options::MaxVolume, options::MinVolume, options::DefVolume;
std::unordered_map<std::string, std::string> options::settings{};
bool options::ShowDialog = false;
GameInput* options::ControlWaitingForInput = nullptr;
std::vector<OptionBase*> options::AllOptions{};

optionsStruct options::Options
{
    {
        {"Left Flipper key", Msg::KEYMAPPER_FlipperL, {InputTypes::Keyboard, SDLK_z}, {}, {InputTypes::GameController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER}},
        {"Right Flipper key", Msg::KEYMAPPER_FlipperR, {InputTypes::Keyboard, SDLK_SLASH}, {}, {InputTypes::GameController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}},
        {"Plunger key", Msg::KEYMAPPER_Plunger, {InputTypes::Keyboard, SDLK_SPACE}, {}, {InputTypes::GameController, SDL_CONTROLLER_BUTTON_A}},
        {"Left Table Bump key", Msg::KEYMAPPER_BumpLeft, {InputTypes::Keyboard, SDLK_x}, {}, {InputTypes::GameController, SDL_CONTROLLER_BUTTON_DPAD_LEFT}},
        {"Right Table Bump key", Msg::KEYMAPPER_BumpRight, {InputTypes::Keyboard, SDLK_PERIOD}, {}, {InputTypes::GameController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT}},
        {"Bottom Table Bump key", Msg::KEYMAPPER_BumpBottom, {InputTypes::Keyboard, SDLK_UP}, {}, {InputTypes::GameController, SDL_CONTROLLER_BUTTON_DPAD_UP}},
        {"New Game", Msg::Menu1_New_Game, {InputTypes::Keyboard, SDLK_F2}, {}, {}},
        {"Toggle Pause", Msg::Menu1_Pause_Resume_Game, {InputTypes::Keyboard, SDLK_F3}, {}, {InputTypes::GameController, SDL_CONTROLLER_BUTTON_START}},
        {"Toggle FullScreen", Msg::Menu1_Full_Screen, {InputTypes::Keyboard, SDLK_F4}, {}, {}},
        {"Toggle Sounds", Msg::Menu1_Sounds, {InputTypes::Keyboard, SDLK_F5}, {}, {}},
        {"Toggle Music", Msg::Menu1_Music, {InputTypes::Keyboard, SDLK_F6}, {}, {}},
        {"Show Control Dialog", Msg::Menu1_Player_Controls, {InputTypes::Keyboard, SDLK_F8}, {}, {}},
        {"Toggle Menu Display", Msg::Menu1_ToggleShowMenu, {InputTypes::Keyboard, SDLK_F9}, {}, {}},
        {"Exit", Msg::Menu1_Exit, {InputTypes::Keyboard, SDLK_ESCAPE}, {}, {InputTypes::GameController, SDL_CONTROLLER_BUTTON_BACK}},
    },
    {"Sounds", true},
    {"Music", true},
    {"FullScreen", true},
    {"Players", 1},
    {"Screen Resolution", 0},
    {"UI Scale", 1.0f},
    {"Uniform scaling", true},
    {"Linear Filtering", true},
    {"Frames Per Second", DefFps},
    {"Updates Per Second", DefUps},
    {"ShowMenu", false},
    {"Uncapped Updates Per Second", false},
    {"Sound Channels", DefSoundChannels},
    {"HybridSleep", false},
    {"Prefer 3DPB Game Data", false},
    {"Integer Scaling", false},
    {"Sound Volume", DefVolume},
    {"Music Volume", DefVolume},
    {"Stereo Sound Effects", false},
    {"Debug Overlay", false},
    {"Debug Overlay Grid", false}, {"Debug Overlay All Edges", false}, {"Debug Overlay Ball Position", false},
    {"Debug Overlay Ball Edges", false}, {"Debug Overlay Collision Mask", false}, {"Debug Overlay Sprites", false},
    {"Debug Overlay Sounds", false}, {"Debug Overlay Ball Depth Grid", false}, {"Debug Overlay AABB", false},
    {"FontFileName", ""}, {"Language", "en"}, {"Hide Cursor", true},
};

static const char* SettingsPath = "D:\\pinball.ini";

static void LoadSettingsFile(std::unordered_map<std::string, std::string>& target)
{
    FILE* f = fopen(SettingsPath, "rb"); if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        char* nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        char* eq = strchr(line, '='); if (!eq) continue;
        *eq++ = 0; target[line] = eq;
    }
    fclose(f);
}
static void SaveSettingsFile(const std::unordered_map<std::string, std::string>& source)
{
    FILE* f = fopen(SettingsPath, "wb"); if (!f) return;
    for (auto it = source.begin(); it != source.end(); ++it)
    {
        fwrite(it->first.data(), 1, it->first.size(), f);
        fwrite("=", 1, 1, f);
        fwrite(it->second.data(), 1, it->second.size(), f);
        fwrite("\r\n", 1, 2, f);
    }
    fclose(f);
}

static std::string XbIntString(int value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return std::string(buffer);
}

static std::string XbFloatString(float value)
{
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));
    return std::string(buffer);
}

void options::InitPrimary()
{
    LoadSettingsFile(settings);
    for (auto opt : AllOptions) opt->Load();
    PostProcessOptions();
}
void options::InitSecondary() { fullscrn::SetResolution(0); }
void options::uninit() { for (auto opt : AllOptions) opt->Save(); SaveSettingsFile(settings); }

const std::string& options::GetSetting(const std::string& key, const std::string& def)
{
    auto it = settings.find(key); if (it != settings.end()) return it->second;
    return settings.emplace(key, def).first->second;
}
void options::SetSetting(const std::string& key, const std::string& value) { settings[key] = value; }
int options::get_int(LPCSTR n, int d) { return atoi(GetSetting(n, XbIntString(d)).c_str()); }
void options::set_int(LPCSTR n, int d) { SetSetting(n, XbIntString(d)); }
float options::get_float(LPCSTR n, float d) { return (float)atof(GetSetting(n, XbFloatString(d)).c_str()); }
void options::set_float(LPCSTR n, float d) { SetSetting(n, XbFloatString(d)); }
void options::GetInput(const std::string&, GameInput (&)[3]) {}
void options::SetInput(const std::string&, GameInput (&)[3]) {}

void options::toggle(Menu1 id)
{
    switch (id)
    {
    case Menu1::Sounds: Options.Sounds ^= true; Sound::Enable(Options.Sounds); break;
    case Menu1::Music: Options.Music ^= true; if (Options.Music) midi::music_play(); else midi::music_stop(); break;
    case Menu1::OnePlayer: case Menu1::TwoPlayers: case Menu1::ThreePlayers: case Menu1::FourPlayers:
        Options.Players = (int)id - (int)Menu1::OnePlayer + 1; break;
    case Menu1::WindowLinearFilter: Options.LinearFiltering ^= true; render::recreate_screen_texture(); break;
    default: break;
    }
}
void options::InputDown(GameInput input) { if (ControlWaitingForInput) { *ControlWaitingForInput = input; ControlWaitingForInput = nullptr; } }
void options::ShowControlDialog() {}
void options::RenderControlDialog() {}
std::vector<GameBindings> options::MapGameInput(GameInput key)
{
    std::vector<GameBindings> r;
    for (auto id = GameBindings::Min; id < GameBindings::Max; id++)
        for (auto& v : Options.Key[~id].Inputs) if (key == v) { r.push_back(id); break; }
    return r;
}
void options::ResetAllOptions() { for (auto o : AllOptions) o->Reset(); PostProcessOptions(); }
void options::PostProcessOptions()
{
    Options.FramesPerSecond = Clamp(Options.FramesPerSecond.V, MinFps, MaxFps);
    Options.UpdatesPerSecond = Clamp(Options.UpdatesPerSecond.V, MinUps, MaxUps);
    Options.UpdatesPerSecond = std::max(Options.UpdatesPerSecond.V, Options.FramesPerSecond.V);
    Options.SoundChannels = Clamp(Options.SoundChannels.V, MinSoundChannels, MaxSoundChannels);
    Options.SoundVolume = Clamp(Options.SoundVolume.V, MinVolume, MaxVolume);
    Options.MusicVolume = Clamp(Options.MusicVolume.V, MinVolume, MaxVolume);
    translations::SetCurrentLanguage("en");
    winmain::UpdateFrameRate();
}

std::string GameInput::GetFullInputDescription() const { return GetShortInputDescription(); }
std::string GameInput::GetShortInputDescription() const
{
    if (Type != InputTypes::GameController) return Type == InputTypes::None ? "Unused" : "Input";
    static const char* n[] = {"A","B","X","Y","Back","Guide","Start","LStick","RStick","LT","RT","Up","Down","Left","Right"};
    return (Value >= 0 && Value < 15) ? n[Value] : "Controller";
}
OptionBase::OptionBase(LPCSTR name) : Name(name) { options::AllOptions.push_back(this); }
OptionBase::~OptionBase()
{
    auto i = std::find(options::AllOptions.begin(), options::AllOptions.end(), this);
    if (i != options::AllOptions.end()) options::AllOptions.erase(i);
}
std::string ControlOption::GetShortcutDescription() const
{
    for (const auto& i : Inputs) if (i.Type != InputTypes::None) return i.GetShortInputDescription();
    return "";
}
