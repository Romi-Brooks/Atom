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
#include <vector>

#include <Event/Input.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Utilities/Packager/Packager.hpp>
#include <Utilities/Packager/Unpackager.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Window/Overlay.hpp>

#include <Log/LogSystem.hpp>

namespace {
constexpr const char* kSourceFiles[] = {
    // replace it
    R"(E:\Music\So Far Away (feat. Jamie Scott & Romy Dya).mp3)",
    R"(E:\Music\DLSS,cny j - Feel U.mp3)",
    R"(E:\Music\1_DLSS,cny j - Feel U_(Instrumental).wav)",
    // Non-ASCII file name maybe cannot be display the title in Debugger overlay
    // you should load your font with imgui to support it
    // ref: https://github.com/ocornut/imgui/blob/master/docs/FONTS.md
    R"(E:\Music\我的歌声里 - 曲婉婷.mp3)",
};
constexpr const char* kPackPath = "music_demo.pak";

// Debugger overlay
class PackedMusicDebugger final : public atom::Debugger {
    public:
        PackedMusicDebugger(atom::MusicPlayer& music, const std::vector<atom::tools::Unpackager::MemoryFile>& files,
                            std::string packPath, std::string packStatus)
            : music_(music), files_(files), pack_path_(std::move(packPath)), pack_status_(std::move(packStatus)) {}

    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("Packed Music Player");

            ImGui::Text("FPS: %.1f", GetFPS());
            ImGui::Separator();

            ImGui::TextUnformatted(pack_path_.c_str());
            ImGui::TextUnformatted(pack_status_.c_str());
            ImGui::Text("Entries: %zu (all loaded into memory)", files_.size());
            ImGui::Separator();

            // One row per pack entry: name, type, size + play/stop
            for (std::size_t i = 0; i < files_.size(); ++i) {
                const auto& file = files_[i];
                const std::string id = "track_" + std::to_string(i);

                ImGui::PushID(static_cast<int>(i));
                const bool loaded = music_.IsLoaded(id);
                const bool playing = music_.IsNowPlaying(id);

                ImGui::TextUnformatted(file.filename.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("[%s, %.1f KB]", file.type.c_str(), static_cast<double>(file.GetSize()) / 1024.0);

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
        const std::vector<atom::tools::Unpackager::MemoryFile>& files_;
        std::string pack_path_;
        std::string pack_status_;
};

class PackedMusicScreen final : public atom::Screen {
    public:
        auto Render(atom::render::IRenderDevice& device) -> void override {
            device.Clear(atom::render::Color{30, 30, 60});
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

    // pack the selected files into a resource pack
    std::string pack_status;
    if (!std::filesystem::exists(kPackPath)) {
        atom::tools::Packager packer;
        atom::tools::Packager::Config config;
        config.verbose = true;
        // Entries use bare filenames (extensions kept, so a decoder can be selected by extension).
        config.preserveStructure = false;

        const std::vector<std::string> sources(std::begin(kSourceFiles), std::end(kSourceFiles));
        const auto result = packer.Pack(sources, kPackPath, config);
        if (result != atom::tools::Packager::Result::SUCCESS) {
            LOG_ERROR(atom::utilities::LogChannel::PACKAGER, "Packing failed, cannot continue");
            return 1;
        }
        packer.PrintPackageInfo();
        pack_status = "Packed fresh (" + std::to_string(packer.GetPackedFiles().size()) + " files)";
    } else {
        pack_status = "Reusing existing pack (delete music_demo.pak to rebuild)";
    }

    // load the pack and extract every entry into memory
    atom::tools::Unpackager unpacker;
    if (unpacker.Load(kPackPath, /*verbose=*/true) != atom::tools::Unpackager::Result::SUCCESS) {
        LOG_ERROR(atom::utilities::LogChannel::PACKAGER, "Failed to load package: " + std::string(kPackPath));
        return 1;
    }
    unpacker.PrintPackageInfo();

    std::vector<atom::tools::Unpackager::MemoryFile> memoryFiles;
    if (unpacker.ExtractAllToMemory(memoryFiles) != atom::tools::Unpackager::Result::SUCCESS) {
        LOG_ERROR(atom::utilities::LogChannel::PACKAGER, "Failed to extract package contents into memory");
        return 1;
    }

    // stream-decode & play straight from memory
    atom::AudioMixer mixer;
    atom::MusicPlayer music{mixer};

    std::size_t loadedCount = 0;
    for (std::size_t i = 0; i < memoryFiles.size(); ++i) {
        const auto& file = memoryFiles[i];
        const std::string id = "track_" + std::to_string(i);
        // The buffer is borrowed by LoadFromMemory: memoryFiles outlives every
        // track for the whole program lifetime, satisfying the contract.
        if (music.LoadFromMemory(id, file.filename, file.GetData(), file.GetSize())) {
            ++loadedCount;
            LOG_INFO(atom::audio::LogChannel::MUSIC, "Track ready (in-memory): " + file.filename);
        } else {
            LOG_WARNING(atom::audio::LogChannel::MUSIC, "Track load failed: " + file.filename);
        }
    }
    if (loadedCount == 0) {
        LOG_ERROR(atom::audio::LogChannel::MUSIC, "No track could be loaded from the pack");
        return 1;
    }

    //  screen register
    atom::ScreenManager::GetInstance().LoadScreen("PackedMusic", std::make_unique<PackedMusicScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("PackedMusic");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - Packaged Music Player (in-memory streaming)", atom::algo::Vec2{920, 720});

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    PackedMusicDebugger debugger{music, memoryFiles, kPackPath, pack_status};
    debugger.Attach(window);
    debugger.SetLoggerEnabled(true);

    window.Run();
    return 0;
}
