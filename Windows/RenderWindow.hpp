/**
  * @file           : RenderWindow.hpp
  * @author         : Romi Brooks
  * @brief          : Main render window singleton (Engine Core)
  * @attention      :
  * @date           : 2025/9/28
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_RENDER_WINDOW_HPP
#define ATOM_RENDER_WINDOW_HPP

// Standard Library
#include <functional>

// Third Party Library
#include <SFML/Graphics.hpp>

// Engine Headers
#include <Math/Vector/Vec2.hpp>
#include <Windows/Manager/ScreenManager.hpp>

namespace atom {
	class RenderWindow {
		private:
			sf::RenderWindow window_;
			unsigned int fps_ = 60;

			RenderWindow() = default;
			~RenderWindow() = default;

			auto ProcessEvents(const ScreenManager& screenManager) -> void {
				while (const auto event = window_.pollEvent()) {
					// Optional overlay event processing (e.g., ImGui)
					// 可选的叠加层事件处理（例如 ImGui）
					if (on_process_event_) {
						on_process_event_(window_, *event);
					}

					screenManager.HandleEvent(*event);

					if (event->is<sf::Event::Closed>()) {
						window_.close();
					}
				}
			}

		public:
			[[nodiscard]] static auto GetInstance() -> RenderWindow& {
				static RenderWindow instance;
				return instance;
			}

			// --- Callback hooks for optional debug overlays (e.g. ImGui-SFML) ---
			//     可选的调试叠加层回调钩子（例如 ImGui-SFML）

			// Called per event for overlay event processing
			// 每事件调用一次，用于叠加层事件处理
			std::function<void(sf::RenderWindow&, const sf::Event&)> on_process_event_;

			// Called per frame before rendering for overlay update
			// 每帧渲染前调用，用于叠加层更新
			std::function<void(sf::Time)> on_update_;

			// Called per frame after scene render for overlay rendering
			// 每帧场景渲染后调用，用于叠加层渲染
			std::function<void(sf::RenderWindow&)> on_render_overlay_;

			// Called on shutdown for overlay cleanup
			// 关闭时调用，用于叠加层清理
			std::function<void()> on_shutdown_;

			// --- Core API ---

			auto Initialize(const std::string& title, Vec2 resolution) -> void {
				window_.create(sf::VideoMode(
					{static_cast<unsigned>(resolution.GetX()),
					 static_cast<unsigned>(resolution.GetY())}), title);
				window_.setFramerateLimit(fps_);
			}

			auto Run() -> void {
				const auto& screenManager = atom::ScreenManager::GetInstance();

				// Check if there is a current screen
				// 检查是否有当前屏幕
				if (screenManager.GetCurrentScreenName().empty()) {
					throw std::runtime_error("No current screen set. Cannot run application.");
				}

				sf::Clock deltaClock;

				while (window_.isOpen()) {
					// Event Process
					ProcessEvents(screenManager);

					// Optional overlay update (e.g., ImGui)
					// 可选的叠加层更新（例如 ImGui）
					if (on_update_) {
						on_update_(deltaClock.restart());
					}

					// self & screen Render
					window_.clear();

					screenManager.Render(window_);

					// Optional overlay rendering (e.g., ImGui)
					// 可选的叠加层渲染（例如 ImGui）
					if (on_render_overlay_) {
						on_render_overlay_(window_);
					}

					window_.display();
				}
				Shutdown();
			}

			auto SetFPS(const unsigned int fps) -> void {
				fps_ = fps;
				window_.setFramerateLimit(fps_);
			}

			auto GetWindow() -> sf::RenderWindow& {
				return window_;
			}

			[[nodiscard]] auto GetWindow() const -> const sf::RenderWindow& {
				return window_;
			}

			[[nodiscard]] auto GetFPS() const -> unsigned {
				return fps_;
			}

			[[nodiscard]] auto IsOpen() const -> bool {
				return window_.isOpen();
			}

			auto Shutdown() -> void {
				// Optional overlay shutdown (e.g., ImGui)
				// 可选的叠加层关闭清理（例如 ImGui）
				if (on_shutdown_) {
					on_shutdown_();
				}
				if (window_.isOpen()) {
					window_.close();
				}
			}
		};
}

#endif // ATOM_RENDER_WINDOW_HPP
