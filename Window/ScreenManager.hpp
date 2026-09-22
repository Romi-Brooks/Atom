/**
  * @file           : ScreenManager.hpp
  * @author         : Romi Brooks
  * @brief          : Screen registration, switching, and per-frame dispatch.
  * @attention      :
  * @date           : 2025/9/23
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_SCREENMANAGER_HPP
#define ATOM_SCREENMANAGER_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Window/Screen.hpp>

namespace atom {
class ScreenManager {
    private:
        std::unordered_map<std::string, std::unique_ptr<Screen>> screens_{};
        Screen* current_screen_ = nullptr;
        std::string current_screen_name_{};
        std::vector<std::pair<std::string, Screen*>> screen_stack_;

        ScreenManager() = default;
        ~ScreenManager() = default;

    public:
        ScreenManager(const ScreenManager&) = delete;
        ScreenManager& operator=(const ScreenManager&) = delete;

        [[nodiscard]] static auto GetInstance() -> ScreenManager&;

        // Returns a non-owning pointer to the stored screen (valid until the
        // name is reloaded or the manager is destroyed). Prefer this over
        // keeping the unique_ptr — LoadScreen takes ownership.
        [[nodiscard]] auto LoadScreen(const std::string& name, std::unique_ptr<Screen> screen) -> Screen*;
        [[nodiscard]] auto GetScreen(const std::string& name) const -> Screen*;
        [[nodiscard]] auto GetScreenName(const Screen* screen) const -> std::string;

        auto SwitchScreen(const std::string& name) -> void;
        auto SwitchScreen(Screen* screen) -> void;
        auto PushScreen(const std::string& name) -> void;
        auto PushScreen(Screen* screen) -> void;
        auto PopScreen() -> void;

        auto Render(atom::render::IRenderDevice& device) const -> void;
        auto HandleEvent(const atom::window::IEvent& event) const -> void;
        auto Update(float deltaTime) const -> void;
        auto FixedUpdate(float deltaTime) const -> void;

        [[nodiscard]] auto GetCurrentScreenName() const -> const std::string&;
        [[nodiscard]] auto GetScreenStack() const -> const std::vector<std::pair<std::string, Screen*>>&;

        // True when `screen` is current or on the screen stack (the same set
        // ScreenManager::Render draws). Used by screen-scoped debug panels.
        [[nodiscard]] auto IsScreenVisible(const Screen* screen) const -> bool;
};
} // namespace atom
#endif // ATOM_SCREENMANAGER_HPP
