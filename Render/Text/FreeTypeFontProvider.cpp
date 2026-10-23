/**
 * @file           : FreeTypeFontProvider.cpp
 * @brief          : FreeTypeFontProvider implementation (W2a).
**/

#include "FreeTypeFontProvider.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <Concurrency/JobSystem.hpp>

namespace atom::text {
namespace {

// FreeType bitmap row pointer, handling bottom-up (negative) pitch.
auto BitmapRow(const FT_Bitmap& bitmap, std::uint32_t row) -> const std::uint8_t* {
    const auto stride = static_cast<std::ptrdiff_t>(std::abs(bitmap.pitch));
    if (bitmap.pitch >= 0)
        return bitmap.buffer + static_cast<std::ptrdiff_t>(row) * stride;
    return bitmap.buffer + static_cast<std::ptrdiff_t>(bitmap.rows - 1u - row) * stride;
}

// Maps RenderMode to the FreeType load target that produces the right hinting /
// antialiasing, then to the matching render mode.
auto LoadTarget(RenderMode mode) -> FT_Int32 {
    switch (mode) {
        case RenderMode::Light:
            return FT_LOAD_TARGET_LIGHT;
        case RenderMode::Mono:
            return FT_LOAD_TARGET_MONO;
        case RenderMode::LCD:
            return FT_LOAD_TARGET_LCD;
        case RenderMode::Normal:
        default:
            return FT_LOAD_TARGET_NORMAL;
    }
}

auto RenderModeOf(RenderMode mode) -> FT_Render_Mode {
    switch (mode) {
        case RenderMode::Mono:
            return FT_RENDER_MODE_MONO;
        case RenderMode::LCD:
            return FT_RENDER_MODE_LCD;
        case RenderMode::Light:
        case RenderMode::Normal:
        default:
            return FT_RENDER_MODE_NORMAL;
    }
}

auto PixelHeight(float size_px) -> FT_UInt {
    return static_cast<FT_UInt>(std::max(1.0f, std::round(size_px)));
}

auto PackGray(const FT_Bitmap& bitmap, GlyphRaster& out) -> bool {
    out.format = RasterFormat::AlphaBitmap;
    out.pixels.resize(static_cast<std::size_t>(bitmap.width) * bitmap.rows);
    for (std::uint32_t y = 0; y < bitmap.rows; ++y) {
        const auto* row = BitmapRow(bitmap, y);
        for (std::uint32_t x = 0; x < bitmap.width; ++x)
            out.pixels[static_cast<std::size_t>(y) * bitmap.width + x] = row[x];
    }
    return true;
}

auto PackMono(const FT_Bitmap& bitmap, GlyphRaster& out) -> bool {
    // A 1-bit bitmap is normalized to 8-bit alpha so the consumer has a
    // single well-defined AlphaBitmap packing to reason about.
    out.format = RasterFormat::AlphaBitmap;
    out.pixels.resize(static_cast<std::size_t>(bitmap.width) * bitmap.rows);
    for (std::uint32_t y = 0; y < bitmap.rows; ++y) {
        const auto* row = BitmapRow(bitmap, y);
        for (std::uint32_t x = 0; x < bitmap.width; ++x) {
            const bool set = (row[x / 8u] & (0x80u >> (x % 8u))) != 0;
            out.pixels[static_cast<std::size_t>(y) * bitmap.width + x] = set ? 255 : 0;
        }
    }
    return true;
}

auto PackBgra(const FT_Bitmap& bitmap, GlyphRaster& out) -> bool {
    out.format = RasterFormat::ColorBitmap;
    out.pixels.resize(static_cast<std::size_t>(bitmap.width) * bitmap.rows * 4u);
    for (std::uint32_t y = 0; y < bitmap.rows; ++y) {
        const auto* row = BitmapRow(bitmap, y);
        for (std::uint32_t x = 0; x < bitmap.width; ++x) {
            const auto* src = row + static_cast<std::size_t>(x) * 4u;
            std::memcpy(out.pixels.data() + (static_cast<std::size_t>(y) * bitmap.width + x) * 4u, src, 4u);
        }
    }
    return true;
}

// Rasterizes one glyph on an already-open face. The caller owns the face's
// concurrency (the single-instance path holds the instance mutex; the batch path
// uses a per-job clone). Shared by both so the packing rules stay in one place.
auto RasterizeFace(FT_Face face, std::uint32_t glyph_id, float size_px, RenderMode mode, GlyphRaster& out) -> bool {
    out = {};
    if (!face || !std::isfinite(size_px) || size_px <= 0.0f)
        return false;
    if (FT_Set_Pixel_Sizes(face, 0, PixelHeight(size_px)) != 0 ||
        FT_Load_Glyph(face, glyph_id, FT_LOAD_DEFAULT | LoadTarget(mode)) != 0 ||
        FT_Render_Glyph(face->glyph, RenderModeOf(mode)) != 0)
        return false;

    const auto& bitmap = face->glyph->bitmap;
    if (!bitmap.buffer || bitmap.width == 0 || bitmap.rows == 0)
        return false;

    out.width = bitmap.width;
    out.height = bitmap.rows;
    out.offset_x = face->glyph->bitmap_left;
    out.offset_y = -face->glyph->bitmap_top;

    switch (bitmap.pixel_mode) {
        case FT_PIXEL_MODE_GRAY:
            return PackGray(bitmap, out);
        case FT_PIXEL_MODE_MONO:
            return PackMono(bitmap, out);
        case FT_PIXEL_MODE_BGRA:
            return PackBgra(bitmap, out);
        default:
            return false; // LCD and other modes arrive in a later wave
    }
}

// One FreeType face owned by a FontInstance. The bytes buffer is shared with the
// provider's family table (FT_New_Memory_Face borrows it), so it stays alive for
// the instance's lifetime. A single mutex guards the face: FreeType forbids
// concurrent use of one face, and W2b's batch path avoids this face entirely via
// per-job clones.
class FreeTypeFontInstance final : public FontInstance {
    public:
        FreeTypeFontInstance(FontInstanceId id, float size_px, FT_Face face,
                             std::shared_ptr<const std::vector<std::byte>> bytes)
            : id_(id), size_px_(size_px), face_(face), bytes_(std::move(bytes)) {}

