// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file MusicCard.cpp
 * @brief Yoga + Renderer2D music HUD card (ported from the retired
 *        SDL_Renderer painter; phase C migration).
 * @author Romi Brooks
 * @date 2026/09/01
 * @attention Temporary widgets and animation helpers intentionally live only
 *            in this example.
 */

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Layout/LayoutTree.hpp>
#include <Layout/LayoutTypes.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Metadata/AudioMetadataReader.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Media/Image/ImageDecoder.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Render/Text/Font.hpp>
#include <Window/Debugger/ImGui/ImGuiFontLoader.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Overlay.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>

#ifdef _WIN32
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#endif // _WIN32

namespace {

// The example scans this directory for audio files. A command-line argument
// can override it for focused cover/shader diagnostics.
constexpr auto MusicPath = R"(E:\Music\)";
// Optional presentation background for the example. If this developer-local
// asset is unavailable, the procedural gradient remains the fallback.
constexpr auto WallpaperPath = R"(C:\Users\xxx\Pictures\Wallpapper\3177905.jpg)";

struct FloatRect {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
};

struct Palette {
        atom::render::Color primary;
        atom::render::Color secondary;
        atom::render::Color accent;
};

struct Track {
        std::string id;
        std::string path;
        Palette palette;
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

[[nodiscard]] auto Clamp01(const float value) -> float {
    return std::clamp(value, 0.0f, 1.0f);
}

[[nodiscard]] auto EaseOutCubic(const float value) -> float {
    const auto inverse = 1.0f - Clamp01(value);
    return 1.0f - inverse * inverse * inverse;
}

[[nodiscard]] auto EaseInCubic(const float value) -> float {
    const auto clamped = Clamp01(value);
    return clamped * clamped * clamped;
}

[[nodiscard]] auto EaseOutBack(const float value) -> float {
    constexpr auto overshoot = 1.70158f;
    const auto shifted = Clamp01(value) - 1.0f;
    return 1.0f + (overshoot + 1.0f) * shifted * shifted * shifted + overshoot * shifted * shifted;
}

[[nodiscard]] auto Stagger(const float progress, const float start, const float end) -> float {
    return Clamp01((progress - start) / (end - start));
}

[[nodiscard]] auto WithAlpha(atom::render::Color color, const float opacity) -> atom::render::Color {
    color.a = static_cast<uint8_t>(static_cast<float>(color.a) * Clamp01(opacity));
    return color;
}

[[nodiscard]] auto MixColor(const atom::render::Color a, const atom::render::Color b, const float amount)
    -> atom::render::Color {
    const auto t = Clamp01(amount);
    return {static_cast<uint8_t>(a.r + (b.r - a.r) * t), static_cast<uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<uint8_t>(a.b + (b.b - a.b) * t), static_cast<uint8_t>(a.a + (b.a - a.a) * t)};
}

[[nodiscard]] auto ExtractPaletteFromRgba(const std::vector<uint8_t>& rgba) -> std::optional<Palette> {
    if (rgba.size() < 4 || rgba.size() % 4 != 0)
        return std::nullopt;
    uint64_t sum_r = 0, sum_g = 0, sum_b = 0, count = 0;
    uint8_t accent_r = 132, accent_g = 219, accent_b = 214;
    float best_accent_score = -1.0f;
    // Sampling every fourth pixel is sufficient for a 640x640 cover and
    // avoids doing work proportional to the full image on the render thread.
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 16) {
        const auto alpha = rgba[i + 3];
        if (alpha < 32)
            continue;
        const auto r = rgba[i];
        const auto g = rgba[i + 1];
        const auto b = rgba[i + 2];
        sum_r += r;
        sum_g += g;
        sum_b += b;
        ++count;
        const auto high = static_cast<float>(std::max({r, g, b}));
        const auto low = static_cast<float>(std::min({r, g, b}));
        const auto saturation = (high - low) / std::max(high, 1.0f);
        const auto score = saturation * (high / 255.0f);
        if (score > best_accent_score) {
            best_accent_score = score;
            accent_r = r;
            accent_g = g;
            accent_b = b;
        }
    }
    if (count == 0)
        return std::nullopt;
    const auto average = atom::render::Color{static_cast<uint8_t>(sum_r / count),
                                              static_cast<uint8_t>(sum_g / count),
                                              static_cast<uint8_t>(sum_b / count), 255};
    const auto primary = atom::render::Color{static_cast<uint8_t>(average.r * 0.52f),
                                              static_cast<uint8_t>(average.g * 0.52f),
                                              static_cast<uint8_t>(average.b * 0.52f), 255};
    const auto secondary = MixColor(average, atom::render::Color::White(), 0.12f);
    const auto accent = atom::render::Color{accent_r, accent_g, accent_b, 255};
    return Palette{primary, secondary, accent};
}

[[nodiscard]] auto ToRect(const FloatRect& rect) -> atom::render::Rect {
    return {rect.x, rect.y, rect.width, rect.height};
}

[[nodiscard]] auto OffsetRect(const FloatRect& rect, const float x, const float y) -> FloatRect {
    return {rect.x + x, rect.y + y, rect.width, rect.height};
}

[[nodiscard]] auto ScaleRectFromCenter(const FloatRect& rect, const float scale) -> FloatRect {
    const auto width = rect.width * scale;
    const auto height = rect.height * scale;
    return {rect.x + (rect.width - width) * 0.5f, rect.y + (rect.height - height) * 0.5f, width, height};
}

[[nodiscard]] auto Contains(const FloatRect& rect, const float x, const float y) -> bool {
    return x >= rect.x && x <= rect.x + rect.width && y >= rect.y && y <= rect.y + rect.height;
}

// Decodes the embedded cover art through the backend-agnostic Atom_Image
// module and hands the RGBA pixels to Renderer2D (CPU decode and GPU upload
// stay separate). Returns nullptr when no artwork is available.
[[nodiscard]] auto CreateArtworkTexture(atom::render::Renderer2D& renderer, const std::vector<uint8_t>& encoded_image)
    -> atom::render::Renderer2D::Texture* {
    if (encoded_image.empty()) {
        LOG_DEBUG(atom::audio::LogChannel::METADATA, "No embedded artwork bytes; using the generated cover fallback");
        return nullptr;
    }
    const auto bytes = std::as_bytes(std::span{encoded_image});
    auto decoded = atom::image::DecodeImageMemory(bytes);
    if (!decoded.IsValid()) {
        LOG_ERROR(atom::audio::LogChannel::METADATA,
                  "Failed to decode embedded artwork (bytes=" + std::to_string(encoded_image.size()) + ")");
        return nullptr;
    }
    auto* texture = renderer.CreateTexture(decoded.width, decoded.height, decoded.rgba.data());
    if (texture != nullptr) {
        LOG_INFO(atom::audio::LogChannel::METADATA,
                 "Decoded embedded artwork into a Renderer2D texture (" + std::to_string(decoded.width) + "x" +
                     std::to_string(decoded.height) + ", bytes=" + std::to_string(encoded_image.size()) + ")");
    } else {
        LOG_ERROR(atom::audio::LogChannel::METADATA, "Failed to create Renderer2D texture for decoded artwork");
    }
    return texture;
}

// Renderer2D-backed painter. All drawing stays in atom::render::Renderer2D
// coordinates (pixels, top-left origin, y-down), matching the Yoga layout
// output that the original SDL_Renderer painter used.
class CardPainter {
    public:
        explicit CardPainter(atom::render::Renderer2D& renderer) : renderer_{renderer} {}

        auto FillRect(const FloatRect& rect, const atom::render::Color color) -> void {
            renderer_.DrawRect(ToRect(rect), color);
        }

