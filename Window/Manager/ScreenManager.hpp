/**
  * @file           : WindowsManager.hpp
  * @author         : Romi Brooks
  * @brief          :
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
            std::unordered_map<std::string, std::unique_ptr<Screen>> screens_ {};
            Screen* current_screen_ = nullptr;
            std::string current_screen_name_ {};
            std::vector<std::pair<std::string, Screen*>> screen_stack_;

            ScreenManager() = default;
            ~ScreenManager() = default;

        public:
            ScreenManager(const ScreenManager&) = delete;
            ScreenManager& operator=(const ScreenManager&) = delete;

            [[nodiscard]] static auto GetInstance() -> ScreenManager&;

            auto LoadScreen(const std::string& name, std::unique_ptr<Screen> screen) -> void;
            auto SwitchScreen(const std::string& name) -> void;
            auto PushScreen(const std::string& name) -> void;
            auto PopScreen() -> void;

            auto Render(IRenderTarget& target) const -> void;
            auto HandleEvent(const IEvent& event) const -> void;
            auto Update(float deltaTime) const -> void;

            [[nodiscard]] auto GetCurrentScreenName() const -> const std::string&;
            [[nodiscard]] auto GetScreenStack() const -> const std::vector<std::pair<std::string, Screen*>>&;
    };
}
#endif // ATOM_SCREENMANAGER_HPP
