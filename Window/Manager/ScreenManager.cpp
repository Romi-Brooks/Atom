/**
  * @file           : WindowsManager.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/23
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <ranges>

// Project Headers
#include <Log/LogSystem.hpp>

// Self Dependency
#include "ScreenManager.hpp"

namespace atom {
	auto ScreenManager::GetInstance() -> ScreenManager& {
		static ScreenManager instance;
		return instance;
	}

	auto ScreenManager::LoadScreen(const std::string& name, std::unique_ptr<Screen> screen) -> void {
		screens_[name] = std::move(screen);
		LOG_INFO(atom::LogChannel::ATOM_SCREEN_MANAGER, "Registering screen: " + name);
	}

	auto ScreenManager::SwitchScreen(const std::string& name) -> void {
		const auto it = screens_.find(name);
		if (it != screens_.end()) {
			if (current_screen_) {
				current_screen_->OnDeactivate();
			}
			// Clear the history stack
			// 清空历史栈
			screen_stack_.clear();
			// Update the current screen
			// 更新当前屏幕
			current_screen_ = it->second.get();
			current_screen_name_ = name;
			current_screen_->OnActivate();
		}
		LOG_INFO(atom::LogChannel::ATOM_SCREEN_MANAGER, "Switched screen to : " + name);
	}

	auto ScreenManager::PushScreen(const std::string& name) -> void {
		const auto it = screens_.find(name);
		if (it != screens_.end()) {
			if (current_screen_) {
				// Push the current screen onto the stack (historical screen)
				// 将当前屏幕压入栈（历史屏幕）
				screen_stack_.emplace_back(current_screen_name_, current_screen_);
				current_screen_->OnDeactivate(); // Pause the current screen
			}
			// The new screen becomes the current top layer
			// 新屏幕成为当前顶层
			current_screen_ = it->second.get();
			current_screen_name_ = name;
			current_screen_->OnActivate();
		}
		LOG_INFO(atom::LogChannel::ATOM_SCREEN_MANAGER, "Pushed & Rendering screen: " + name);
	}

	auto ScreenManager::PopScreen() -> void  {
		if (!screen_stack_.empty()) {
			if (current_screen_) {
				current_screen_->OnDeactivate();
			}
			// Restore the historical screen at the top of the stack
			// 恢复栈顶的历史屏幕
			const auto& previous = screen_stack_.back();
			current_screen_name_ = previous.first;
			current_screen_ = previous.second;
			screen_stack_.pop_back(); // Remove the top of the stack
			current_screen_->OnActivate(); // Restore the previous screen
		}
		LOG_INFO(atom::LogChannel::ATOM_SCREEN_MANAGER, "Popped all screen: ");
	}

	auto ScreenManager::Render(sf::RenderWindow& window) const -> void {
		// 1. Render the historical screens in the stack (lower layers)
		// 1. 渲染栈中的历史屏幕（下层屏幕）
		for (const auto& [name, screen] : screen_stack_) {
			screen->Render(window);
		}
		// 2. Render the current top screen (overlaid on top)
		// 2. 渲染当前顶层屏幕（覆盖在最上层）
		if (current_screen_) {
			current_screen_->Render(window);
		}
	}

	auto ScreenManager::HandleEvent(const sf::Event& event) const -> void {
		// 1. Give priority to the top screen (currently active screen, e.g., Start)
		// 1. 优先让顶层屏幕处理（当前激活的屏幕，如Start）
		if (current_screen_) {
			if (current_screen_->HandleEvent(event)) {
				return; // Top screen consumed the event, do not pass to lower layers
			}
		}

		// 2. If the top layer did not consume it, let the screens in the stack handle it (e.g., Debugger and other lower layers)
		// 2. 若顶层未消费，再让栈中屏幕处理（如Debugger等下层屏幕）
		// Note: Screens in the stack typically follow a "last-in, first-out" order; the traversal direction can be adjusted based on the actual hierarchy
		// 注意：栈中屏幕通常按"后入先出"顺序，可根据实际层级调整遍历方向
		for (const auto& screen : screen_stack_ | std::views::values) {
			if (screen->HandleEvent(event)) {
				return; // Lower screen consumed the event, stop propagation
			}
		}
	}

	auto ScreenManager::Update(const float deltaTime) const -> void {
		if (current_screen_) {
			current_screen_->Update(deltaTime);
		}
		// If lower layer screens also need to be updated, you can add:
		// 如果需要让下层屏幕也更新，可添加：
		// for (const auto& [name, screen] : screen_stack_) {
		//     screen->Update(deltaTime);
		// }
	}

	auto ScreenManager::GetCurrentScreenName() const -> const std::string& {
		return current_screen_name_;
	}

	auto ScreenManager::GetScreenStack() const -> const std::vector<std::pair<std::string, Screen*>>& {
		return screen_stack_;
	}

}
