// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#ifndef ATOM_DEBUGGER_IMGUI_FONT_LOADER_HPP
#define ATOM_DEBUGGER_IMGUI_FONT_LOADER_HPP

#include <cstddef>
#include <span>
#include <string_view>

#include <imgui.h>

namespace atom::debugger {

enum class ImGuiGlyphPreset { Default, ChineseFull };

struct ImGuiFontLoadOptions {
        float size_pixels = 16.0f;
        ImGuiGlyphPreset glyph_preset = ImGuiGlyphPreset::Default;
        bool set_as_default = false;
};

// Loads fonts into the current debugger ImGui context. Call after
// Debugger::Attach() initializes ImGui and before RenderWindow::Run().
class ImGuiFontLoader final {
    public:
        [[nodiscard]] static auto LoadFromFile(std::string_view path, const ImGuiFontLoadOptions& options = {})
            -> ImFont*;

        // Copies the supplied bytes into ImGui-owned memory. The caller may release
        // its buffer as soon as this function returns.
        [[nodiscard]] static auto LoadFromMemory(std::span<const std::byte> font_data,
                                                 const ImGuiFontLoadOptions& options = {}) -> ImFont*;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_IMGUI_FONT_LOADER_HPP
