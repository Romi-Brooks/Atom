/**
  * @file           : WindowsManager.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/23
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// We use a "RenderWindow" class to make sure that all screen's (like 'StartScreen', 'HUD', 'Debugger', etc...)
// render are clearly and switchable, Render Window is singleton class that it has only one entity in all program,
// And it will render all things, if player selected some screen, use 'ScreenManager' class to switch it.

// to creat a screen, write a screen class with only render function, save to Screen Manager, use api to control it,
// Render Window will render it as well.

#ifndef ATOM_SCREENMANAGER_HPP
#define ATOM_SCREENMANAGER_HPP

// Standard Library
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Engine Headers
#include <Windows/Screen.hpp>

namespace atom {
	class ScreenManager {
		private:
			// Store all loaded screens
			// 存储所有已加载的屏幕
			std::unordered_map<std::string, std::unique_ptr<Screen>> screens_ {};
			// Currently active screen (top of the stack)
			// 当前激活的屏幕 (栈顶屏幕)
			Screen* current_screen_ = nullptr;
			std::string current_screen_name_ {};
			// Use vector to simulate a stack, storing historical screens (from bottom to top)
			// 改用vector模拟栈, 存储历史屏幕 (从底到顶)
			std::vector<std::pair<std::string, Screen*>> screen_stack_;

			ScreenManager() = default;
			~ScreenManager() = default;

		public:
			ScreenManager(const ScreenManager&) = delete;
			ScreenManager& operator=(const ScreenManager&) = delete;

			[[nodiscard]] static auto GetInstance() -> ScreenManager&;

			// Load a screen and register it with the manager
			// 加载屏幕并注册到管理器
			auto LoadScreen(const std::string& name, std::unique_ptr<Screen> screen) -> void;

			// Switch screen (replace the current screen, clear the stack)
			// 切换屏幕（替换当前屏幕，清空栈）
			auto SwitchScreen(const std::string& name) -> void;

			// Push a new screen (keep the current screen on the stack, the new screen becomes the top)
			// 压入新屏幕（保留当前屏幕到栈中，新屏幕成为顶层）
			auto PushScreen(const std::string& name) -> void;

			// Pop the top screen (restore the historical screen at the top of the stack)
			// 弹出顶层屏幕（恢复栈顶的历史屏幕）
			auto PopScreen() -> void;

			// Multi-screen rendering: first render all historical screens in the stack (bottom to top), then render the current top screen
			// 多屏幕渲染：先渲染栈中所有历史屏幕（从底到顶），再渲染当前顶层屏幕
			auto Render(sf::RenderWindow& window) const -> void;

			// Event handling: by default, only the top screen handles events (can be modified to pass to all screens as needed)
			// 事件处理：默认只让顶层屏幕处理事件（可根据需求修改为传递给所有屏幕）
			auto HandleEvent(const sf::Event& event) const -> void;

			// Update logic: by default, only the top screen is updated (can be modified as needed)
			// 更新逻辑：默认只更新顶层屏幕（可根据需求修改）
			auto Update(float deltaTime) const -> void;

			[[nodiscard]] auto GetCurrentScreenName() const -> const std::string&;

			// Get the screen stack (returns a const reference to avoid copying)
			// 获取屏幕栈（返回const引用，避免复制）
			[[nodiscard]] auto GetScreenStack() const -> const std::vector<std::pair<std::string, Screen*>>&;
	};
}
#endif // ATOM_SCREENMANAGER_HPP