        ~FreeTypeFontInstance() override {
            if (face_)
                FT_Done_Face(face_);
        }

        [[nodiscard]] auto Id() const -> FontInstanceId override {
            return id_;
        }

        [[nodiscard]] auto Bytes() const -> std::span<const std::byte> {
            return bytes_ ? std::span<const std::byte>{*bytes_} : std::span<const std::byte>{};
        }

        [[nodiscard]] auto SizePx() const -> float { return size_px_; }

        [[nodiscard]] auto HasGlyph(std::uint32_t codepoint) const -> bool override {
            return GlyphIndex(codepoint) != 0;
        }

        [[nodiscard]] auto GlyphIndex(std::uint32_t codepoint) const -> std::uint32_t override {
            if (!face_ || codepoint > 0x10ffffu)
                return 0;
            std::scoped_lock lock{mutex_};
            return FT_Get_Char_Index(face_, codepoint);
        }

        [[nodiscard]] auto Metrics(float size_px) const -> FontMetrics override {
            if (!face_ || !std::isfinite(size_px) || size_px <= 0.0f)
                return {};
            std::scoped_lock lock{mutex_};
            if (FT_Set_Pixel_Sizes(face_, 0, PixelHeight(size_px)) != 0 || !face_->size)
                return {};
            const auto& metrics = face_->size->metrics;
            const float ascent = static_cast<float>(metrics.ascender) / 64.0f;
            const float descent = static_cast<float>(metrics.descender) / 64.0f;
            const float line_height = static_cast<float>(metrics.height) / 64.0f;
            return {ascent, descent, std::max(line_height - (ascent - descent), 0.0f)};
        }

        [[nodiscard]] auto ScaleForPixelHeight(float size_px) const -> float override {
            return size_px;
        }

        [[nodiscard]] auto Rasterize(std::uint32_t glyph_id, float size_px, RenderMode mode, GlyphRaster& out) const
            -> bool override {
            std::scoped_lock lock{mutex_};
            return RasterizeFace(face_, glyph_id, size_px, mode, out);
        }

    private:
        FontInstanceId id_;
        float size_px_ = 0.0f;
        FT_Face face_ = nullptr;
        std::shared_ptr<const std::vector<std::byte>> bytes_; // keeps the face's buffer alive
        mutable std::mutex mutex_;
};

// Rasterizes one glyph on a fresh, private FT_Library + FT_Face cloned from the
// family bytes. Each batch job runs this independently, so no FreeType object is
// shared across workers (FreeType forbids concurrent use of one face).
auto RasterizeOne(const std::span<const std::byte> bytes, const std::uint32_t glyph_id, const float size_px,
                  const RenderMode mode) -> RasterizeResult {
    RasterizeResult result{};
    if (bytes.empty())
        return result;

    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0)
        return result;

    FT_Face face = nullptr;
    if (FT_New_Memory_Face(library, reinterpret_cast<const FT_Byte*>(bytes.data()), static_cast<FT_Long>(bytes.size()),
                           0, &face) != 0 ||
        !face) {
        FT_Done_FreeType(library);
        return result;
    }

    result.ok = RasterizeFace(face, glyph_id, size_px, mode, result.raster);

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return result;
}

} // namespace

struct FreeTypeFontProvider::Impl {
        FT_Library library = nullptr;
        std::unordered_map<std::string, std::shared_ptr<const std::vector<std::byte>>> families{};
        std::unordered_map<std::uint32_t, std::unique_ptr<FreeTypeFontInstance>> instances{};
        std::uint32_t next_id = 1;
        std::mutex mutex{};

        ~Impl() {
            instances.clear(); // FT_Done_Face before the library goes away
            if (library)
                FT_Done_FreeType(library);
        }
};

FreeTypeFontProvider::FreeTypeFontProvider() : impl_(std::make_unique<Impl>()) {
    FT_Init_FreeType(&impl_->library);
}

