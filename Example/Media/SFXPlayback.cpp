/**
  * @file           : SFXPlayback.cpp
  * @author         : Romi Brooks
  * @brief          : SFX playback demo with voice pool (overlapping sounds).
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Media/Audio/AudioBackend.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/SFXPlayer.hpp>
#include <Media/Audio/Resources/AudioClipCache.hpp>
#include <Filesystem/FileSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>
#include <Window/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Debugger/Overlay.hpp>
#include <Debugger/LogDebugger.hpp>

#include <Log/LogSystem.hpp>

namespace {
// replace it
constexpr auto SFXDir = R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\)";
constexpr auto SFX1Name = "Cymatics - Vocal Essentials One Shot 1 - C.wav";
constexpr auto SFX2Name = "Cymatics - Vocal Essentials One Shot 2 - C.wav";

// Debugger overlay
class SFXDebugger final : public atom::debugger::DebugPanel {
    public:
        explicit SFXDebugger(atom::SFXPlayer& sfx) : DebugPanel("SFXDebugger"), sfx_(sfx) {}

    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("SFX Debugger");
            ImGui::Text("Press A to play sfx1, B to play sfx2");
            if (ImGui::Button("A")) {
                sfx_.Play("registerId_1");
            }
            if (ImGui::Button("B")) {
                sfx_.Play("registerId_2");
            }
            ImGui::Separator();

            ImGui::Text("this btm allows you play those file at the same time:");
            if (ImGui::Button("Play")) {
                sfx_.Play("registerId_1");
                sfx_.Play("registerId_2");
            }
            ImGui::Separator();

            const auto& backend_id = atom::audio::GetAudioBackendId();
            ImGui::TextDisabled("Active audio backend: %s", backend_id.c_str());
            ImGui::Separator();

            ImGui::Text("Press ESC to exit");
            ImGui::End();
        }

    private:
        atom::SFXPlayer& sfx_;
};

class SFXScreen final : public atom::Screen {
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
    atom::AudioMixer mixer;
    atom::AudioClipCache clips;
    atom::SFXPlayer sfx{clips, mixer};

    // Mount the SFX directory as res:// so clips load through the VFS contract.
    std::unique_ptr<atom::fs::NativeFileSystem> filesystem{};
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(SFXDir), filesystem) !=
            atom::fs::Result::Success ||
        !filesystem) {
        LOG_ERROR(atom::log::audio::Music, "SFX directory unavailable: " + std::string{SFXDir});
        return 1;
    }
    atom::fs::AssetPath sfx1{};
    atom::fs::AssetPath sfx2{};
    if (!atom::fs::AssetPath::TryParse("res://" + std::string{SFX1Name}, sfx1) ||
        !atom::fs::AssetPath::TryParse("res://" + std::string{SFX2Name}, sfx2)) {
        LOG_ERROR(atom::log::audio::Music, "SFX filenames are not valid AssetPath segments");
        return 1;
    }
    sfx.Load("registerId_1", *filesystem, sfx1);
    sfx.Load("registerId_2", *filesystem, sfx2);

    auto* sfx_screen =
        atom::ScreenManager::GetInstance().LoadScreen("SFX", std::make_unique<SFXScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen(sfx_screen);

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - SFX Playback Example", atom::algo::Vec2{720, 720},
                      atom::backend::RenderBackendId::SdlGpu);

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    atom::debugger::LogDebugger log_panel{};
    log_panel.Attach(window);

    SFXDebugger debugger{sfx};
    debugger.Attach(window);

    window.Run();
}
