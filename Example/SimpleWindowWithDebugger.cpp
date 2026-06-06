/**
  * @file           : SimpleWindowWithDebugger.cpp
  * @author         : Romi Brooks
  * @brief          : Single-window example with custom debug overlay
  * @attention      : Demonstrates how to inherit atom::Debugger for custom content
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

// Third Party Library
#include <SFML/Graphics.hpp>
#include <imgui.h>

// Engine Headers
#include <Window/RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Screen.hpp>
#include <Window/Debugger.hpp>

// simple FPS debugger
class ExampleDebugger final : public atom::Debugger {
    protected:
        auto OnDrawOverlay() -> void override {
            // Calculate FPS every second
            ++frame_count_;
            if (fps_clock_.getElapsedTime().asSeconds() >= 1.0f) {
                fps_value_ = static_cast<float>(frame_count_);
                frame_count_ = 0;
                fps_clock_.restart();
            }

            ImGui::Begin("Example Debugger");
            ImGui::Text("FPS: %.1f", fps_value_);
            ImGui::Separator();
            ImGui::Text("Press ESC to exit");
            ImGui::End();
        }

    private:
        std::size_t frame_count_ = 0;
        sf::Clock fps_clock_ {};
        float fps_value_ = 0.0f;
};

class ExampleScreen final : public atom::Screen {
	public:
		auto Render(sf::RenderWindow& window) -> void override {
			window.clear(sf::Color(30, 30, 60));
		}

		auto HandleEvent(const sf::Event& event) -> bool override {
			if (event.is<sf::Event::KeyPressed>()) {
				const auto& key = event.getIf<sf::Event::KeyPressed>();
				if (key->code == sf::Keyboard::Key::Escape) {
					atom::RenderWindow::GetInstance().Shutdown();
					return true;
				}
			}
			return false;
		}

		auto Update(float) -> void override {
		}
};

auto main() -> int {
	// 1. Register the example screen
	auto& screenManager = atom::ScreenManager::GetInstance();
	screenManager.LoadScreen("Example", std::make_unique<ExampleScreen>());
	screenManager.SwitchScreen("Example");

	// 2. Initialize the render window
	auto& window = atom::RenderWindow::GetInstance();
	window.Initialize("Atom Engine - Debug Overlay Example", atom::Vec2{1280, 720});

	// 3. attach a custom debug overlay to the window
	ExampleDebugger debugger {};
	debugger.Attach(window);

	// 4. Run the main loop
	window.Run();
}
