/**
  * @file           : PackagedMusicPlayback.cpp
  * @author         : Romi Brooks
  * @brief          : use atom packager to provided a way which can be play in
  *                   memory with the media that from the dat file.
  * @attention      : The pack is created automatically on first
  *                   run; delete music_demo.pak to rebuild it.
  * @date           : 2026/8/20
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <Input/Key.hpp>
#include <Filesystem/PackageFileSystem.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Utilities/Packager/PackageBuilder.hpp>
#include <Utilities/Utf8/Utf8.hpp>
#include <Window/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Debugger/Overlay.hpp>
#include <Debugger/LogDebugger.hpp>

#include <Log/LogSystem.hpp>

namespace {
constexpr const char* source_files[] = {
    // replace it
    R"(E:\Music\So Far Away (feat. Jamie Scott & Romy Dya).mp3)",
    R"(E:\Music\DLSS,cny j - Feel U.mp3)",
    R"(E:\Music\1_DLSS,cny j - Feel U_(Instrumental).wav)",
    // Non-ASCII file name maybe cannot be display the title in Debugger overlay
    // you should load your font with imgui to support it
    // ref: https://github.com/ocornut/imgui/blob/master/docs/FONTS.md
    R"(E:\Music\我的歌声里 - 曲婉婷.mp3)",
};
constexpr const char* pack_path = "music_demo.apkg";

// One packed entry's in-memory payload, extracted from the APKG package.
struct PackEntry {
        std::string filename{}; // package-relative name (extension kept)
        std::vector<std::byte> data{};
};

// Debugger overlay
class PackedMusicDebugger final : public atom::debugger::DebugPanel {
    public:
        PackedMusicDebugger(atom::MusicPlayer& music, const std::vector<PackEntry>& entries,
                            std::string packPath, std::string packStatus)
            : DebugPanel("PackedMusicDebugger"), music_(music), entries_(entries), pack_path_(std::move(packPath)),
              pack_status_(std::move(packStatus)) {}

    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("Packed Music Player");

            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Separator();

            ImGui::TextUnformatted(pack_path_.c_str());
            ImGui::TextUnformatted(pack_status_.c_str());
            ImGui::Text("Entries: %zu (all loaded into memory)", entries_.size());
            ImGui::Separator();

            // One row per pack entry: name, size + play/stop
            for (std::size_t i = 0; i < entries_.size(); ++i) {
                const auto& entry = entries_[i];
                const std::string id = "track_" + std::to_string(i);

                ImGui::PushID(static_cast<int>(i));
                const bool loaded = music_.IsLoaded(id);
                const bool playing = music_.IsNowPlaying(id);

                ImGui::TextUnformatted(entry.filename.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("[%.1f KB]", static_cast<double>(entry.data.size()) / 1024.0);

                if (loaded) {
                    if (playing) {
                        if (ImGui::Button("Stop"))
                            music_.Stop(id);
                    } else if (ImGui::Button("Play")) {
                        music_.Play(id);
                    }
                } else {
                    ImGui::TextDisabled("load failed");
                }
                ImGui::PopID();
                ImGui::Separator();
            }

            ImGui::Text("Now playing: %s", music_.GetNowPlaying().c_str());
            const auto now_playing = music_.GetNowPlaying();
            if (!now_playing.empty()) {
                auto position = music_.GetPlayingOffset(now_playing);
                const auto duration = music_.GetDuration(now_playing);
                if (duration > 0.0f) {
                    ImGui::Text("Position: %.2f / %.2f s", static_cast<double>(position),
                                static_cast<double>(duration));
                    if (music_.IsSeekable(now_playing) &&
                        ImGui::SliderFloat("Seek", &position, 0.0f, duration, "%.2f s"))
                        music_.Seek(now_playing, position);
                } else {
                    ImGui::Text("Position: %.2f s (duration unknown)", static_cast<double>(position));
                }
            }
            ImGui::Separator();

            static float volume = music_.GetMusicVolume();
            if (ImGui::SliderFloat("Music Volume", &volume, 0.0f, 100.0f, "%.1f")) {
                music_.SetMusicVolume(volume);
            }

            ImGui::TextDisabled("Press ESC to exit");
            ImGui::End();
        }

    private:
        atom::MusicPlayer& music_;
        const std::vector<PackEntry>& entries_;
        std::string pack_path_;
        std::string pack_status_;
};

class PackedMusicScreen final : public atom::Screen {
    public:
        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::render::Color{30, 30, 60});
        }

        // INPUT: two paths exist and they are not interchangeable.
        //
        //   This one (event style) - a discrete command that must fire once per
        //   press and wants the exact event payload, here "Escape went down".
        //   Returning true consumes the event.
        //
        //   input::InputSystem (polling style) - anything continuous: "is this
        //   key held", "how far did the mouse move", "was a chord pressed this
        //   frame", and the only way to ask the same question from more than one
        //   place in a frame. Add <Input/InputSystem.hpp> and read it from
        //   Update()/FixedUpdate().
        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                if (key.key == atom::input::Key::Escape) {
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

    // Pack the selected files into an APKG package on first run.
    std::string pack_status;
    if (!std::filesystem::exists(pack_path)) {
        atom::tools::PackageBuilder builder;
        for (const auto* source : source_files) {
            const std::string name = std::filesystem::path{source}.filename().string();
            if (builder.AddFile(source, name) != atom::tools::PackageBuilder::Result::Success) {
                LOG_ERROR(atom::log::utilities::Packager, "Failed to queue file for packing: " + std::string{source});
                return 1;
            }
        }
        atom::tools::PackageBuilder::Config config;
        config.verbose = true;
        if (builder.Build(pack_path, config) != atom::tools::PackageBuilder::Result::Success) {
            LOG_ERROR(atom::log::utilities::Packager, "Packing failed, cannot continue");
            return 1;
        }
        pack_status = "Packed fresh (" + std::to_string(builder.PlannedFiles().size()) + " files)";
    } else {
        pack_status = "Reusing existing pack (delete music_demo.apkg to rebuild)";
    }

    // Mount the package and extract every entry into memory.
    std::unique_ptr<atom::fs::PackageFileSystem> package{};
    if (atom::fs::PackageFileSystem::Create("res", pack_path, package) != atom::fs::Result::Success || !package) {
        LOG_ERROR(atom::log::utilities::Packager, "Failed to open package: " + std::string{pack_path});
        return 1;
    }

    std::vector<atom::fs::PackagedAssetInfo> assets{};
    if (package->EnumerateAssets(assets) != atom::fs::Result::Success) {
        LOG_ERROR(atom::log::utilities::Packager, "Failed to enumerate package contents");
        return 1;
    }

    std::vector<PackEntry> entries;
    entries.reserve(assets.size());
    for (const auto& asset : assets) {
        atom::fs::AssetPath asset_path{};
        if (!atom::fs::AssetPath::TryParse("res://" + asset.path, asset_path))
            continue;
        std::unique_ptr<atom::fs::IFile> file{};
        if (package->OpenRead(asset_path, file) != atom::fs::Result::Success || !file)
            continue;
        std::vector<std::byte> data(file->Size());
        if (file->ReadNext(data) != atom::fs::Result::Success)
            continue;
        entries.push_back(PackEntry{asset.path, std::move(data)});
    }

    // stream-decode & play straight from memory
    atom::AudioMixer mixer;
    atom::MusicPlayer music{mixer};

    std::size_t loadedCount = 0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const std::string id = "track_" + std::to_string(i);
        // The buffer is borrowed by LoadFromMemory: `entries` outlives every
        // track for the whole program lifetime, satisfying the contract.
        if (music.LoadFromMemory(id, entry.filename, entry.data.data(), entry.data.size())) {
            ++loadedCount;
            LOG_INFO(atom::log::audio::Music, "Track ready (in-memory): " + entry.filename);
        } else {
            LOG_WARNING(atom::log::audio::Music, "Track load failed: " + entry.filename);
        }
    }
    if (loadedCount == 0) {
        LOG_ERROR(atom::log::audio::Music, "No track could be loaded from the pack");
        return 1;
    }

    //  screen register
    atom::ScreenManager::GetInstance().LoadScreen("PackedMusic", std::make_unique<PackedMusicScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("PackedMusic");

    auto& window = atom::RenderWindow::GetInstance();
    const auto initialized =
        window.Initialize("Atom Engine - Packaged Music Player (in-memory streaming)", atom::algo::Vec2{920, 720},
                          atom::backend::RenderBackendId::SdlGpu);
    if (!initialized) {
        LOG_ERROR(atom::log::core::Main, "Window initialization failed: " + initialized.error().Message());
        return 1;
    }

    window.SetFPS(60);

    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    PackedMusicDebugger debugger{music, entries, pack_path, pack_status};
    debugger.Attach(window);

    window.Run();
    return 0;
}
