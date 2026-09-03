// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "ImGuiFontLoader.hpp"

#include <cstring>
#include <limits>
#include <string>

#include <Log/LogSystem.hpp>

namespace atom::debugger {
namespace {
[[nodiscard]] auto ResolveGlyphRanges(ImFontAtlas& atlas, const ImGuiGlyphPreset preset) -> const ImWchar* {
    switch (preset) {
    case ImGuiGlyphPreset::ChineseFull:
        return atlas.GetGlyphRangesChineseFull();
    case ImGuiGlyphPreset::Default:
        return atlas.GetGlyphRangesDefault();
    }
    return atlas.GetGlyphRangesDefault();
}

[[nodiscard]] auto PresetName(const ImGuiGlyphPreset preset) -> std::string_view {
    return preset == ImGuiGlyphPreset::ChineseFull ? "ChineseFull" : "Default";
}

auto ApplyDefaultFont(ImGuiIO& io, ImFont* font, const bool set_as_default) -> ImFont* {
    if (font != nullptr && set_as_default) {
        io.FontDefault = font;
    }
    return font;
}
} // namespace

auto ImGuiFontLoader::LoadFromFile(const std::string_view path, const ImGuiFontLoadOptions& options) -> ImFont* {
    if (ImGui::GetCurrentContext() == nullptr) {
        LOG_ERROR(LogChannel::IMGUI, "Cannot load font file before the debugger ImGui context is initialized");
        return nullptr;
    }
    if (path.empty() || options.size_pixels <= 0.0f) {
        LOG_WARNING(LogChannel::IMGUI, "Rejected invalid font file path or non-positive pixel size");
        return nullptr;
    }

    auto& io = ImGui::GetIO();
    const auto null_terminated_path = std::string{path};
    auto* font = io.Fonts->AddFontFromFileTTF(null_terminated_path.c_str(), options.size_pixels, nullptr,
                                              ResolveGlyphRanges(*io.Fonts, options.glyph_preset));
    if (font == nullptr) {
        LOG_ERROR(LogChannel::IMGUI, "Failed to load ImGui font file: " + null_terminated_path);
        return nullptr;
    }

    LOG_INFO(LogChannel::IMGUI, "Loaded ImGui font file: " + null_terminated_path +
                                    " (size=" + std::to_string(options.size_pixels) +
                                    ", glyphs=" + std::string{PresetName(options.glyph_preset)} +
                                    ", default=" + (options.set_as_default ? "true" : "false") + ")");
    return ApplyDefaultFont(io, font, options.set_as_default);
}

auto ImGuiFontLoader::LoadFromMemory(const std::span<const std::byte> font_data, const ImGuiFontLoadOptions& options)
    -> ImFont* {
    if (ImGui::GetCurrentContext() == nullptr) {
        LOG_ERROR(LogChannel::IMGUI, "Cannot load memory font before the debugger ImGui context is initialized");
        return nullptr;
    }
    if (font_data.empty() || options.size_pixels <= 0.0f ||
        font_data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        LOG_WARNING(LogChannel::IMGUI, "Rejected empty/oversized memory font or non-positive pixel size");
        return nullptr;
    }

    auto* owned_data = IM_ALLOC(font_data.size());
    if (owned_data == nullptr) {
        LOG_ERROR(LogChannel::IMGUI, "Failed to allocate ImGui-owned memory for font data");
        return nullptr;
    }
    std::memcpy(owned_data, font_data.data(), font_data.size());

    auto& io = ImGui::GetIO();
    auto* font = io.Fonts->AddFontFromMemoryTTF(owned_data, static_cast<int>(font_data.size()), options.size_pixels,
                                                nullptr, ResolveGlyphRanges(*io.Fonts, options.glyph_preset));
    if (font == nullptr) {
        LOG_ERROR(LogChannel::IMGUI, "Failed to register the copied memory font with ImGui");
        return nullptr;
    }

    LOG_INFO(LogChannel::IMGUI, "Loaded ImGui font from memory (bytes=" + std::to_string(font_data.size()) +
                                    ", size=" + std::to_string(options.size_pixels) +
                                    ", glyphs=" + std::string{PresetName(options.glyph_preset)} +
                                    ", default=" + (options.set_as_default ? "true" : "false") + ")");
    return ApplyDefaultFont(io, font, options.set_as_default);
}

} // namespace atom::debugger
