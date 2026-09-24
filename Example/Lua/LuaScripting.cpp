/**
  * @file           : LuaScripting.cpp
  * @author         : Romi Brooks
  * @brief          : Loads an external Lua script through the LuaHost and plays
  *                   a music track driven from Lua. Demonstrates script file IO
  *                   (VFS), context injection, and the Atom.Audio.* bindings.
  * @attention      : The script lives at Example/Lua/script.lua and is mounted
  *                   as res:// so LuaHost::LoadScript reads it through the VFS,
  *                   never a raw path.
  * @date           : 2026/9/24
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <filesystem>
#include <memory>
#include <string>

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

constexpr auto kMusicDir = R"(E:\Music\)";

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

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);

    atom::AudioMixer mixer;
    atom::MusicPlayer music{mixer};

    // Mount the music directory as "res" and the script directory as "scripts"
    // on the process-wide default Vfs.
    std::unique_ptr<atom::fs::NativeFileSystem> music_fs{};
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(kMusicDir), music_fs) !=
            atom::fs::Result::Success ||
        !music_fs) {
        LOG_ERROR(atom::log::audio::Music, "Music directory unavailable: " + std::string{kMusicDir});
        return 1;
    }
    atom::fs::Vfs::GetInstance().Mount("res", 0, std::move(music_fs));

    std::unique_ptr<atom::fs::NativeFileSystem> script_fs{};
    const std::string script_dir = atom::PathToUtf8(std::filesystem::path{__FILE__}.parent_path().string());
    if (atom::fs::NativeFileSystem::Create("scripts", script_dir, script_fs) != atom::fs::Result::Success ||
        !script_fs) {
        LOG_ERROR(atom::log::core::Lua, "Script directory unavailable: " + script_dir);
        return 1;
    }
    atom::fs::Vfs::GetInstance().Mount("scripts", 0, std::move(script_fs));

    // Wire the engine objects into the Lua context so the script can drive them.
    atom::LuaHost host;
    host.Context().music = &music;
    host.Context().mixer = &mixer;
    if (!host.Initialize()) {
        LOG_ERROR(atom::log::core::Lua, "Failed to initialize Lua state");
        return 1;
    }

    // Load the external script through the VFS.
    atom::fs::AssetPath script_path{};
    if (!atom::fs::AssetPath::TryParse("scripts://script.lua", script_path) ||
        !host.LoadScript(atom::fs::Vfs::GetInstance(), script_path)) {
        LOG_ERROR(atom::log::core::Lua, "Failed to load script from VFS");
        return 1;
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
