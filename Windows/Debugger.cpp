/**
  * @file           : Debugger.cpp
  * @author         : Romi Brooks
  * @brief          : Debug overlay implementation (ImGui-SFML backend)
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Debugger.hpp"

// Third Party Library
#include <imgui-SFML.h>

// Engine Headers
#include <Windows/RenderWindow.hpp>

namespace atom {

Debugger::~Debugger() {
    if (attached_) {
        Detach();
    }
}

auto Debugger::Attach(RenderWindow& window) -> void {
    if (attached_) {
        return;
    }

    target_window_ = &window;
    auto& sfmlWindow = window.GetWindow();

    // Initialize ImGui-SFML
    if (const auto result = ImGui::SFML::Init(sfmlWindow); result == false) {
        return;
    }

    // Attach all callbacks to the given window
    window.on_process_event_ =
        [](sf::RenderWindow& w, const sf::Event& event) {
            ImGui::SFML::ProcessEvent(w, event);
        };

    window.on_update_ =
        [this, &window](sf::Time deltaTime) {
            ImGui::SFML::Update(window.GetWindow(), deltaTime);
        };

    window.on_render_overlay_ =
        [this](sf::RenderWindow& w) {
            OnDrawOverlay();
            ImGui::SFML::Render(w);
        };

    window.on_shutdown_ =
        [] {
            ImGui::SFML::Shutdown();
        };

    attached_ = true;
}

auto Debugger::Detach() -> void {
    if (!attached_ || !target_window_) {
        return;
    }

    // Clear all callbacks
    target_window_->on_process_event_ = nullptr;
    target_window_->on_update_ = nullptr;
    target_window_->on_render_overlay_ = nullptr;
    target_window_->on_shutdown_ = nullptr;

    // Shutdown ImGui-SFML
    ImGui::SFML::Shutdown();

    target_window_ = nullptr;
    attached_ = false;
}

} // namespace atom
