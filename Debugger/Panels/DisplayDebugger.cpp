// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "DisplayDebugger.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <Backend/Contracts/Render/PresentMode.hpp>
#include <Backend/Contracts/Window/WindowMode.hpp>
#include <Display/DisplaySystem.hpp>
#include <Window/RenderWindow.hpp>

#include <imgui.h>

namespace atom::debugger {
namespace {

constexpr const char* kWindowModeLabels[] = {"Windowed", "Borderless window", "Exclusive fullscreen"};
constexpr const char* kPresentModeLabels[] = {"VSync", "Mailbox", "Immediate (uncapped)"};

// Every placement is offered: a backend that cannot provide one reports failure
// when the change is applied, which the panel then shows as "Applied".
auto WindowModeCombo(const window::WindowMode current) -> std::optional<window::WindowMode> {
    std::optional<window::WindowMode> picked{};
    ImGui::SetNextItemWidth(-170.0f);
    if (ImGui::BeginCombo("Window", kWindowModeLabels[static_cast<std::size_t>(current)])) {
        for (std::size_t index = 0; index < 3; ++index) {
            const auto candidate = static_cast<window::WindowMode>(index);
            const bool selected = candidate == current;
            if (ImGui::Selectable(kWindowModeLabels[index], selected) && !selected)
                picked = candidate;
        }
        ImGui::EndCombo();
    }
    return picked;
}

// Entries the device does not accept stay visible but greyed out: hiding them
// would make "this driver cannot tear" look like "this build forgot the option".
auto PresentModeCombo(const render::PresentMode current, const std::uint8_t supported)
    -> std::optional<render::PresentMode> {
    std::optional<render::PresentMode> picked{};
    ImGui::SetNextItemWidth(-170.0f);
    if (ImGui::BeginCombo("Present", kPresentModeLabels[static_cast<std::size_t>(current)])) {
        for (std::size_t index = 0; index < 3; ++index) {
            const auto candidate = static_cast<render::PresentMode>(index);
            const bool enabled = render::SupportsPresentMode(supported, candidate);
            const bool selected = candidate == current;
            if (!enabled)
                ImGui::BeginDisabled();
            if (ImGui::Selectable(kPresentModeLabels[index], selected) && enabled && !selected)
                picked = candidate;
            if (!enabled)
                ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
    return picked;
}

} // namespace

auto DisplayDebugger::OnDrawOverlay() -> void {
    auto* window = GetTargetWindow();
    auto* device = window != nullptr ? window->GetRenderDevice() : nullptr;

    ImGui::Begin("Display");

    if (window == nullptr || device == nullptr || window->GetIWindow() == nullptr) {
        ImGui::TextDisabled("No render device.");
        ImGui::End();
        return;
    }

    auto& display = atom::display::DisplaySystem::GetInstance();
    // Requests are queued, not applied: the swapchain is reconfigured at the next
    // frame boundary, so the widgets must show the applied value, not the click.
    const auto applied = display.Applied();
    const auto supported = device->SupportedPresentModes();

    if (const auto picked = WindowModeCombo(applied.window_mode))
        display.RequestWindowMode(*picked);
    if (const auto picked = PresentModeCombo(applied.present_mode, supported))
        display.RequestPresentMode(*picked);

    if (display.HasPendingChange())
        ImGui::TextDisabled("Applying at the next frame boundary...");

    ImGui::Separator();

    // Read the live values back from the backend: it, not the request, decides.
    const auto live_present = device->GetPresentMode();
    const bool paced = device->IsPresentRefreshPaced();
    ImGui::Text("Applied: %s  +  %s",
                std::string{window::ToString(window->GetIWindow()->GetWindowMode())}.c_str(),
                std::string{render::ToString(live_present)}.c_str());
    if (paced) {
        ImGui::TextColored(ImVec4{1.0f, 0.8f, 0.3f, 1.0f}, "Refresh-paced: frame rate capped at the display.");
        ImGui::TextDisabled("VSync always caps; Mailbox also caps in exclusive fullscreen");
        ImGui::TextDisabled("(no tear flag there, so the driver falls back to vblank sync).");
    } else {
        ImGui::TextColored(ImVec4{0.4f, 1.0f, 0.4f, 1.0f}, "Not refresh-paced: uncapped (may tear).");
        ImGui::TextDisabled("Still capped? A GPU control-panel VSync override won: check the");
        ImGui::TextDisabled("panel setting before blaming the engine.");
    }

    ImGui::End();
}

} // namespace atom::debugger