        auto FillRoundedRect(const FloatRect& rect, const float radius, const atom::render::Color color) -> void {
            const auto safe_radius = std::min({radius, rect.width * 0.5f, rect.height * 0.5f});
            if (safe_radius <= 0.0f) {
                FillRect(rect, color);
                return;
            }
            // Cross body + quarter discs at each corner (clipped to the corner
            // squares) reproduces the original rounded-rect look.
            renderer_.DrawRect(
                ToRect(FloatRect{rect.x + safe_radius, rect.y, rect.width - safe_radius * 2.0f, rect.height}), color);
            renderer_.DrawRect(
                ToRect(FloatRect{rect.x, rect.y + safe_radius, rect.width, rect.height - safe_radius * 2.0f}), color);
            const std::array<FloatRect, 4> corners{{
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
                renderer_.PushClip(ToRect(corners[i]));
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

        auto DrawTexture(atom::render::Renderer2D::Texture& texture, const FloatRect& rect, const float opacity)
            -> void {
            renderer_.DrawTexture(texture, ToRect(rect), WithAlpha(atom::render::Color::White(), opacity), nullptr);
        }

    private:
        atom::render::Renderer2D& renderer_;
};

class MusicCardScreen final : public atom::Screen {
    public:
        MusicCardScreen(atom::MusicPlayer& music, std::vector<std::string> paths) : music_{music} {
            layout_tree_.SetPointScaleFactor(1.0f);
            root_ = layout_tree_.Root();
            card_ = layout_tree_.CreateNode();
            cover_ = layout_tree_.CreateNode();
            details_ = layout_tree_.CreateNode();
            title_ = layout_tree_.CreateNode();
            author_ = layout_tree_.CreateNode();
            controls_ = layout_tree_.CreateNode();
            previous_button_ = layout_tree_.CreateNode();
            play_button_ = layout_tree_.CreateNode();
            next_button_ = layout_tree_.CreateNode();
            BuildTrackStubs(std::move(paths));
            artwork_textures_.resize(tracks_.size());
            artwork_texture_attempted_.resize(tracks_.size());
            BuildLayoutTree();
            // Load the first track synchronously so the card has content on the
            // very first frame. All remaining tracks are resolved lazily by the
            // background prefetch worker (current ± 1).
            if (!tracks_.empty()) {
                LoadTrackMetadata(0);
            }
            // Kick off the initial neighbour prefetch.
            {
                std::lock_guard lock{tracks_mutex_};
                prefetch_requested_ = true;
            }
            loader_thread_ = std::jthread{[this](std::stop_token st) { LoaderLoop(st); }};
            LOG_INFO(atom::core::LogChannel::SCREEN, "Music card initialized with " + std::to_string(tracks_.size()) +
                                                         " track path(s); lazy metadata + audio loading enabled");
        }

        ~MusicCardScreen() override {
            // loader_thread_ is a std::jthread: its destructor requests stop and
            // joins, so the worker exits before renderer_/tracks_ are torn down.
            renderer_.Shutdown();
        }

        auto ShutdownRenderer() -> void {
            renderer_.Shutdown();
        }

        auto Play() -> void {
            if (tracks_.empty()) {
                return;
            }
            if (!is_playing_) {
                StartCurrentTrack();
            }
            if (animation_state_ == AnimationState::Hidden) {
                RestartEntrance();
            }
        }

        auto Stop() -> void {
            StopCurrentTrack();
        }

        auto Previous() -> void {
            RequestRelativeTrack(-1);
        }

        auto Next() -> void {
            RequestRelativeTrack(1);
        }

        auto ToggleCorner() -> void {
            corner_ = corner_ == CardCorner::BottomLeft ? CardCorner::TopRight : CardCorner::BottomLeft;
            LOG_INFO(atom::core::LogChannel::SCREEN, "Music card corner changed to " + std::string{GetCornerName()});
            if (animation_state_ != AnimationState::Hidden) {
                RestartEntrance();
            }
        }

        auto ToggleCardVisibility() -> void {
            pending_track_ = tracks_.size();
            if (animation_state_ == AnimationState::Hidden || animation_state_ == AnimationState::Exiting) {
                RestartEntrance();
            } else {
                animation_state_ = AnimationState::Exiting;
            }
            LOG_DEBUG(atom::core::LogChannel::SCREEN,
                      "Music card visibility transition requested; state=" + std::string{GetAnimationStateName()});
        }

        // Returns a copy of the current track's display fields. The full Track
        // is never exposed by reference because the background loader may write
        // metadata at any time.
        struct TrackDisplay {
                std::string title;
                std::string artist;
                bool is_loaded = false;
                bool metadata_loaded = false;
        };

        [[nodiscard]] auto GetCurrentDisplay() const -> TrackDisplay {
            std::lock_guard lock{tracks_mutex_};
            if (current_track_ >= tracks_.size()) {
                return {};
            }
            const auto& t = tracks_[current_track_];
            return {t.title, t.artist, t.is_loaded, t.metadata_loaded};
        }

        [[nodiscard]] auto GetCurrentTrack() const -> Track {
            std::lock_guard lock{tracks_mutex_};
            if (current_track_ >= tracks_.size()) {
                return {};
            }
            return tracks_[current_track_];
        }

        [[nodiscard]] auto HasTracks() const -> bool {
            return !tracks_.empty();
        }

        [[nodiscard]] auto IsPlaying() const -> bool {
            return is_playing_;
        }

        [[nodiscard]] auto IsCardVisible() const -> bool {
            return animation_state_ != AnimationState::Hidden && animation_state_ != AnimationState::Exiting;
        }

        auto LoadInterfaceFont() -> bool {
#ifdef _WIN32
            constexpr std::array font_candidates{
                "C:/Windows/Fonts/msyh.ttc",    "C:/Windows/Fonts/msyhbd.ttc",
                "C:/Windows/Fonts/simhei.ttf",        "C:/Windows/Fonts/simsun.ttc",  "C:/Windows/Fonts/msgothic.ttc",
                "C:/Windows/Fonts/meiryo.ttc",        "C:/Windows/Fonts/meiryob.ttc", "C:/Windows/Fonts/yugothib.ttf",
                "C:/Windows/Fonts/yugothic.ttf",      "C:/Windows/Fonts/malgun.ttf",
            };
#else
            constexpr std::array font_candidates{
                "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
            };
#endif // _WIN32
            for (const auto* font_path : font_candidates) {
                if (!std::filesystem::exists(font_path)) {
                    continue;
                }
                std::ifstream stream{font_path, std::ios::binary | std::ios::ate};
                if (!stream)
                    continue;
                const auto size = stream.tellg();
                if (size <= 0)
                    continue;
                interface_font_data_.resize(static_cast<std::size_t>(size));
                stream.seekg(0, std::ios::beg);
                if (!stream.read(reinterpret_cast<char*>(interface_font_data_.data()), size)) {
                    interface_font_data_.clear();
                    continue;
                }

                // The same user-interface font feeds the production Renderer2D
                // text path and the ImGui-only debugger window.
                const auto* debug_font = atom::debugger::ImGuiFontLoader::LoadFromFile(
                    font_path, {.size_pixels = 18.0f,
                                .glyph_preset = atom::debugger::ImGuiGlyphPreset::ChineseFull,
                                .set_as_default = true});
                if (!debug_font)
                    LOG_WARNING(atom::debugger::LogChannel::IMGUI,
                                "CJK font loaded for Renderer2D, but not for the ImGui debugger");
                LOG_INFO(atom::core::LogChannel::SCREEN, "Music card loaded interface font: " + std::string{font_path});
                return true;
            }
            LOG_WARNING(atom::core::LogChannel::SCREEN,
                        "No suitable CJK interface font was loaded; on-card text will be hidden");
            return false;
        }

        [[nodiscard]] auto GetCornerName() const -> std::string_view {
            return corner_ == CardCorner::BottomLeft ? "Bottom left" : "Top right";
        }

        [[nodiscard]] auto GetAnimationStateName() const -> std::string_view {
            switch (animation_state_) {
            case AnimationState::Entering:
                return "Entering";
            case AnimationState::Visible:
                return "Visible";
            case AnimationState::Exiting:
                return "Exiting";
            case AnimationState::Hidden:
                return "Hidden";
            }
            return "Unknown";
        }

        auto Render(atom::render::IRenderDevice& device) -> void override {
            if (!renderer_.IsInitialized() && !renderer_.Initialize(device, ATOM_SHADER_OUTPUT_DIR)) {
                if (!renderer_initialization_failed_) {
                    LOG_ERROR(atom::core::LogChannel::SCREEN, "Music card: Renderer2D initialization failed");
                    renderer_initialization_failed_ = true;
                }
                return;
            }
            if (!interface_font_attempted_ && !interface_font_data_.empty()) {
                interface_font_attempted_ = true;
                interface_font_ = renderer_.LoadFontFromMemory(interface_font_data_);
                if (!interface_font_)
                    LOG_ERROR(atom::core::LogChannel::SCREEN,
                              "Music card: Renderer2D rejected the selected interface font");
            }
            EnsureBackgroundTexture();
            device.Clear(atom::render::Color{8, 10, 18, 255});
            const auto size = device.GetOutputSize();
            window_width_ = size.GetX();
            window_height_ = size.GetY();
            if (window_width_ <= 0.0f || window_height_ <= 0.0f) {
                return;
            }
            UpdateLayout();
            UpdateCardPostProcessRegion();
            if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f)) {
                return;
            }
            auto painter = CardPainter{renderer_};
            DrawBackground(painter);
            renderer_.SetPostProcess({});
            if (!renderer_.EndFrame() && !renderer_frame_failed_) {
                LOG_ERROR(atom::core::LogChannel::SCREEN, "Music card: background frame submission failed");
                renderer_frame_failed_ = true;
            }

            // Render the wallpaper into the reusable offscreen target and
            // resolve only the popup region through a Gaussian blur. This is
            // the backdrop of the glass panel; the debugger and the rest of
            // the wallpaper remain untouched by the blur pass.
            if (animation_state_ != AnimationState::Hidden && current_card_rect_.width > 0.0f &&
                current_card_rect_.height > 0.0f) {
                if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f)) {
                    return;
                }
                DrawBackground(painter);
                atom::render::PostProcess2DParams blur{};
                blur.effect = atom::render::PostProcess2DEffect::GaussianBlur;
                blur.has_region = true;
                blur.region = ToRect(current_card_bounds_);
                blur.corner_radius = 24.0f;
                blur.feather = 8.0f;
                blur.amount = 16.0f;
                renderer_.SetPostProcess(blur);
                if (!renderer_.EndFrame() && !renderer_frame_failed_) {
                    LOG_ERROR(atom::core::LogChannel::SCREEN, "Music card: backdrop blur submission failed");
                    renderer_frame_failed_ = true;
                }
            }

