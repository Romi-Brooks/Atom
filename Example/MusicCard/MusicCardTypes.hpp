/**
 * @file           : MusicCardTypes.hpp
 * @brief          : Shared types and helpers for the MusicCard example: theme,
 *                   track record, corner/animation enums, input bindings, and
 *                   the Renderer2D-backed painter.
 *
 * Split out of the original single-file MusicCard.cpp so the screen, the
 * debugger panel and the entry point each stay focused.
 */

#ifndef ATOM_EXAMPLE_MUSIC_CARD_TYPES_HPP
#define ATOM_EXAMPLE_MUSIC_CARD_TYPES_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Algorithm/Color/ColorMath.hpp>
#include <Algorithm/Geometry/Rect.hpp>
#include <Input/ActionMap.hpp>
#include <Filesystem/AssetPath.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Image/Analysis/ColorStatistics.hpp>
#include <Media/Image/ImageDecoder.hpp>
#include <Render/Resources/TextureCache.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>

// The example scans this directory for audio files. A command-line argument
// can override it for focused cover/shader diagnostics.
inline constexpr auto MusicPath = R"(E:\Music\)";
// Optional presentation background for the example. If this developer-local
// asset is unavailable, the procedural gradient remains the fallback.
inline constexpr auto WallpaperPath = R"(C:\Users\Romi\Pictures\Wallpapper\3177905.jpg)";

enum class MusicCardAction { Quit, Previous, Next, TogglePlayback, ToggleCorner, ToggleVisibility };

[[nodiscard]] inline auto GetMusicCardBindings() -> const atom::input::ActionMap<MusicCardAction>& {
    static const auto bindings = [] {
        atom::input::ActionMap<MusicCardAction> result;
        result.Bind({atom::input::Key::Escape}, MusicCardAction::Quit);
        result.Bind({atom::input::Key::Left}, MusicCardAction::Previous);
        result.Bind({atom::input::Key::Right}, MusicCardAction::Next);
        result.Bind({atom::input::Key::Space}, MusicCardAction::TogglePlayback);
        result.Bind({atom::input::Key::C}, MusicCardAction::ToggleCorner);
        result.Bind({atom::input::Key::H}, MusicCardAction::ToggleVisibility);
        return result;
    }();
    return bindings;
}

// This is presentation policy for this sample, not a reusable image-analysis
// result. It turns neutral ColorStatistics into the three colors the card UI
// chooses to render.
struct MusicCardTheme {
    atom::render::Color primary;
    atom::render::Color secondary;
    atom::render::Color accent;
};

struct Track {
    std::string id;
    std::string path;
    // VFS identity of the track, resolved from the filename inside the
    // mounted music directory. Used by MusicPlayer::Load(IFileSystem, ...)
    // and AudioMetadataReader::Read(IFileSystem, ...); `path` stays only for
    // display and filesystem diagnostics.
    atom::fs::AssetPath asset_path;
    MusicCardTheme theme;
    // Lazy-loaded fields (protected by MusicCardScreen::tracks_mutex_).
    // metadata_loaded is set to true only after all fields below are written.
    bool metadata_loaded = false;
    std::string title;
    std::string artist;
    bool is_loaded = false;
    std::string artwork_mime_type;
    std::vector<uint8_t> artwork_data;
};

enum class CardCorner { BottomLeft, TopRight };

enum class AnimationState { Entering, Visible, Exiting, Hidden };

[[nodiscard]] inline auto BuildMusicCardTheme(const atom::image::DecodedImage& artwork) -> std::optional<MusicCardTheme> {
    const auto extracted = atom::image::CalculateColorStatistics(artwork);
    if (!extracted.has_value())
        return std::nullopt;
    // The extractor returns neutral image statistics. This sample's darker
    // panel and brighter secondary color remain a presentation/theme policy.
    return MusicCardTheme{atom::color::Blend(extracted->average, atom::render::Color::Black(), 0.48f),
                          atom::color::Blend(extracted->average, atom::render::Color::White(), 0.12f),
                          extracted->maximum};
}