FreeTypeFontProvider::~FreeTypeFontProvider() = default;

auto FreeTypeFontProvider::AddFamily(const std::string_view family, const std::span<const std::byte> bytes) -> bool {
    if (family.empty() || bytes.empty() || !impl_->library)
        return false;

    auto owned = std::make_shared<const std::vector<std::byte>>(bytes.begin(), bytes.end());

    // Validate the face eagerly so a bad family is rejected here, not at the
    // first CreateInstance.
    FT_Face probe = nullptr;
    if (FT_New_Memory_Face(impl_->library, reinterpret_cast<const FT_Byte*>(owned->data()),
                           static_cast<FT_Long>(owned->size()), 0, &probe) != 0 ||
        !probe) {
        return false;
    }
    FT_Done_Face(probe);

    std::lock_guard lock{impl_->mutex};
    impl_->families[std::string{family}] = std::move(owned);
    return true;
}

auto FreeTypeFontProvider::CreateInstance(const std::string_view family, const float size_px,
                                          const std::string_view /*variation*/) -> FontInstanceId {
    if (!impl_->library)
        return FontInstanceId{};

    const float effective_size = std::isfinite(size_px) && size_px > 0.0f ? size_px : 16.0f;

    std::shared_ptr<const std::vector<std::byte>> bytes;
    {
        std::lock_guard lock{impl_->mutex};
        const auto it = impl_->families.find(std::string{family});
        if (it == impl_->families.end())
            return FontInstanceId{};
        bytes = it->second;
    }

    FT_Face face = nullptr;
    if (FT_New_Memory_Face(impl_->library, reinterpret_cast<const FT_Byte*>(bytes->data()),
                           static_cast<FT_Long>(bytes->size()), 0, &face) != 0 ||
        !face) {
        return FontInstanceId{};
    }

    std::lock_guard lock{impl_->mutex};
    const FontInstanceId id{impl_->next_id++};
    impl_->instances.emplace(id.value,
                             std::make_unique<FreeTypeFontInstance>(id, effective_size, face, std::move(bytes)));
    return id;
}

auto FreeTypeFontProvider::FallbackFor(const FontInstanceId /*instance*/, const std::uint32_t /*codepoint*/)
    -> FontInstanceId {
    // No fallback chain yet: a single bundled family per provider is W2a's
    // scope; fallback lands in W4.
    return FontInstanceId{};
}

auto FreeTypeFontProvider::GetInstance(const FontInstanceId id) -> FontInstance* {
    std::lock_guard lock{impl_->mutex};
    const auto it = impl_->instances.find(id.value);
    return it != impl_->instances.end() ? it->second.get() : nullptr;
}

auto FreeTypeFontProvider::GetInstance(const FontInstanceId id) const -> const FontInstance* {
    std::lock_guard lock{impl_->mutex};
    const auto it = impl_->instances.find(id.value);
    return it != impl_->instances.end() ? it->second.get() : nullptr;
}

auto FreeTypeFontProvider::BytesFor(const FontInstanceId id) const -> std::span<const std::byte> {
    std::lock_guard lock{impl_->mutex};
    const auto it = impl_->instances.find(id.value);
    return it != impl_->instances.end() ? it->second->Bytes() : std::span<const std::byte>{};
}

auto FreeTypeFontProvider::SizePxFor(const FontInstanceId id) const -> float {
    std::lock_guard lock{impl_->mutex};
    const auto it = impl_->instances.find(id.value);
    return it != impl_->instances.end() ? it->second->SizePx() : 0.0f;
}

auto FreeTypeFontProvider::RasterizeBatch(const std::span<const RasterizeRequest> requests)
    -> std::vector<RasterizeResult> {
    std::vector<RasterizeResult> results(requests.size());

    // Pre-resolve the family bytes on the calling thread (a short mutex hold),
    // so each worker only does read-only rasterization. The spans are valid for
    // the provider's lifetime, which outlives this blocking call.
    std::vector<std::span<const std::byte>> byte_spans(requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i)
        byte_spans[i] = BytesFor(requests[i].instance);

    auto& jobs = atom::job::JobSystem::GetInstance();
    if (!jobs.IsRunning()) {
        // Scheduler not started: fall back to sequential rasterization so callers
        // never have to reason about scheduler state.
        for (std::size_t i = 0; i < requests.size(); ++i)
            results[i] = RasterizeOne(byte_spans[i], requests[i].glyph_id, requests[i].size_px, requests[i].mode);
        return results;
    }

    // One job per request, each cloning its own FT_Library + FT_Face (no shared
    // FreeType object across workers). Distinct result indices mean no data race.
    std::vector<atom::job::JobHandle> handles;
    handles.reserve(requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i) {
        const auto bytes = byte_spans[i];
        const RasterizeRequest request = requests[i];
        handles.push_back(jobs.Schedule("text:rasterize", [bytes, request, &results, i] {
            results[i] = RasterizeOne(bytes, request.glyph_id, request.size_px, request.mode);
        }));
    }
    jobs.WaitAll(handles);
    return results;
}

} // namespace atom::text
