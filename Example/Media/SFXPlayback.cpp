/**
  * @file           : SFXPlayback.cpp
  * @author         : Romi Brooks
  * @brief          : SFX playback demo with voice pool (overlapping sounds).
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <Backend/Runtime/BackendRuntime.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/SFXPlayer.hpp>
#include <Media/Audio/Resources/AudioClipCache.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>
#include <Window/Overlay.hpp>

#include <Log/LogSystem.hpp>

#ifdef _WIN32
#include "windows.h"
#endif // _WIN32

namespace {
// replace it
constexpr auto kSFX1Path =
    R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\Cymatics - Vocal Essentials One Shot 1 - C.wav)";
constexpr auto kSFX2Path =
    R"(D:\Sample Packs\Cymatics - Vocal Essentials\Vocal Shots\Cymatics - Vocal Essentials One Shot 2 - C.wav)";

// Debugger overlay
class SFXDebugger final : public atom::Debugger {
    public:
        explicit SFXDebugger(atom::SFXPlayer& sfx) : sfx_(sfx) {}

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

            ImGui::TextDisabled("Playback Backend: SDL3 (only registered playback backend)");
            ImGui::Separator();

            ImGui::Text("Press ESC to exit");
            ImGui::End();
        }

    private:
        atom::SFXPlayer& sfx_;
};

class SFXScreen final : public atom::Screen {
    public:
        auto Render(atom::render::IRenderTarget& target) -> void override {
            target.Clear(atom::render::Color{30, 30, 60});
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                if (key.scancode == 41) { // SDL_SCANCODE_ESCAPE
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
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32

    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);
    atom::AudioMixer mixer;
    atom::AudioClipCache clips;
    atom::SFXPlayer sfx{clips, mixer};

    sfx.Load("registerId_1", kSFX1Path);
    sfx.Load("registerId_2", kSFX2Path);

    atom::ScreenManager::GetInstance().LoadScreen("SFX", std::make_unique<SFXScreen>());
    atom::ScreenManager::GetInstance().SwitchScreen("SFX");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom Engine - SFX Playback Example", atom::algo::Vec2{720, 720});

    // It is recommended to limit the FPS when creating the window,
    // or define a custom FPS limit; otherwise it will significantly
    // consume GPU/CPU resources.
    window.SetFPS(60);

    SFXDebugger debugger{sfx};
    debugger.Attach(window);

    window.Run();
}
