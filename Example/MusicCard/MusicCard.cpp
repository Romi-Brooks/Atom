/**
 * @file           : MusicCard.cpp
 * @brief          : MusicCard example entry point: mounts the music directory,
 *                   discovers playable tracks, wires up the screen + debugger,
 *                   and runs the window loop.
 *
 * The screen's implementation lives in MusicCardScreen.cpp (declared in
 * MusicCardScreen.hpp), the debugger panel in MusicCardDebugger.hpp, and
 * shared types in MusicCardTypes.hpp.
 */

#include "MusicCardDebugger.hpp"
#include "Backend/SDLGPU/Device/SDLGPUBackend.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include <Backend/Contracts/Audio/AudioBackendId.hpp>
#include <Backend/Composition/BackendRuntime.hpp>
#include <Debugger/Panels/LogDebugger.hpp>
#include <Debugger/Overlay.hpp>
#include <Debugger/Panels/JobDebugger.hpp>
#include <Debugger/Panels/ThreadDebugger.hpp>
#include <Debugger/Panels/FpsDebugger.hpp>
#include <Debugger/Panels/RenderDebugger.hpp>
#include <Debugger/Panels/DisplayDebugger.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Media/Audio/AudioBackend.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Utilities/Utf8/Utf8.hpp>
#include <Window/RenderWindow.hpp>
#include <Runtime/Application.hpp>
#include <Runtime/ScreenManager.hpp>