            // Render only the popup into the post-process target. Keeping the
            // background in its own direct pass prevents chromatic aberration
            // and glitch effects from contaminating the whole window. The
            // debugger overlay is submitted later by RenderWindow, after this
            // screen render, so it is never part of this post-process input.
            if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f)) {
                return;
            }
            DrawCard(painter, false);
            atom::render::PostProcess2DParams postprocess{};
            postprocess.time = effect_time_;
            postprocess.has_region = true;
            postprocess.region = ToRect(current_card_rect_);
            postprocess.corner_radius = 24.0f;
            postprocess.feather = 16.0f;
            if (animation_state_ == AnimationState::Entering || animation_state_ == AnimationState::Exiting) {
                postprocess.effect = atom::render::PostProcess2DEffect::Glitch;
                postprocess.progress = std::sin(animation_progress_ * 3.14159265f);
                postprocess.intensity = 1.0f;
                postprocess.direction = glitch_direction_;
            } else {
                postprocess.effect = atom::render::PostProcess2DEffect::ChromaticAberration;
                postprocess.amount = 0.003f;
                postprocess.scanline = 0.035f;
                postprocess.noise = 0.018f;
            }
            renderer_.SetPostProcess(postprocess);
            if (!renderer_.EndFrame() && !renderer_frame_failed_) {
                LOG_ERROR(atom::core::LogChannel::SCREEN, "Music card: Renderer2D frame submission failed");
                renderer_frame_failed_ = true;
            }
            // Text is intentionally submitted after the card post-process.
            // It uses the glyph atlas' nearest sampler and is never softened
            // or channel-split by the visual effects.
            if (animation_state_ != AnimationState::Hidden) {
                if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f)) {
                    return;
                }
                DrawCardText();
                renderer_.SetPostProcess({});
                if (!renderer_.EndFrame() && !renderer_frame_failed_) {
                    LOG_ERROR(atom::core::LogChannel::SCREEN, "Music card: text frame submission failed");
                    renderer_frame_failed_ = true;
                }
            }
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (event.type == atom::window::EventType::KeyPressed) {
                const auto& key = std::get<atom::window::KeyEvent>(event.data);
                switch (key.scancode) {
                case SDL_SCANCODE_ESCAPE:
                    atom::RenderWindow::GetInstance().Shutdown();
                    return true;
                case SDL_SCANCODE_LEFT:
                    Previous();
                    return true;
                case SDL_SCANCODE_RIGHT:
                    Next();
                    return true;
                case SDL_SCANCODE_SPACE:
                    TogglePlayback();
                    return true;
                case SDL_SCANCODE_C:
                    ToggleCorner();
                    return true;
                case SDL_SCANCODE_H:
                    ToggleCardVisibility();
                    return true;
                default:
                    break;
                }
            }

            if (event.type == atom::window::EventType::MouseButtonPressed) {
                const auto& mouse = std::get<atom::window::MouseEvent>(event.data);
                if (mouse.button != SDL_BUTTON_LEFT) {
                    return false;
                }
                if (Contains(previous_hitbox_, mouse.x, mouse.y)) {
                    Previous();
                    return true;
                }
                if (Contains(play_hitbox_, mouse.x, mouse.y)) {
                    TogglePlayback();
                    return true;
                }
                if (Contains(next_hitbox_, mouse.x, mouse.y)) {
                    Next();
                    return true;
                }
            }
            return false;
        }

        auto Update(const float delta_time) -> void override {
            effect_time_ += std::max(0.0f, delta_time);
            TryStartAfterLoad();
            switch (animation_state_) {
            case AnimationState::Entering:
                animation_progress_ = std::min(1.0f, animation_progress_ + delta_time / 0.32f);
                if (animation_progress_ >= 1.0f) {
                    animation_state_ = AnimationState::Visible;
                    visible_time_ = 0.0f;
                }
                break;
            case AnimationState::Visible:
                break;
            case AnimationState::Exiting:
                animation_progress_ = std::max(0.0f, animation_progress_ - delta_time / 0.24f);
                if (animation_progress_ <= 0.0f) {
                    CompleteExit();
                }
                break;
            case AnimationState::Hidden:
                break;
            }
        }

    private:
        auto EnsureBackgroundTexture() -> void {
            if (background_texture_attempted_ || !renderer_.IsInitialized())
                return;
            background_texture_attempted_ = true;
            const char* override_path = std::getenv("ATOM_MUSIC_CARD_WALLPAPER");
            const std::string wallpaper_path = override_path && *override_path ? override_path : WallpaperPath;
            const auto decoded = atom::image::DecodeImageFile(wallpaper_path, false);
            if (!decoded.IsValid()) {
                LOG_WARNING(atom::core::LogChannel::SCREEN,
                            "Music card wallpaper unavailable; using procedural background: " + wallpaper_path);
                return;
            }
            background_texture_ = renderer_.CreateTexture(decoded.width, decoded.height, decoded.rgba.data());
            if (!background_texture_) {
                LOG_ERROR(atom::core::LogChannel::SCREEN,
                          "Music card failed to upload wallpaper texture: " + wallpaper_path);
                return;
            }
            LOG_INFO(atom::core::LogChannel::SCREEN,
                     "Music card wallpaper loaded (" + std::to_string(decoded.width) + "x" +
                         std::to_string(decoded.height) + "): " + wallpaper_path);
        }

        auto BuildLayoutTree() -> void {
            auto card_style = atom::layout::LayoutStyle{};
            card_style.width = atom::layout::Length::Points(430.0f);
            card_style.height = atom::layout::Length::Points(132.0f);
            card_style.padding = atom::layout::Edges::All(atom::layout::Length::Points(14.0f));
            card_style.column_gap = 16.0f;
            card_style.position_type = atom::layout::PositionType::Absolute;
            layout_tree_.SetStyle(card_, card_style);

            auto cover_style = atom::layout::LayoutStyle{};
            cover_style.width = atom::layout::Length::Points(104.0f);
            cover_style.height = atom::layout::Length::Points(104.0f);
            layout_tree_.SetStyle(cover_, cover_style);

            auto details_style = atom::layout::LayoutStyle{};
            details_style.flex_direction = atom::layout::FlexDirection::Column;
            details_style.flex_grow = 1.0f;
            details_style.padding.top = atom::layout::Length::Points(4.0f);
            details_style.padding.bottom = atom::layout::Length::Points(2.0f);
            details_style.row_gap = 5.0f;
            layout_tree_.SetStyle(details_, details_style);

            auto title_style = atom::layout::LayoutStyle{};
            title_style.height = atom::layout::Length::Points(22.0f);
            layout_tree_.SetStyle(title_, title_style);

            auto author_style = atom::layout::LayoutStyle{};
            author_style.height = atom::layout::Length::Points(15.0f);
            layout_tree_.SetStyle(author_, author_style);

            auto controls_style = atom::layout::LayoutStyle{};
            controls_style.height = atom::layout::Length::Points(42.0f);
            controls_style.margin.top = atom::layout::Length::Auto();
            controls_style.column_gap = 10.0f;
            controls_style.align_items = atom::layout::Align::Center;
            layout_tree_.SetStyle(controls_, controls_style);

            auto button_style = atom::layout::LayoutStyle{};
            button_style.width = atom::layout::Length::Points(34.0f);
            button_style.height = atom::layout::Length::Points(34.0f);
            layout_tree_.SetStyle(previous_button_, button_style);
            layout_tree_.SetStyle(next_button_, button_style);
            button_style.width = atom::layout::Length::Points(42.0f);
            button_style.height = atom::layout::Length::Points(42.0f);
            layout_tree_.SetStyle(play_button_, button_style);

            layout_tree_.Append(root_, card_);
            layout_tree_.Append(card_, cover_);
            layout_tree_.Append(card_, details_);
            layout_tree_.Append(details_, title_);
            layout_tree_.Append(details_, author_);
            layout_tree_.Append(details_, controls_);
            layout_tree_.Append(controls_, previous_button_);
            layout_tree_.Append(controls_, play_button_);
            layout_tree_.Append(controls_, next_button_);
        }

        auto UpdateLayout() -> void {
            auto root_style = atom::layout::LayoutStyle{};
            root_style.width = atom::layout::Length::Points(window_width_);
            root_style.height = atom::layout::Length::Points(window_height_);
            layout_tree_.SetStyle(root_, root_style);

            auto card_style = atom::layout::LayoutStyle{};
            card_style.width = atom::layout::Length::Points(430.0f);
            card_style.height = atom::layout::Length::Points(132.0f);
            card_style.padding = atom::layout::Edges::All(atom::layout::Length::Points(14.0f));
            card_style.column_gap = 16.0f;
            card_style.position_type = atom::layout::PositionType::Absolute;
            if (corner_ == CardCorner::BottomLeft) {
                card_style.position.left = atom::layout::Length::Points(34.0f);
                card_style.position.bottom = atom::layout::Length::Points(34.0f);
            } else {
                card_style.position.right = atom::layout::Length::Points(34.0f);
                card_style.position.top = atom::layout::Length::Points(34.0f);
            }
            layout_tree_.SetStyle(card_, card_style);
            layout_tree_.Calculate();
            if (window_width_ != reported_layout_width_ || window_height_ != reported_layout_height_) {
                const auto resolved = layout_tree_.GetLayout(card_).value_or(atom::layout::Rect{});
                LOG_DEBUG(atom::core::LogChannel::SCREEN,
                          "Music card layout resolved: viewport=" + std::to_string(window_width_) + "x" +
                              std::to_string(window_height_) + ", card=" + std::to_string(resolved.left) + "," +
                              std::to_string(resolved.top) + " " + std::to_string(resolved.width) + "x" +
                              std::to_string(resolved.height));
                reported_layout_width_ = window_width_;
                reported_layout_height_ = window_height_;
            }
        }

        [[nodiscard]] auto GlobalRect(const atom::layout::LayoutTree::NodeId node, const FloatRect& parent) const
            -> FloatRect {
            const auto layout = layout_tree_.GetLayout(node).value_or(atom::layout::Rect{});
            return {parent.x + layout.left, parent.y + layout.top, layout.width, layout.height};
        }

        auto DrawBackground(CardPainter& painter) -> void {
            if (background_texture_ != nullptr) {
                const auto texture_width = static_cast<float>(background_texture_->GetWidth());
                const auto texture_height = static_cast<float>(background_texture_->GetHeight());
                const auto window_aspect = window_width_ / std::max(window_height_, 1.0f);
                const auto texture_aspect = texture_width / std::max(texture_height, 1.0f);
                FloatRect source{0.0f, 0.0f, texture_width, texture_height};
                if (texture_aspect > window_aspect) {
                    source.width = texture_height * window_aspect;
                    source.x = (texture_width - source.width) * 0.5f;
                } else {
                    source.height = texture_width / window_aspect;
                    source.y = (texture_height - source.height) * 0.5f;
                }
                const auto source_rect = ToRect(source);
                renderer_.DrawTexture(*background_texture_, {0.0f, 0.0f, window_width_, window_height_},
                                      atom::render::Color{255, 255, 255, 255}, &source_rect);
                // Dark glass tint keeps the foreground card and debugger
                // readable without routing the wallpaper through postprocess.
                painter.FillRect({0.0f, 0.0f, window_width_, window_height_},
                                 atom::render::Color{8, 10, 18, 28});
                return;
            }
            constexpr auto band_count = 12;
            const auto band_height = window_height_ / static_cast<float>(band_count);
            for (auto index = 0; index < band_count; ++index) {
                const auto blend = static_cast<float>(index) / static_cast<float>(band_count - 1);
                const auto color = atom::render::Color{
                    static_cast<uint8_t>(8.0f + blend * 8.0f),
                    static_cast<uint8_t>(10.0f + blend * 9.0f),
                    static_cast<uint8_t>(18.0f + blend * 17.0f),
                    255,
                };
                painter.FillRect({0.0f, band_height * static_cast<float>(index), window_width_, band_height + 1.0f},
                                 color);
            }

            painter.FillCircle(window_width_ * 0.72f, window_height_ * 0.30f, 150.0f,
                               atom::render::Color{43, 36, 84, 34});
            painter.FillCircle(window_width_ * 0.23f, window_height_ * 0.68f, 110.0f,
                               atom::render::Color{21, 87, 96, 25});
        }

        [[nodiscard]] auto ComputeAnimatedCardRect() const -> FloatRect {
            const auto layout = layout_tree_.GetLayout(card_).value_or(atom::layout::Rect{});
            const auto final_card = FloatRect{layout.left, layout.top, layout.width, layout.height};
            const auto direction = corner_ == CardCorner::BottomLeft ? -1.0f : 1.0f;
            const auto panel_progress = EaseOutCubic(Stagger(animation_progress_, 0.0f, 0.62f));
            return OffsetRect(final_card, direction * (1.0f - panel_progress) * 86.0f, 0.0f);
        }

        auto UpdateCardPostProcessRegion() -> void {
            if (animation_state_ == AnimationState::Hidden || tracks_.empty()) {
                current_card_bounds_ = {};
                current_card_rect_ = {};
                return;
            }
            const auto card = ComputeAnimatedCardRect();
            current_card_bounds_ = card;
            current_card_rect_ = {card.x - 16.0f, card.y - 16.0f, card.width + 32.0f, card.height + 32.0f};
        }

        auto DrawCard(CardPainter& painter, const bool draw_text = true) -> void {
            previous_hitbox_ = {};
            play_hitbox_ = {};
            next_hitbox_ = {};
            if (animation_state_ == AnimationState::Hidden || tracks_.empty()) {
                return;
            }

            const auto card_layout = layout_tree_.GetLayout(card_).value_or(atom::layout::Rect{});
            const auto final_card = FloatRect{card_layout.left, card_layout.top, card_layout.width, card_layout.height};
            const auto direction = corner_ == CardCorner::BottomLeft ? -1.0f : 1.0f;
            const auto panel_progress = EaseOutCubic(Stagger(animation_progress_, 0.0f, 0.62f));
            const auto cover_progress = EaseOutBack(Stagger(animation_progress_, 0.10f, 0.72f));
            const auto title_progress = EaseOutCubic(Stagger(animation_progress_, 0.28f, 0.78f));
            const auto author_progress = EaseOutCubic(Stagger(animation_progress_, 0.38f, 0.86f));
            const auto controls_progress = EaseOutBack(Stagger(animation_progress_, 0.48f, 1.0f));
            const auto exit_softening = animation_state_ == AnimationState::Exiting
                                            ? 1.0f - 0.12f * EaseInCubic(1.0f - animation_progress_)
                                            : 1.0f;

            const auto card = OffsetRect(final_card, direction * (1.0f - panel_progress) * 86.0f, 0.0f);
            const auto panel_opacity = panel_progress * exit_softening;
            for (auto layer = 3; layer >= 1; --layer) {
                const auto offset = static_cast<float>(layer) * 4.0f;
                painter.FillRoundedRect(OffsetRect(card, 0.0f, offset), 24.0f,
                                        atom::render::Color{0, 0, 0, static_cast<uint8_t>(18 * layer * panel_opacity)});
            }
            // Semi-transparent themed glass layer; the backdrop remains
            // visible and is no longer hidden by an opaque panel color.
            painter.FillRoundedRect(card, 24.0f, WithAlpha(atom::render::Color{38, 46, 78, 78}, panel_opacity));
            painter.FillRoundedRect({card.x + 1.0f, card.y + 1.0f, card.width - 2.0f, 1.0f}, 1.0f,
                                    WithAlpha(atom::render::Color{255, 255, 255, 38}, panel_opacity));

            const auto cover_final = GlobalRect(cover_, card);
            const auto cover_offset = direction * (1.0f - cover_progress) * 54.0f;
            const auto cover_rect =
                ScaleRectFromCenter(OffsetRect(cover_final, cover_offset, 0.0f), 0.76f + cover_progress * 0.24f);
            DrawAlbumCover(painter, cover_rect, cover_progress * panel_opacity);

            const auto details_final = GlobalRect(details_, card);
            const auto title_final = GlobalRect(title_, details_final);
            const auto author_final = GlobalRect(author_, details_final);
            const auto controls_final = GlobalRect(controls_, details_final);
            const auto title_x = title_final.x + direction * (1.0f - title_progress) * 28.0f;
            const auto author_x = author_final.x + direction * (1.0f - author_progress) * 34.0f;
            const auto display = GetCurrentDisplay();
            const auto& palette = tracks_[current_track_].palette;
            const auto title_text = display.metadata_loaded ? display.title : std::string{"Loading..."};
            const auto artist_text = display.metadata_loaded ? display.artist : std::string{"resolving metadata"};

            if (draw_text) {
                DrawInterfaceText({title_x, title_final.y, title_final.width, title_final.height}, title_text, 21.0f,
                                  MixColor(palette.accent, atom::render::Color::White(), 0.58f));
                DrawInterfaceText({author_x, author_final.y, author_final.width, author_final.height}, artist_text,
                                  14.0f, MixColor(palette.secondary, atom::render::Color::White(), 0.38f));
            }

            const auto previous_final = GlobalRect(previous_button_, controls_final);
            const auto play_final = GlobalRect(play_button_, controls_final);
            const auto next_final = GlobalRect(next_button_, controls_final);
            const auto controls_y = (1.0f - controls_progress) * 18.0f;
            previous_hitbox_ = OffsetRect(previous_final, 0.0f, controls_y);
            play_hitbox_ = OffsetRect(play_final, 0.0f, controls_y);
            next_hitbox_ = OffsetRect(next_final, 0.0f, controls_y);
            DrawControls(painter, controls_progress * panel_opacity);
        }

        auto DrawCardText() -> void {
            if (animation_state_ == AnimationState::Hidden || tracks_.empty() || interface_font_ == nullptr)
                return;
            const auto card_layout = layout_tree_.GetLayout(card_).value_or(atom::layout::Rect{});
            const auto final_card = FloatRect{card_layout.left, card_layout.top, card_layout.width, card_layout.height};
            const auto direction = corner_ == CardCorner::BottomLeft ? -1.0f : 1.0f;
            const auto title_progress = EaseOutCubic(Stagger(animation_progress_, 0.28f, 0.78f));
            const auto author_progress = EaseOutCubic(Stagger(animation_progress_, 0.38f, 0.86f));
            const auto panel_progress = EaseOutCubic(Stagger(animation_progress_, 0.0f, 0.62f));
            const auto card = OffsetRect(final_card, direction * (1.0f - panel_progress) * 86.0f, 0.0f);
            const auto details_final = GlobalRect(details_, card);
            const auto title_final = GlobalRect(title_, details_final);
            const auto author_final = GlobalRect(author_, details_final);
            const auto title_x = title_final.x + direction * (1.0f - title_progress) * 28.0f;
            const auto author_x = author_final.x + direction * (1.0f - author_progress) * 34.0f;
            const auto display = GetCurrentDisplay();
            const auto& palette = tracks_[current_track_].palette;
            const auto title_text = display.metadata_loaded ? display.title : std::string{"Loading..."};
            const auto artist_text = display.metadata_loaded ? display.artist : std::string{"resolving metadata"};
            DrawInterfaceText({title_x, title_final.y, title_final.width, title_final.height}, title_text, 21.0f,
                              MixColor(palette.accent, atom::render::Color::White(), 0.58f));
            DrawInterfaceText({author_x, author_final.y, author_final.width, author_final.height}, artist_text, 14.0f,
                              MixColor(palette.secondary, atom::render::Color::White(), 0.38f));
        }

        // On-card title/author text is production scene content and therefore
        // goes through Renderer2D. ImGui remains an optional debugger overlay.
        auto DrawInterfaceText(const FloatRect& rect, const std::string& text, const float font_size,
                               atom::render::Color color) -> void {
            if (interface_font_ == nullptr || text.empty()) {
                return;
            }
            // Text is deliberately excluded from opacity animation and blur:
            // partially transparent glyphs become soft against bright
            // wallpaper, especially after post-process compositing.
            color.a = 255;
            renderer_.PushClip(ToRect(rect));
            auto shadow = atom::render::Color::Black();
            shadow.a = 210;
            renderer_.DrawText(*interface_font_, text, rect.x + 1.2f, rect.y + 1.2f, shadow, font_size, rect.width);
            renderer_.DrawText(*interface_font_, text, rect.x, rect.y, color, font_size, rect.width);
            renderer_.PopClip();
        }

        auto DrawAlbumCover(CardPainter& painter, const FloatRect& rect, const float opacity) -> void {
            const auto& palette = tracks_[current_track_].palette;
            // Only attempt artwork texture creation once the background loader
            // has resolved metadata for this track. Before that we keep the
            // generated-cover fallback and leave artwork_texture_attempted_
            // false so the next frame retries after metadata arrives.
            if (!artwork_texture_attempted_[current_track_]) {
                const auto display = GetCurrentDisplay();
                if (display.metadata_loaded) {
                    artwork_texture_attempted_[current_track_] = true;
                    std::vector<uint8_t> artwork_data;
                    {
                        std::lock_guard lock{tracks_mutex_};
                        artwork_data = tracks_[current_track_].artwork_data;
                    }
                    if (const auto extracted = ExtractPaletteFromRgba(atom::image::DecodeImageMemory(
                            std::as_bytes(std::span{artwork_data}), false).rgba);
                        extracted.has_value()) {
                        std::lock_guard lock{tracks_mutex_};
                        tracks_[current_track_].palette = *extracted;
                        LOG_INFO(atom::audio::LogChannel::METADATA,
                                 "Extracted MusicCard palette from embedded artwork for track " +
                                     std::to_string(current_track_));
                    }
                    artwork_textures_[current_track_] = CreateArtworkTexture(renderer_, artwork_data);
                }
            }
            if (artwork_textures_[current_track_] != nullptr) {
                const auto image_rect = FloatRect{rect.x + 2.0f, rect.y + 2.0f, rect.width - 4.0f, rect.height - 4.0f};
                painter.DrawTexture(*artwork_textures_[current_track_], image_rect, opacity);
                painter.FillRoundedRect({rect.x, rect.y, rect.width, 2.0f}, 1.0f,
                                        WithAlpha(atom::render::Color{255, 255, 255, 70}, opacity));
                return;
            }

            constexpr auto stripe_count = 7;
            const auto stripe_height = rect.height / static_cast<float>(stripe_count);
            for (auto stripe = 0; stripe < stripe_count; ++stripe) {
                const auto stripe_opacity = stripe % 2 == 0 ? 0.22f : 0.10f;
                painter.FillRect(
                    {rect.x, rect.y + stripe_height * static_cast<float>(stripe), rect.width, stripe_height + 1.0f},
                    WithAlpha(palette.secondary, opacity * stripe_opacity));
            }

            painter.FillCircle(rect.x + rect.width * 0.68f, rect.y + rect.height * 0.34f, rect.width * 0.31f,
                               WithAlpha(palette.accent, opacity * 0.78f));
            painter.FillCircle(rect.x + rect.width * 0.30f, rect.y + rect.height * 0.73f, rect.width * 0.23f,
                               WithAlpha(atom::render::Color{255, 255, 255, 110}, opacity));
            painter.FillCircle(rect.x + rect.width * 0.68f, rect.y + rect.height * 0.34f, rect.width * 0.065f,
                               WithAlpha(atom::render::Color{24, 27, 39, 235}, opacity));
        }

        auto DrawControls(CardPainter& painter, const float opacity) const -> void {
            const auto secondary = WithAlpha(atom::render::Color{189, 195, 214, 255}, opacity);
            const auto primary = WithAlpha(tracks_[current_track_].palette.accent, opacity);
            painter.FillCircle(play_hitbox_.x + play_hitbox_.width * 0.5f, play_hitbox_.y + play_hitbox_.height * 0.5f,
                               play_hitbox_.width * 0.5f, primary);

            const auto button_alpha = WithAlpha(atom::render::Color{255, 255, 255, 255}, opacity);
            // Previous / play / next glyphs drawn as simple shapes (no text in
            // CardPainter; textual glyphs come from the ImGui overlay).
            DrawTriangleGlyph(painter, previous_hitbox_, false, secondary);
            DrawTriangleGlyph(painter, next_hitbox_, true, secondary);
            DrawPlayPauseGlyph(painter, button_alpha);
        }

        auto DrawTriangleGlyph(CardPainter& painter, const FloatRect& box, const bool points_right,
                               const atom::render::Color color) const -> void {
            const float cx = box.x + box.width * 0.5f;
            const float cy = box.y + box.height * 0.5f;
            const float r = std::min(box.width, box.height) * 0.24f;
            const float tip_x = points_right ? cx + r : cx - r;
            const float base_x = points_right ? cx - r : cx + r;
            const float y0 = cy - r;
            const float y1 = cy + r;
            // Triangle outline approximated with the available primitives:
            painter.DrawLine(tip_x, cy, base_x, y0, color, 2.0f);
            painter.DrawLine(base_x, y0, base_x, y1, color, 2.0f);
            painter.DrawLine(base_x, y1, tip_x, cy, color, 2.0f);
        }

        auto DrawPlayPauseGlyph(CardPainter& painter, const atom::render::Color color) const -> void {
            if (is_playing_) {
                const float x = play_hitbox_.x + play_hitbox_.width * 0.28f;
                const float y = play_hitbox_.y + play_hitbox_.height * 0.28f;
                const float w = play_hitbox_.width * 0.16f;
                const float h = play_hitbox_.height * 0.44f;
                painter.FillRect({x, y, w, h}, color);
                painter.FillRect({x + w * 2.6f, y, w, h}, color);
            } else {
                const float cx = play_hitbox_.x + play_hitbox_.width * 0.5f;
                const float cy = play_hitbox_.y + play_hitbox_.height * 0.5f;
                const float r = play_hitbox_.width * 0.22f;
                const float tip_x = cx + r;
                const float base_x = cx - r;
                const float y0 = cy - r;
                const float y1 = cy + r;
                // Approximate the play triangle with two thick lines + a bar.
                painter.DrawLine(tip_x, cy, base_x, y0, color, 2.0f);
                painter.DrawLine(base_x, y0, base_x, y1, color, 2.0f);
                painter.DrawLine(base_x, y1, tip_x, cy, color, 2.0f);
            }
        }

        [[nodiscard]] auto HasLoadedTracks() const -> bool {
            std::lock_guard lock{tracks_mutex_};
            return std::ranges::any_of(tracks_, [](const Track& track) { return track.is_loaded; });
        }

        auto RequestRelativeTrack(const int offset) -> void {
            if (tracks_.empty()) {
                return;
            }
            const auto count = static_cast<int>(tracks_.size());
            const auto requested_track =
                static_cast<std::size_t>((static_cast<int>(current_track_) + offset + count) % count);
            {
                std::lock_guard lock{tracks_mutex_};
                pending_track_ = requested_track;
            }
            glitch_direction_ = offset >= 0 ? 1.0f : -1.0f;
            LOG_DEBUG(atom::core::LogChannel::SCREEN,
                      "Music card track transition requested: " + std::to_string(current_track_) + " -> " +
                          std::to_string(pending_track_));
            // Wake the background loader immediately so it starts resolving
            // metadata + audio for the pending track before the animation ends.
            {
                std::lock_guard lock{tracks_mutex_};
                prefetch_requested_ = true;
            }
            loader_cv_.notify_one();
            if (animation_state_ == AnimationState::Hidden) {
                CompleteExit();
            } else {
                animation_state_ = AnimationState::Exiting;
            }
        }

        auto CompleteExit() -> void {
            if (pending_track_ < tracks_.size()) {
                StopCurrentTrack();
                {
                    std::lock_guard lock{tracks_mutex_};
                    current_track_ = pending_track_;
                    pending_track_ = tracks_.size();
                }
                // Never resolve metadata or open a decoder on the render
                // thread. The old synchronous EnsureTrackLoaded() call caused
                // the first previous/next transition to stall for a fraction
                // of a second. Let the worker finish and poll readiness from
                // Update() instead.
                start_after_load_ = true;
                RestartEntrance();
                return;
            }
            animation_state_ = AnimationState::Hidden;
        }

        auto RestartEntrance() -> void {
            animation_progress_ = 0.0f;
            visible_time_ = 0.0f;
            animation_state_ = AnimationState::Entering;
        }

        auto TryStartAfterLoad() -> void {
            if (!start_after_load_ || current_track_ >= tracks_.size())
                return;
            bool loaded = false;
            {
                std::lock_guard lock{tracks_mutex_};
                loaded = tracks_[current_track_].is_loaded;
            }
            if (loaded) {
                start_after_load_ = false;
                StartCurrentTrack();
                LOG_DEBUG(atom::core::LogChannel::SCREEN,
                          "Music card deferred track start completed without blocking the render thread");
            }
        }

        auto TogglePlayback() -> void {
            if (tracks_.empty()) {
                return;
            }
            if (is_playing_) {
                Stop();
            } else {
                Play();
            }
        }

        auto StartCurrentTrack() -> void {
            std::string id;
            std::string title;
            bool is_loaded = false;
            {
                std::lock_guard lock{tracks_mutex_};
                if (current_track_ >= tracks_.size()) {
                    return;
                }
                is_loaded = tracks_[current_track_].is_loaded;
                id = tracks_[current_track_].id;
                title = tracks_[current_track_].title;
            }
            if (is_loaded) {
                is_playing_ = true;
                LOG_INFO(atom::audio::LogChannel::MUSIC, "Music card playing track: " + title);
                music_.Play(id);
            } else {
                is_playing_ = false;
                LOG_WARNING(atom::audio::LogChannel::MUSIC,
                            "Music card cannot play track (audio not loaded yet): " + title);
            }
        }

        auto StopCurrentTrack() -> void {
            start_after_load_ = false;
            std::string id;
            std::string title;
            bool is_loaded = false;
            {
                std::lock_guard lock{tracks_mutex_};
                if (tracks_.empty() || current_track_ >= tracks_.size()) {
                    is_playing_ = false;
                    return;
                }
                is_loaded = tracks_[current_track_].is_loaded;
                id = tracks_[current_track_].id;
                title = tracks_[current_track_].title;
            }
            if (is_loaded) {
                LOG_INFO(atom::audio::LogChannel::MUSIC, "Music card stopping track: " + title);
                music_.Stop(id);
            }
            is_playing_ = false;
        }

        // Creates Track stubs (id + path + palette) from discovered file paths.
        // No metadata or audio is loaded here; that happens lazily.
        auto BuildTrackStubs(std::vector<std::string> paths) -> void {
            constexpr std::array palettes{
                Palette{{52, 45, 111, 255}, {92, 74, 168, 255}, {132, 219, 214, 255}},
                Palette{{24, 82, 96, 255}, {39, 134, 139, 255}, {245, 188, 111, 255}},
            };
            tracks_.reserve(paths.size());
            for (auto i = std::size_t{0}; i < paths.size(); ++i) {
                tracks_.push_back(
                    {"music_card_track_" + std::to_string(i), std::move(paths[i]), palettes[i % palettes.size()]});
            }
        }

        // Resolves metadata + audio for a single track. Safe to call from any
        // thread; the slow operations (TagLib read, decoder open) run without
        // holding tracks_mutex_, and only the final write is locked.
        auto LoadTrackMetadata(std::size_t index) -> void {
            if (index >= tracks_.size()) {
                return;
            }
            {
                std::lock_guard lock{tracks_mutex_};
                if (tracks_[index].metadata_loaded) {
                    return;
                }
            }
            // Snapshot immutable fields outside the lock.
            const std::string path = tracks_[index].path;
            const std::string id = tracks_[index].id;

            LOG_DEBUG(atom::audio::LogChannel::METADATA, "Lazy-loading track " + std::to_string(index) + ": " + path);
            auto metadata = atom::audio::AudioMetadataReader::Read(path);
            auto title =
                metadata && !metadata->title.empty() ? metadata->title : std::filesystem::path{path}.stem().string();
            auto artist = metadata && !metadata->artist.empty() ? metadata->artist : std::string{"UNKNOWN ARTIST"};
            const auto is_loaded = music_.Load(id, path);
            auto artwork_mime_type = metadata ? std::move(metadata->artworkMimeType) : std::string{};
            auto artwork_data = metadata ? std::move(metadata->artworkData) : std::vector<uint8_t>{};

            {
                std::lock_guard lock{tracks_mutex_};
                auto& t = tracks_[index];
                t.title = std::move(title);
                t.artist = std::move(artist);
                t.is_loaded = is_loaded;
                t.artwork_mime_type = std::move(artwork_mime_type);
                t.artwork_data = std::move(artwork_data);
                t.metadata_loaded = true;
            }
            LOG_INFO(atom::audio::LogChannel::METADATA, "Resolved track " + std::to_string(index) + " (audio=" +
                                                            std::string{is_loaded ? "loaded" : "unavailable"} + ")");
        }

        // Background worker: waits for a prefetch notification, then resolves
        // the target track and its two neighbours (previous / next). This keeps
        // the number of simultaneously-open audio decoders small (≤ 3) while
        // making adjacent-track switches instant.
        auto LoaderLoop(std::stop_token stop) -> void {
            LOG_DEBUG(atom::core::LogChannel::SCREEN, "Music card background loader started");
            while (!stop.stop_requested()) {
                std::size_t target;
                {
                    std::unique_lock lock{tracks_mutex_};
                    loader_cv_.wait(lock, stop, [this] { return prefetch_requested_; });
                    if (stop.stop_requested()) {
                        break;
                    }
                    prefetch_requested_ = false;
                    // Prefetch the pending track if a transition is in progress,
                    // otherwise the current track.
                    target = pending_track_ < tracks_.size() ? pending_track_ : current_track_;
                }
                if (tracks_.empty()) {
                    continue;
                }
                const auto count = tracks_.size();
                const std::size_t indices[] = {
                    target,
                    (target + count - 1) % count, // previous
                    (target + 1) % count,         // next
                };
                for (const auto idx : indices) {
                    if (stop.stop_requested()) {
                        break;
                    }
                    LoadTrackMetadata(idx);
                }
            }
            LOG_DEBUG(atom::core::LogChannel::SCREEN, "Music card background loader stopped");
        }

        atom::MusicPlayer& music_;
        std::vector<Track> tracks_;
        std::size_t current_track_ = 0;
        std::size_t pending_track_ = tracks_.size();
        bool is_playing_ = false;
        bool start_after_load_ = false;
        // Lazy-loading state. tracks_mutex_ protects every mutable field of
        // Track (title, artist, is_loaded, artwork_*, metadata_loaded); id,
        // path and palette are immutable after construction.
        mutable std::mutex tracks_mutex_;
        std::condition_variable_any loader_cv_;
        std::jthread loader_thread_;
        bool prefetch_requested_ = false;
        CardCorner corner_ = CardCorner::BottomLeft;
        AnimationState animation_state_ = AnimationState::Entering;
        float animation_progress_ = 0.0f;
        float visible_time_ = 0.0f;
        float effect_time_ = 0.0f;
        float glitch_direction_ = 1.0f;
        float window_width_ = 0.0f;
        float window_height_ = 0.0f;
        FloatRect current_card_rect_{};
        FloatRect current_card_bounds_{};
        float reported_layout_width_ = -1.0f;
        float reported_layout_height_ = -1.0f;

        atom::render::Renderer2D renderer_;
        atom::render::Font* interface_font_ = nullptr;
        std::vector<std::byte> interface_font_data_{};
        bool interface_font_attempted_ = false;
        bool renderer_initialization_failed_ = false;
        bool renderer_frame_failed_ = false;
        atom::render::Renderer2D::Texture* background_texture_ = nullptr;
        bool background_texture_attempted_ = false;
        std::vector<atom::render::Renderer2D::Texture*> artwork_textures_;
        std::vector<bool> artwork_texture_attempted_;

        atom::layout::LayoutTree layout_tree_;
        atom::layout::LayoutTree::NodeId root_ = atom::layout::LayoutTree::kInvalidNode;
        atom::layout::LayoutTree::NodeId card_ = atom::layout::LayoutTree::kInvalidNode;
        atom::layout::LayoutTree::NodeId cover_ = atom::layout::LayoutTree::kInvalidNode;
        atom::layout::LayoutTree::NodeId details_ = atom::layout::LayoutTree::kInvalidNode;
        atom::layout::LayoutTree::NodeId title_ = atom::layout::LayoutTree::kInvalidNode;
        atom::layout::LayoutTree::NodeId author_ = atom::layout::LayoutTree::kInvalidNode;
        atom::layout::LayoutTree::NodeId controls_ = atom::layout::LayoutTree::kInvalidNode;
        atom::layout::LayoutTree::NodeId previous_button_ = atom::layout::LayoutTree::kInvalidNode;
        atom::layout::LayoutTree::NodeId play_button_ = atom::layout::LayoutTree::kInvalidNode;
        atom::layout::LayoutTree::NodeId next_button_ = atom::layout::LayoutTree::kInvalidNode;

        FloatRect previous_hitbox_;
        FloatRect play_hitbox_;
        FloatRect next_hitbox_;
};

