// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "PanelLayout.hpp"

#include <imgui.h>

namespace atom::debugger {

auto ApplyPanelSlot(const float pos_x, const float pos_y, const float size_w, const float size_h) -> void {
    ImGui::SetNextWindowPos(ImVec2{pos_x, pos_y}, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2{size_w, size_h}, ImGuiCond_Once);
}

auto ApplyLogPanelSlot() -> void {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width = viewport ? viewport->WorkSize.x : 1280.0f;
    const float height = viewport ? viewport->WorkSize.y : 720.0f;
    const float margin = 16.0f;
    const float pos_y = height * 0.45f;
    ApplyPanelSlot(margin, pos_y, width - margin * 2.0f, height - pos_y - margin);
}

auto ApplyStatusPanelSlot() -> void {
    ApplyPanelSlot(16.0f, 16.0f, 320.0f, 180.0f);
}

} // namespace atom::debugger
