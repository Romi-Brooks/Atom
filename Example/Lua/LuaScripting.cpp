/**
  * @file           : LuaScripting.cpp
  * @author         : Romi Brooks
  * @brief          : Loads an external Lua script through the LuaHost and plays
  *                   a music track driven from Lua. Demonstrates script file IO
  *                   (VFS), context injection, and the Atom.Audio.* bindings.
  * @attention      : The script lives at a fixed path (ScriptPath, next to the
  *                   music directory) and is mounted as scripts://, so
  *                   LuaHost::LoadScript reads it through the VFS. Loading is
  *                   hot: if the script is missing or fails to run, the example
  *                   logs a warning and keeps running the window instead of
  *                   exiting.
  * @date           : 2026/9/24
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <memory>
#include <string>
#include <string_view>

#include <Filesystem/FileSystem.hpp>
#include <Filesystem/Vfs.hpp>
#include <Log/LogSystem.hpp>
#include <Lua/LuaHost.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Utilities/Utf8/Utf8.hpp>

#include <Window/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Debugger/LogDebugger.hpp>

namespace {
// Usually, this directory is open to Lua script developers,
// which is a security trade-off to prevent scripts from accessing higher-level directories.
constexpr auto MusicDir = R"(E:\Music\)";

// The script entry point is configured explicitly here,
// Change ScriptFile to point at a different script. Leave ScriptFile empty to disable
// script loading entirely.
constexpr auto ScriptDir = R"(.\)";
constexpr auto ScriptFile = "script.lua";

class LuaScreen final : public atom::Screen {
    public:
        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::render::Color{20, 20, 30});
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                if (key.key == atom::event::Key::Escape) {
                    atom::RenderWindow::GetInstance().Shutdown();
                    return true;
                }
            }
            return false;
        }

        auto Update(float) -> void override {}
};

} // namespace

static auto HandleResFs() -> atom::fs::Result {
    // Mount the music directory as "res" on the process-wide default Vfs.
    std::unique_ptr<atom::fs::NativeFileSystem> music_fs{};
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(MusicDir), music_fs) !=
            atom::fs::Result::Success ||
        !music_fs) {
        LOG_ERROR(atom::log::audio::Music, "Music directory unavailable: " + std::string{MusicDir});
        return atom::fs::Result::NotFound;
        }
    atom::fs::Vfs::GetInstance().Mount("res", 0, std::move(music_fs));
    return atom::fs::Result::Success;
}

static auto HandleScriptFs() -> atom::fs::Result {
    // The script plays the role of a mod's entry script: a plain directory the
    // host mounts, with the script filename configured explicitly above (no
    // argv/__FILE__ derivation). The host still owns mounting; the script only
    // ever sees a mounted VFS path.
    std::unique_ptr<atom::fs::NativeFileSystem> script_fs{};
    if (atom::fs::NativeFileSystem::Create("scripts", atom::PathToUtf8(ScriptDir), script_fs) !=
            atom::fs::Result::Success ||
        !script_fs) {
        LOG_ERROR(atom::log::core::Lua, "Script directory unavailable: " + std::string{ScriptDir});
        return atom::fs::Result::NotFound;
        }
    atom::fs::Vfs::GetInstance().Mount("scripts", 0, std::move(script_fs));
    return atom::fs::Result::Success;
}

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    // init the audio player
    atom::AudioMixer mixer;
    atom::MusicPlayer music{mixer};

    HandleResFs();
    HandleScriptFs();

    // Wire the engine objects into the Lua context so the script can drive them.
    atom::LuaHost host;
    host.Context().music = &music;
    host.Context().mixer = &mixer;
    if (!host.Initialize()) {
        LOG_ERROR(atom::log::core::Lua, "Failed to initialize Lua state");
        return 1;
    }

    // Hot load the script: a missing or failing script is reported and skipped,
    // it never tears the whole program down. The window and audio keep running.
    if (std::string_view{ScriptFile}.empty()) {
        LOG_WARNING(atom::log::core::Lua, "No script configured; running without a Lua script");
    } else {
        const std::string script_asset = std::string{"scripts://"} + ScriptFile;
        atom::fs::AssetPath script_path{};
        if (!atom::fs::AssetPath::TryParse(script_asset, script_path) ||
            !host.LoadScript(atom::fs::Vfs::GetInstance(), script_path)) {
            LOG_WARNING(atom::log::core::Lua, "Script failed to load; continuing without it");
        }
    }

    // The script already called Atom.Audio.Music.Load/Play; verify the track is
    // registered and playing from the C++ side.
    if (!music.IsLoaded("song_1"))
        LOG_WARNING(atom::log::audio::Music, "Lua loaded track is not registered as 'song_1'");
    else
        LOG_INFO(atom::log::audio::Music, "Track 'song_1' registered by the Lua script (now playing)");

    auto* lua_screen = atom::ScreenManager::GetInstance().LoadScreen("LuaScripting", std::make_unique<LuaScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen(lua_screen);

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom - Lua Scripting Example", atom::algo::Vec2{720, 720},
                      atom::backend::RenderBackendId::SdlGpu);
    window.SetFPS(60);

    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    window.Run();
    return 0;
}
