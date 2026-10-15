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

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include <Backend/Contracts/Audio/AudioBackendId.hpp>
#include <Backend/Runtime/BackendRuntime.hpp>
#include <Debugger/LogDebugger.hpp>
#include <Debugger/Overlay.hpp>
#include <Debugger/JobSystemDebugger.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Media/Audio/AudioBackend.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Utilities/Utf8/Utf8.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/ScreenManager.hpp>

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
#ifdef _WIN32
    // Raise the C runtime file-descriptor ceiling. Each streaming audio decoder
    // keeps its source file open; with lazy loading we keep <= 3 decoders alive,
    // but a generous ceiling prevents EMFILE surprises during metadata reads.
    _setmaxstdio(512);
#endif // _WIN32
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_INFO);
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
    const auto initialized = window.Initialize("Atom - Music Card Layout", atom::algo::Vec2{1920.0f, 1080.0f},
                                              atom::backend::RenderBackendId::SdlGpu);
    if (!initialized) {
        LOG_ERROR(atom::log::core::Main, "Window initialization failed: " + initialized.error().Message());
        return 1;
    }
    window.SetVSync(false);
    window.SetFPS(165);

    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    atom::debugger::JobSystemDebugger job_panel{};
    job_panel.Attach(window);

    MusicCardDebugger debugger{*music_card_screen};
    debugger.Attach(window);
    auto renderer_shutdown = window.AddShutdownListener([music_card_screen] { music_card_screen->Shutdown(); });
    music_card_screen->LoadInterfaceFont();

    window.Run();
    return 0;
}
