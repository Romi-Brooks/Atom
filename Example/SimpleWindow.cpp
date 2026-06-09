/**
  * @file           : SimpleWindow.cpp
  * @author         : Romi Brooks
  * @brief          : Simple single-window rendering example using Atom Engine API
  * @attention      :
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

// Third Party Library
#include <SFML/Graphics.hpp>

// Engine Headers
#include <Window/RenderWindow.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Screen.hpp>

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
	atom::ScreenManager::GetInstance().LoadScreen("Example", std::make_unique<ExampleScreen>());
	atom::ScreenManager::GetInstance().SwitchScreen("Example");

	auto& window = atom::RenderWindow::GetInstance();
	window.Initialize("Atom Engine - Simple Window Example", atom::Vec2{1280, 720});

	window.Run();

	return 0;
}
