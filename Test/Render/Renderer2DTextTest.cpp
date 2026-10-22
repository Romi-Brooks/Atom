/**
 * @file           : Renderer2DTextTest.cpp
 * @brief          : Glyph atlas submission and sampler regression coverage.
 */

#include <Backend/Contracts/Render/IRender2DContext.hpp>
#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Render/Text/Font.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <vector>

namespace {

[[nodiscard]] auto ReadBinary(const std::filesystem::path& path) -> std::vector<std::byte> {
    std::ifstream input{path, std::ios::binary};
    if (!input)
        return {};
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    std::vector<std::byte> result(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i)
        result[i] = static_cast<std::byte>(bytes[i]);
    return result;
}

class RecordingDevice final : public atom::render::IRenderDevice, public atom::render::IRender2DContext {
    public:
        [[nodiscard]] auto BeginFrame() -> bool override { return true; }
        auto Clear(const atom::render::Color&) -> void override {}
        auto EndFrame() -> void override {}
        auto HandleResize(uint32_t, uint32_t) -> void override {}
        auto SetVSync(bool) -> bool override { return true; }
        [[nodiscard]] auto IsVSyncEnabled() const -> bool override { return false; }
        [[nodiscard]] auto GetOutputSize() const -> atom::algo::Vec2 override { return {1280.0f, 720.0f}; }
        [[nodiscard]] auto GetBackendInfo() const -> const atom::render::RenderBackendInfo& override { return info_; }

        auto Initialize2D(const std::filesystem::path&) -> bool override { return true; }
        auto CreateTexture2D(uint32_t, uint32_t) -> atom::render::Texture2D override { return next_texture_++; }
        auto UpdateTexture2D(atom::render::Texture2D, const void*, uint32_t) -> bool override { return true; }
        auto UpdateTexture2DRegion(atom::render::Texture2D, uint32_t, uint32_t, uint32_t width, uint32_t height,
                                   const void*, uint32_t) -> bool override {
            ++region_uploads_;
            uploads_.push_back({width, height});
            return true;
        }
        auto DestroyTexture2D(atom::render::Texture2D) -> void override {}
        auto CreateSampler2D(const atom::render::Sampler2DDesc& desc) -> atom::render::Sampler2D override {
            samplers_.push_back(desc);
            return static_cast<atom::render::Sampler2D>(samplers_.size());
        }
        auto DestroySampler2D(atom::render::Sampler2D) -> void override {}
        auto SetPostProcess2D(const atom::render::PostProcess2DParams&) -> void override {}
        auto ResolvePostProcess2D() -> bool override { return true; }
        auto Submit2DFrame(const atom::render::Render2DFrame& frame) -> bool override {
            submitted_items_.assign(frame.items, frame.items + frame.item_count);
            return true;
        }
        [[nodiscard]] auto GetMax2DVertices() const -> uint32_t override { return 65536; }
        [[nodiscard]] auto GetMax2DIndices() const -> uint32_t override { return 65536; }

        [[nodiscard]] auto Sampler(atom::render::Sampler2D handle) const -> const atom::render::Sampler2DDesc& {
            return samplers_.at(static_cast<std::size_t>(handle - 1u));
        }
        [[nodiscard]] auto SubmittedItems() const -> const std::vector<atom::render::DrawItem2D>& {
            return submitted_items_;
        }
        [[nodiscard]] auto RegionUploads() const -> uint32_t { return region_uploads_; }
        [[nodiscard]] auto Uploads() const -> const std::vector<std::pair<uint32_t, uint32_t>>& { return uploads_; }

    private:
        atom::render::RenderBackendInfo info_{};
        atom::render::Texture2D next_texture_ = 1;
        std::vector<atom::render::Sampler2DDesc> samplers_{};
        std::vector<atom::render::DrawItem2D> submitted_items_{};
        std::vector<std::pair<uint32_t, uint32_t>> uploads_{};
        uint32_t region_uploads_ = 0;
};

} // namespace

auto main() -> int {
    const auto bytes = ReadBinary(std::filesystem::path{ATOM_SOURCE_ROOT} / "ThirdParty/HarfBuzz/perf/fonts/Roboto-Regular.ttf");
    if (bytes.empty())
        return atom::test::Fail("Pinned font fixture is unavailable");

    RecordingDevice device{};
    atom::render::Renderer2D renderer{};
    ATOM_CHECK(renderer.Initialize(device, "unused-shader-root"));
    const auto* font = renderer.LoadFontFromMemory(bytes);
    ATOM_CHECK(font);
    ATOM_CHECK(renderer.BeginFrame());
    renderer.DrawText(*font, "Atom AV", 20.0f, 30.0f, atom::render::Color::White(), 21.0f);
    ATOM_CHECK(renderer.EndFrame());
    ATOM_CHECK(device.RegionUploads() >= 2); // white fallback + first glyph page
    // Glyph atlas mutations must upload a complete page until the engine owns
    // a backend-verified, versioned subresource upload queue.
    bool saw_complete_glyph_page = false;
    for (const auto& [width, height] : device.Uploads())
        saw_complete_glyph_page = saw_complete_glyph_page || (width == 1024 && height == 1024);
    ATOM_CHECK(saw_complete_glyph_page);

    bool saw_glyph_draw = false;
    for (const auto& item : device.SubmittedItems()) {
        if (item.texture == 1) // the renderer's white fallback texture
            continue;
        saw_glyph_draw = true;
        const auto& sampler = device.Sampler(item.sampler);
        ATOM_CHECK(sampler.min_filter == atom::render::Filter2D::Nearest);
        ATOM_CHECK(sampler.mag_filter == atom::render::Filter2D::Nearest);
    }
    ATOM_CHECK(saw_glyph_draw);

    // MeasureText runs the same layout state machine as DrawText, so a
    // non-empty run reports a positive advance, and wrapping at a narrow width
    // produces extra lines (and therefore more height).
    const auto single_line = renderer.MeasureText(*font, "Atom AV", 21.0f, 0.0f);
    ATOM_CHECK(single_line.width > 0.0f);
    ATOM_CHECK(single_line.height > 0.0f);

    const auto wrapped = renderer.MeasureText(*font, "Atom AV", 21.0f, 1.0f);
    ATOM_CHECK(wrapped.height > single_line.height);
    return 0;
}