class MusicCardDebugger final : public atom::Debugger {
    public:
        explicit MusicCardDebugger(MusicCardScreen& screen) : screen_{screen} {}

    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("Music Card Debugger");
            ImGui::Text("FPS: %.1f", static_cast<double>(GetFPS()));
            ImGui::Text("VSync: %s", atom::RenderWindow::GetInstance().IsVSyncEnabled() ? "on" : "off");
            if (!reported_frame_pacing_ && GetFPS() > 0.0f) {
                LOG_INFO(atom::core::LogChannel::WINDOW,
                         "Music card frame pacing: " + std::to_string(GetFPS()) + " FPS, VSync " +
                             (atom::RenderWindow::GetInstance().IsVSyncEnabled() ? "on" : "off"));
                reported_frame_pacing_ = true;
            }
            ImGui::Separator();

            if (ImGui::Button("Previous")) {
                screen_.Previous();
            }
            ImGui::SameLine();
            if (ImGui::Button("Play")) {
                screen_.Play();
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop")) {
                screen_.Stop();
            }
            ImGui::SameLine();
            if (ImGui::Button("Next")) {
                screen_.Next();
            }

            if (ImGui::Button("Switch corner")) {
                screen_.ToggleCorner();
            }
            ImGui::SameLine();
            if (ImGui::Button(screen_.IsCardVisible() ? "Hide card" : "Show card")) {
                screen_.ToggleCardVisibility();
            }

