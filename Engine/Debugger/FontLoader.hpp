// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file FontLoader.hpp
 * @brief Debugger-only ImGui font atlas helper (not a panel, not a general font API).
 */

#ifndef ATOM_DEBUGGER_FONT_LOADER_HPP
#define ATOM_DEBUGGER_FONT_LOADER_HPP

#include <cstddef>
#include <span>
#include <string_view>

#include <imgui.h>

namespace atom::debugger {

enum class GlyphPreset { Default, ChineseFull };

struct FontLoadOptions {
        float size_pixels = 16.0f;
        GlyphPreset glyph_preset = GlyphPreset::Default;
        bool set_as_default = false;
};

// Loads fonts into the current debugger ImGui context. Not a DebugPanel —
// panels may call it during setup. Production UI text uses Renderer2D.
class FontLoader final {
    public:
        [[nodiscard]] static auto LoadFromFile(std::string_view path, const FontLoadOptions& options = {})
            -> ImFont*;

        [[nodiscard]] static auto LoadFromMemory(std::span<const std::byte> font_data,
                                                 const FontLoadOptions& options = {}) -> ImFont*;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_FONT_LOADER_HPP
