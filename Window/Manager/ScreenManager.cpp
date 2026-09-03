/**
  * @file           : WindowsManager.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/23
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include "ScreenManager.hpp"

#include <ranges>

#include <Log/LogSystem.hpp>

namespace atom {
auto ScreenManager::GetInstance() -> ScreenManager& {
    static ScreenManager instance;
    return instance;
}

auto ScreenManager::LoadScreen(const std::string& name, std::unique_ptr<Screen> screen) -> void {
    screens_[name] = std::move(screen);
    LOG_INFO(atom::core::LogChannel::SCREEN_MANAGER, "Registering screen: " + name);
}

auto ScreenManager::SwitchScreen(const std::string& name) -> void {
    const auto it = screens_.find(name);
    if (it != screens_.end()) {
        if (current_screen_) {
            current_screen_->OnDeactivate();
        }
        screen_stack_.clear();
        current_screen_ = it->second.get();
        current_screen_name_ = name;
        current_screen_->OnActivate();
    }
    LOG_INFO(atom::core::LogChannel::SCREEN_MANAGER, "Switched screen to : " + name);
}

auto ScreenManager::PushScreen(const std::string& name) -> void {
    const auto it = screens_.find(name);
    if (it != screens_.end()) {
        if (current_screen_) {
            screen_stack_.emplace_back(current_screen_name_, current_screen_);
            current_screen_->OnDeactivate();
        }
        current_screen_ = it->second.get();
        current_screen_name_ = name;
        current_screen_->OnActivate();
    }
    LOG_INFO(atom::core::LogChannel::SCREEN_MANAGER, "Pushed & Rendering screen: " + name);
}

auto ScreenManager::PopScreen() -> void {
    if (!screen_stack_.empty()) {
        if (current_screen_) {
            current_screen_->OnDeactivate();
        }
        const auto& previous = screen_stack_.back();
        current_screen_name_ = previous.first;
        current_screen_ = previous.second;
        screen_stack_.pop_back();
        current_screen_->OnActivate();
    }
    LOG_INFO(atom::core::LogChannel::SCREEN_MANAGER, "Popped all screen: ");
}

auto ScreenManager::Render(atom::render::IRenderTarget& target) const -> void {
    for (const auto& [name, screen] : screen_stack_) {
        screen->Render(target);
    }
    if (current_screen_) {
        current_screen_->Render(target);
    }
}

auto ScreenManager::HandleEvent(const atom::window::IEvent& event) const -> void {
    if (current_screen_) {
        if (current_screen_->HandleEvent(event)) {
            return;
        }
    }
    for (const auto& screen : screen_stack_ | std::views::values) {
        if (screen->HandleEvent(event)) {
            return;
        }
    }
}

auto ScreenManager::Update(const float deltaTime) const -> void {
    if (current_screen_) {
        current_screen_->Update(deltaTime);
    }
}

auto ScreenManager::GetCurrentScreenName() const -> const std::string& {
    return current_screen_name_;
}

auto ScreenManager::GetScreenStack() const -> const std::vector<std::pair<std::string, Screen*>>& {
    return screen_stack_;
}
} // namespace atom