namespace {

// Discovers audio files under the configured directory. Metadata and decoder
// work are deferred to the background prefetch loader so startup remains
// responsive and open file handles stay bounded.
//
// Only files the decoder registry can actually decode are listed: an entry the
// engine cannot open would otherwise show up on the card and then stay silent.
[[nodiscard]] auto LoadTrackPaths(const std::string& music_root) -> std::vector<std::string> {
    constexpr std::array audio_extensions{".mp3", ".wav", ".flac", ".ogg",  ".m4a",
                                          ".aac", ".wma", ".opus", ".aiff", ".aif"};
    auto paths = std::vector<std::string>{};
    const auto music_dir = atom::PathFromUtf8(music_root);
    std::error_code ec;
    if (!std::filesystem::is_directory(music_dir, ec)) {
        LOG_ERROR(atom::log::audio::Music, "Music path is not a directory: " + music_root);
        return paths;
    }
    auto& decoders = atom::audio::GetAudioDecoders();
    auto skipped_extensions = std::vector<std::string>{};
    auto skipped_files = std::size_t{0};
    for (const auto& entry : std::filesystem::directory_iterator(music_dir, ec)) {
        if (ec) {
            break;
        }
        std::error_code entry_ec;
        if (!entry.is_regular_file(entry_ec) || entry_ec) {
            continue;
        }
        auto ext = entry.path().extension().string();
        std::ranges::transform(ext, ext.begin(),
                               [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (std::ranges::find(audio_extensions, ext) == audio_extensions.end()) {
            continue;
        }
        if (!decoders.Contains(ext)) {
            ++skipped_files;
            if (std::ranges::find(skipped_extensions, ext) == skipped_extensions.end()) {
                skipped_extensions.push_back(ext);
            }
            continue;
        }
        paths.push_back(atom::PathToUtf8(entry.path()));
    }
    if (ec) {
        LOG_WARNING(atom::log::audio::Music, "Directory iteration error: " + ec.message());
    }
    if (skipped_files > 0) {
        auto list = std::string{};
        for (const auto& ext : skipped_extensions) {
            list += (list.empty() ? "" : ", ") + ext;
        }
        LOG_WARNING(atom::log::audio::Music, "Skipped " + std::to_string(skipped_files) +
                                                 " file(s) with no registered decoder (" + list +
                                                 "); register a decoder in AudioDecoderRegistry to play them");
    }
    std::ranges::sort(paths);
    LOG_INFO(atom::log::audio::Music, "Discovered " + std::to_string(paths.size()) + " playable audio file(s) in " +
                                          music_root + " (metadata deferred to lazy loader)");
    return paths;
}

} // namespace


auto main(int argc, char** argv) -> int {
    atom::Log::SetConsoleOutputUtf8();

    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_INFO);

    // Persist the log to Log/atom-YYYYMMDD-HHMMSS.log (the on-screen viewer keeps
    // only a small 1024-entry ring). The engine owns the directory-naming policy,
    // so a program never rebuilds this by hand. Open it before any subsystem
    // logs so the whole run is captured.
    const std::string log_file = atom::Log::EnableTimestampedFileSink();
    if (log_file.empty())
        LOG_WARNING(atom::log::core::Main, "Could not open the log file sink");
    else
        LOG_INFO(atom::log::core::Main, "Log file sink enabled: " + log_file);

    atom::AudioMixer mixer;
    atom::MusicPlayer music{mixer};
    const std::string music_root = argc > 1 ? std::string{argv[1]} : std::string{MusicPath};
    auto paths = LoadTrackPaths(music_root);

    // Mount the music directory as res:// so audio loads go through the VFS
    // (the same contract as packaged assets). Kept alive for the screen's
    // lifetime; the TagLib metadata reader still uses the raw paths.
    std::unique_ptr<atom::fs::NativeFileSystem> fs_owned{};
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(music_root), fs_owned) !=
            atom::fs::Result::Success ||
        !fs_owned) {
        LOG_ERROR(atom::log::audio::Music, "Music directory unavailable: " + music_root);
        return 1;
    }
    auto filesystem = std::shared_ptr<atom::fs::NativeFileSystem>{std::move(fs_owned)};

    auto screen = std::make_unique<MusicCardScreen>(music, std::move(paths), std::move(filesystem));
    const auto loaded = atom::ScreenManager::GetInstance().LoadScreen("MusicCard", std::move(screen));
    if (loaded != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::audio::Music,
                  "Failed to register the MusicCard screen: " + std::string{atom::ToString(loaded)});
        return 1;
    }
    // The manager now owns the screen; borrow the derived type back for the
    // debugger and the shutdown hook (this example never replaces it).
    auto* music_card_screen =
        dynamic_cast<MusicCardScreen*>(atom::ScreenManager::GetInstance().GetScreen("MusicCard"));
    if (music_card_screen == nullptr) {
        LOG_ERROR(atom::log::audio::Music, "MusicCard screen disappeared after registration");
        return 1;
    }
    atom::ScreenManager::GetInstance().SwitchScreen("MusicCard");

    auto& window = atom::RenderWindow::GetInstance();
    auto& application = atom::runtime::Application::GetInstance();
    // Startup presentation is the conservative desktop default: a normal window
    // paced to the display refresh. The Display panel attached below switches
    // window placement and present mode live, so every combination (windowed /
    // borderless / exclusive x vsync / mailbox / immediate) can be compared
    // without a restart -- which is the only honest way to separate what the
    // engine costs per frame from what the compositor allows.
    atom::runtime::ApplicationConfig display_config{};
    display_config.title = "Atom - Music Card Layout";
    display_config.resolution = atom::algo::Vec2{1920.0f, 1080.0f};
    display_config.render_backend = atom::backend::RenderBackendId::SdlGpu;
    display_config.present_mode = atom::render::PresentMode::VSync;
    display_config.gpu_driver     = atom::backend::GpuDriver::Direct3D12;

    const auto initialized = application.Initialize(display_config);

    if (!initialized) {
        LOG_ERROR(atom::log::core::Main, "Window initialization failed: " + initialized.error().Message());
        return 1;
    }

    // What means unlimited: 0 disables the frame limiter entirely.
    // The loop then runs as fast as presentation allows, so a refresh-paced
    // present mode caps it at the display refresh while Immediate
    // exposes the engine's own per-frame cost.
    window.SetFPS(0);

    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    atom::debugger::JobDebugger job_panel{};
    job_panel.Attach(window);

    atom::debugger::ThreadDebugger thread_panel{};
    thread_panel.Attach(window);

    // Three panels, one question each: how fast (FPS), where the time goes and
    // on what device (Render), and which presentation is producing it (Display).
    atom::debugger::FpsDebugger fps_panel{};
    fps_panel.Attach(window);

    atom::debugger::RenderDebugger render_panel{};
    render_panel.Attach(window);

    atom::debugger::DisplayDebugger display_panel{};
    display_panel.Attach(window);

    MusicCardDebugger debugger{*music_card_screen};
    debugger.Attach(window);

    auto renderer_shutdown = application.AddShutdownObserver([music_card_screen] { music_card_screen->Shutdown(); });
    music_card_screen->LoadInterfaceFont();

    application.Run();
    return 0;
}
