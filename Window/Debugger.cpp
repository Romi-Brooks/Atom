/**
  * @file           : Debugger.cpp
  * @author         : Romi Brooks
  * @brief          : Debug overlay implementation (ImGui SDL3 backend)
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Debugger.hpp"

// Third Party Libraries
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

// Engine Headers
#include <Window/RenderWindow.hpp>

namespace atom {

Debugger::~Debugger() {
    if (attached_) {
        Detach();
    }
}

auto Debugger::Attach(RenderWindow& window) -> void {
    if (attached_)
        return;

    target_window_ = &window;

    void* native_window = window.GetNativeWindowHandle();
    void* native_renderer = window.GetNativeRendererHandle();

    if (!native_window || !native_renderer)
        return;

    // Initialize ImGui SDL3 backend
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLRenderer(static_cast<SDL_Window*>(native_window),
                                      static_cast<SDL_Renderer*>(native_renderer));
    ImGui_ImplSDLRenderer3_Init(static_cast<SDL_Renderer*>(native_renderer));

    // Hook raw SDL event processing (ImGui needs the SDL_Event before translation)
    window.on_pre_process_sdl_event_ = [](const SDL_Event& event) { ImGui_ImplSDL3_ProcessEvent(&event); };

    // Hook per-frame update
    window.on_update_ = [this](float deltaTime) {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Track FPS
        frame_count_++;
        fps_accumulator_ += deltaTime;
        if (fps_accumulator_ >= 1.0f) {
            fps_display_ = static_cast<float>(frame_count_) / fps_accumulator_;
            frame_count_ = 0;
            fps_accumulator_ = 0.0f;
        }
    };

    // Hook overlay render
    window.on_render_overlay_ = [this]() {
        OnDrawOverlay();
        ImGui::Render();
        auto* renderer = static_cast<SDL_Renderer*>(target_window_->GetNativeRendererHandle());
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    };

    // Hook shutdown
    window.on_shutdown_ = [this]() {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        imgui_shutdown_ = true;
    };

    attached_ = true;
}

auto Debugger::Detach() -> void {
    if (!attached_ || !target_window_)
        return;

    // Clear all callbacks
    target_window_->on_pre_process_sdl_event_ = nullptr;
    target_window_->on_process_event_ = nullptr;
    target_window_->on_update_ = nullptr;
    target_window_->on_render_overlay_ = nullptr;
    target_window_->on_shutdown_ = nullptr;

    // Shutdown ImGui SDL3 backend (skip if already done by the shutdown callback)
    if (!imgui_shutdown_) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    target_window_ = nullptr;
    attached_ = false;
}

} // namespace atom