            ImGui::Separator();
            ImGui::Text("State: %s", screen_.GetAnimationStateName().data());
            ImGui::Text("Position: %s", screen_.GetCornerName().data());
            ImGui::Text("Playback: %s", screen_.IsPlaying() ? "playing" : "stopped");
            if (screen_.HasTracks()) {
                const auto track = screen_.GetCurrentTrack();
                ImGui::Text("Title: %s", track.title.c_str());
                ImGui::Text("Artist: %s", track.artist.c_str());
                ImGui::Text("Audio: %s", track.is_loaded ? "loaded" : "visual demo only");
                ImGui::Text("Metadata: %s", track.metadata_loaded ? "resolved" : "pending");
                ImGui::Text("Artwork: %s%s%s (%zu bytes)", track.artwork_data.empty() ? "fallback" : "embedded",
                            track.artwork_mime_type.empty() ? "" : ", ", track.artwork_mime_type.c_str(),
                            track.artwork_data.size());
            }

            ImGui::Separator();
            ImGui::TextDisabled("Keyboard: Left/Right, Space, C, H, Esc");
            ImGui::End();
        }

    private:
        MusicCardScreen& screen_;
        bool reported_frame_pacing_ = false;
};

// On Windows std::filesystem::path::string() converts through the ANSI code
// page, which corrupts non-ASCII (e.g. Chinese) filenames. Go through the
// wide representation and convert to UTF-8 explicitly so that downstream
// readers (TagLib, audio decoders) receive valid UTF-8 paths.
[[nodiscard]] auto PathToUtf8(const std::filesystem::path& path) -> std::string {
#ifdef _WIN32
    const auto& wide = path.wstring();
    if (wide.empty()) {
        return {};
    }
    const auto size =
        ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string utf8;
    utf8.resize(static_cast<std::size_t>(size));
    ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
#else
    return path.string();
#endif
}