// Atom's TextureCache owns GPU textures with last-handle + frame-boundary
// reclaim. This sample wrapper only supplies MusicCard-specific diagnostics
// and treats an empty result as the procedural-cover fallback.
[[nodiscard]] inline auto LoadArtworkTexture(atom::render::resources::TextureCache& cache, const std::string& cache_key,
                                             const std::vector<uint8_t>& encoded_image)
    -> atom::render::resources::TextureHandle {
    if (encoded_image.empty()) {
        LOG_DEBUG(atom::log::audio::Metadata, "No embedded artwork bytes; using the generated cover fallback");
        return {};
    }
    const auto bytes = std::as_bytes(std::span{encoded_image});
    auto handle = cache.AcquireFromEncodedMemory(cache_key, bytes);
    if (handle) {
        LOG_INFO(atom::log::audio::Metadata, "Decoded embedded artwork into a cached Renderer2D texture (" +
                                                 std::to_string(handle.GetWidth()) + "x" +
                                                 std::to_string(handle.GetHeight()) +
                                                 ", bytes=" + std::to_string(encoded_image.size()) + ")");
    } else {
        LOG_ERROR(atom::log::audio::Metadata, "Failed to create Renderer2D texture for decoded artwork");
    }
    return handle;
}

// Renderer2D-backed painter. All drawing stays in atom::render::Renderer2D
// coordinates (pixels, top-left origin, y-down), matching the Yoga layout
// output that the original SDL_Renderer painter used.
class CardPainter {
    public:
        explicit CardPainter(atom::render::Renderer2D& renderer) : renderer_{renderer} {}

        auto FillRect(const atom::algo::Rect& rect, const atom::render::Color color) -> void {
            renderer_.DrawRect(rect, color);
        }

        auto FillRoundedRect(const atom::algo::Rect& rect, const float radius, const atom::render::Color color)
            -> void {
            const auto safe_radius = std::min({radius, rect.width * 0.5f, rect.height * 0.5f});
            if (safe_radius <= 0.0f) {
                FillRect(rect, color);
                return;
            }
            // Cross body + quarter discs at each corner (clipped to the corner
            // squares) reproduces the original rounded-rect look.
            renderer_.DrawRect(
                atom::algo::Rect{rect.x + safe_radius, rect.y, rect.width - safe_radius * 2.0f, rect.height}, color);
            renderer_.DrawRect(
                atom::algo::Rect{rect.x, rect.y + safe_radius, rect.width, rect.height - safe_radius * 2.0f}, color);
            const std::array<atom::algo::Rect, 4> corners{{
                {rect.x, rect.y, safe_radius, safe_radius},
                {rect.x + rect.width - safe_radius, rect.y, safe_radius, safe_radius},
                {rect.x, rect.y + rect.height - safe_radius, safe_radius, safe_radius},
                {rect.x + rect.width - safe_radius, rect.y + rect.height - safe_radius, safe_radius, safe_radius},
            }};
            const std::array<std::pair<float, float>, 4> centers{{
                {rect.x, rect.y},
                {rect.x + rect.width, rect.y},
                {rect.x, rect.y + rect.height},
                {rect.x + rect.width, rect.y + rect.height},
            }};
            for (std::size_t i = 0; i < corners.size(); ++i) {
                renderer_.PushClip(corners[i]);
                renderer_.DrawCircle(centers[i].first, centers[i].second, safe_radius, color);
                renderer_.PopClip();
            }
        }

        auto FillCircle(const float center_x, const float center_y, const float radius, const atom::render::Color color)
            -> void {
            renderer_.DrawCircle(center_x, center_y, radius, color);
        }

        auto DrawLine(const float x0, const float y0, const float x1, const float y1, const atom::render::Color color,
                      const float thickness = 1.0f) -> void {
            renderer_.DrawLine(x0, y0, x1, y1, color, thickness);
        }

        auto DrawTexture(atom::render::Renderer2D::Texture& texture, const atom::algo::Rect& rect, const float opacity)
            -> void {
            renderer_.DrawTexture(texture, rect, atom::color::ApplyOpacity(atom::render::Color::White(), opacity),
                                  nullptr);
        }

    private:
        atom::render::Renderer2D& renderer_;
};

#endif // ATOM_EXAMPLE_MUSIC_CARD_TYPES_HPP