// Discovers audio files under the configured directory. Metadata and decoder
// work are deferred to the background prefetch loader so startup remains
// responsive and open file handles stay bounded.
[[nodiscard]] auto LoadTrackPaths(const std::string& music_root) -> std::vector<std::string> {
    constexpr std::array audio_extensions{".mp3", ".wav", ".flac", ".ogg",  ".m4a",
                                          ".aac", ".wma", ".opus", ".aiff", ".aif"};
    auto paths = std::vector<std::string>{};
    const auto music_dir = std::filesystem::path{music_root};
    std::error_code ec;
    if (!std::filesystem::is_directory(music_dir, ec)) {
        LOG_ERROR(atom::audio::LogChannel::MUSIC, "Music path is not a directory: " + music_root);
        return paths;
    }
    for (const auto& entry : std::filesystem::directory_iterator(music_dir, ec)) {
        if (ec) {
            break;
        }
        std::error_code entry_ec;
        if (!entry.is_regular_file(entry_ec) || entry_ec) {
            continue;
        }
        auto ext = entry.path().extension().string();
        std::ranges::transform(ext, ext.begin(),
                               [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (std::ranges::find(audio_extensions, ext) != audio_extensions.end()) {
            paths.push_back(PathToUtf8(entry.path()));
        }
    }
    if (ec) {
        LOG_WARNING(atom::audio::LogChannel::MUSIC, "Directory iteration error: " + ec.message());
    }
    std::ranges::sort(paths);
    LOG_INFO(atom::audio::LogChannel::MUSIC, "Discovered " + std::to_string(paths.size()) + " audio file(s) in " +
                                                 music_root + " (metadata deferred to lazy loader)");
    return paths;
}
} // namespace

auto main(int argc, char** argv) -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    // Raise the C runtime file-descriptor ceiling. Each streaming audio decoder
    // keeps its source file open; with lazy loading we keep ≤ 3 decoders alive,
    // but a generous ceiling prevents EMFILE surprises during metadata reads.
    _setmaxstdio(512);
#endif // _WIN32
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_DEBUG);
    atom::AudioMixer mixer;
    atom::MusicPlayer music{mixer};
    const std::string music_root = argc > 1 ? std::string{argv[1]} : std::string{MusicPath};
    auto paths = LoadTrackPaths(music_root);

    auto screen = std::make_unique<MusicCardScreen>(music, std::move(paths));
    auto* screen_pointer = screen.get();
    atom::ScreenManager::GetInstance().LoadScreen("MusicCard", std::move(screen));
    atom::ScreenManager::GetInstance().SwitchScreen("MusicCard");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom - Music Card Layout", atom::algo::Vec2{1920.0f, 1080.0f});
    window.SetVSync(false);
    window.SetFPS(165);

    MusicCardDebugger debugger{*screen_pointer};
    debugger.Attach(window);
    debugger.SetLoggerEnabled(true);
    auto renderer_shutdown = window.AddShutdownListener([screen_pointer] { screen_pointer->ShutdownRenderer(); });
    screen_pointer->LoadInterfaceFont();

    window.Run();
    return 0;
}
