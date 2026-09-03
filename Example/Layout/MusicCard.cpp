// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file MusicCard.cpp
 * @brief Yoga and SDL3 proof-of-concept for an animated music HUD card.
 * @author Romi Brooks
 * @date 2026/09/01
 * @attention Temporary widgets and animation helpers intentionally live only
 *            in this example.
 */

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <Backend/Runtime/BackendRuntime.hpp>
#include <Backend/SDL3/Window/ISDL3WindowExtensions.hpp>
#include <Layout/LayoutConfig.hpp>
#include <Layout/LayoutNode.hpp>
#include <Layout/LayoutTypes.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Metadata/AudioMetadataReader.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Window/Debugger/ImGui/ImGuiFontLoader.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Overlay.hpp>
#include <Window/RenderWindow.hpp>
#include <Window/Screen.hpp>

#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif // _WIN32

namespace {
constexpr auto kMusic1Path = R"(E:\Music\汪苏泷,By2 - 有点甜.mp3)";
constexpr auto kMusic2Path = R"(E:\Music\张芸京 - 偏爱.mp3)";

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
        std::string title;
        std::string artist;
        std::string path;
        Palette palette;
        bool is_loaded = false;
        std::string artwork_mime_type;
        std::vector<uint8_t> artwork_data;
};

enum class CardCorner { BottomLeft, TopRight };

enum class AnimationState { Entering, Visible, Exiting, Hidden };

#ifdef _WIN32
[[nodiscard]] auto CreateArtworkTexture(SDL_Renderer& renderer, const std::vector<uint8_t>& encoded_image)
    -> SDL_Texture* {
    if (encoded_image.empty()) {
        LOG_DEBUG(atom::audio::LogChannel::METADATA, "No embedded artwork bytes; using the generated cover fallback");
        return nullptr;
    }
    if (encoded_image.size() > std::numeric_limits<DWORD>::max()) {
        LOG_WARNING(atom::audio::LogChannel::METADATA,
                    "Embedded artwork is too large for the Windows WIC memory decoder");
        return nullptr;
    }

    const auto com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto should_uninitialize = SUCCEEDED(com_result);

    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;

    auto result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) {
        result = factory->CreateStream(&stream);
    }
    if (SUCCEEDED(result)) {
        result = stream->InitializeFromMemory(const_cast<BYTE*>(encoded_image.data()),
                                              static_cast<DWORD>(encoded_image.size()));
    }
    if (SUCCEEDED(result)) {
        result = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    }
    if (SUCCEEDED(result)) {
        result = decoder->GetFrame(0, &frame);
    }
    if (SUCCEEDED(result)) {
        result = factory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(result)) {
        result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                       WICBitmapPaletteTypeCustom);
    }

    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(result)) {
        result = converter->GetSize(&width, &height);
    }
    if (FAILED(result) || width == 0 || height == 0 || width > static_cast<UINT>(std::numeric_limits<int>::max()) ||
        height > static_cast<UINT>(std::numeric_limits<int>::max()) ||
        width > std::numeric_limits<std::size_t>::max() / 4U / height ||
        static_cast<std::size_t>(width) * 4U * height > std::numeric_limits<UINT>::max()) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        LOG_ERROR(atom::audio::LogChannel::METADATA,
                  "Failed to decode embedded artwork with WIC (HRESULT=" + std::to_string(static_cast<long>(result)) +
                      ", bytes=" + std::to_string(encoded_image.size()) + ")");
        return nullptr;
    }

    const auto pitch = width * 4U;
    auto pixels = std::vector<uint8_t>(static_cast<std::size_t>(pitch) * height);
    result = converter->CopyPixels(nullptr, pitch, static_cast<UINT>(pixels.size()), pixels.data());
    SDL_Texture* texture = nullptr;
    if (SUCCEEDED(result)) {
        auto* surface = SDL_CreateSurfaceFrom(static_cast<int>(width), static_cast<int>(height), SDL_PIXELFORMAT_RGBA32,
                                              pixels.data(), static_cast<int>(pitch));
        if (surface != nullptr) {
            texture = SDL_CreateTextureFromSurface(&renderer, surface);
            SDL_DestroySurface(surface);
        } else {
            LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                      "Failed to create artwork surface: " + std::string{SDL_GetError()});
        }
    }
    if (texture != nullptr) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        LOG_INFO(atom::backend::sdl3::LogChannel::RENDER, "Decoded embedded artwork into an SDL texture (" +
                                                              std::to_string(width) + "x" + std::to_string(height) +
                                                              ", bytes=" + std::to_string(encoded_image.size()) + ")");
    } else if (SUCCEEDED(result)) {
        LOG_ERROR(atom::backend::sdl3::LogChannel::RENDER,
                  "Failed to create artwork texture: " + std::string{SDL_GetError()});
    } else {
        LOG_ERROR(atom::audio::LogChannel::METADATA,
                  "Failed to copy decoded artwork pixels (HRESULT=" + std::to_string(static_cast<long>(result)) + ")");
    }
    if (should_uninitialize) {
        CoUninitialize();
    }
    return texture;
}
#endif // _WIN32

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

[[nodiscard]] auto ToAscii(std::string text, const std::size_t maximum_length) -> std::string {
    for (auto& character : text) {
        const auto value = static_cast<unsigned char>(character);
        if (value < 32 || value > 126) {
            character = '?';
        }
    }
    if (text.size() > maximum_length) {
        text.resize(maximum_length > 3 ? maximum_length - 3 : maximum_length);
        text += "...";
    }
    return text;
}

class SdlPainter {
    public:
        explicit SdlPainter(SDL_Renderer& renderer) : renderer_{renderer} {
            SDL_SetRenderDrawBlendMode(&renderer_, SDL_BLENDMODE_BLEND);
        }

        [[nodiscard]] auto GetRenderer() -> SDL_Renderer& {
            return renderer_;
        }

        auto FillRect(const FloatRect& rect, const atom::render::Color color) -> void {
            SDL_SetRenderDrawColor(&renderer_, color.r, color.g, color.b, color.a);
            const auto destination = SDL_FRect{rect.x, rect.y, rect.width, rect.height};
            SDL_RenderFillRect(&renderer_, &destination);
        }

        auto FillRoundedRect(const FloatRect& rect, const float radius, const atom::render::Color color) -> void {
            const auto safe_radius = std::min({radius, rect.width * 0.5f, rect.height * 0.5f});
            if (safe_radius <= 0.0f) {
                FillRect(rect, color);
                return;
            }

            FillRect({rect.x + safe_radius, rect.y, rect.width - safe_radius * 2.0f, rect.height}, color);
            FillRect({rect.x, rect.y + safe_radius, rect.width, rect.height - safe_radius * 2.0f}, color);

            SDL_SetRenderDrawColor(&renderer_, color.r, color.g, color.b, color.a);
            const auto line_count = static_cast<int>(std::ceil(safe_radius));
            for (auto line = 0; line < line_count; ++line) {
                const auto y = static_cast<float>(line) + 0.5f;
                const auto distance = safe_radius - y;
                const auto inset =
                    safe_radius - std::sqrt(std::max(0.0f, safe_radius * safe_radius - distance * distance));
                const auto width = rect.width - inset * 2.0f;
                const auto top = SDL_FRect{rect.x + inset, rect.y + static_cast<float>(line), width, 1.0f};
                const auto bottom = SDL_FRect{
                    rect.x + inset,
                    rect.y + rect.height - static_cast<float>(line) - 1.0f,
                    width,
                    1.0f,
                };
                SDL_RenderFillRect(&renderer_, &top);
                SDL_RenderFillRect(&renderer_, &bottom);
            }
        }

        auto FillCircle(const float center_x, const float center_y, const float radius, const atom::render::Color color)
            -> void {
            SDL_SetRenderDrawColor(&renderer_, color.r, color.g, color.b, color.a);
            const auto line_count = static_cast<int>(std::ceil(radius));
            for (auto line = -line_count; line <= line_count; ++line) {
                const auto y = static_cast<float>(line);
                const auto half_width = std::sqrt(std::max(0.0f, radius * radius - y * y));
                const auto scanline = SDL_FRect{center_x - half_width, center_y + y, half_width * 2.0f, 1.0f};
                SDL_RenderFillRect(&renderer_, &scanline);
            }
        }

        auto DrawText(const float x, const float y, const std::string& text, const float scale,
                      const atom::render::Color color) -> void {
            float previous_x = 1.0f;
            float previous_y = 1.0f;
            SDL_GetRenderScale(&renderer_, &previous_x, &previous_y);
            SDL_SetRenderDrawColor(&renderer_, color.r, color.g, color.b, color.a);
            SDL_SetRenderScale(&renderer_, scale, scale);
            SDL_RenderDebugText(&renderer_, x / scale, y / scale, text.c_str());
            SDL_SetRenderScale(&renderer_, previous_x, previous_y);
        }

        auto DrawTexture(SDL_Texture& texture, const FloatRect& rect, const float opacity) -> void {
            const auto destination = SDL_FRect{rect.x, rect.y, rect.width, rect.height};
            SDL_SetTextureAlphaModFloat(&texture, Clamp01(opacity));
            SDL_RenderTexture(&renderer_, &texture, nullptr, &destination);
        }

    private:
        SDL_Renderer& renderer_;
};

class MusicCardScreen final : public atom::Screen {
    public:
        MusicCardScreen(atom::MusicPlayer& music, std::vector<Track> tracks)
            : music_{music}, tracks_{std::move(tracks)}, root_{layout_config_}, card_{layout_config_},
              cover_{layout_config_}, details_{layout_config_}, title_{layout_config_}, author_{layout_config_},
              controls_{layout_config_}, previous_button_{layout_config_}, play_button_{layout_config_},
              next_button_{layout_config_} {
            layout_config_.SetPointScaleFactor(1.0f);
            artwork_textures_.resize(tracks_.size());
            artwork_texture_attempted_.resize(tracks_.size());
            BuildLayoutTree();
            LOG_INFO(atom::core::LogChannel::SCREEN,
                     "Music card initialized with " + std::to_string(tracks_.size()) + " track(s)");
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
            if (animation_state_ == AnimationState::Hidden) {
                RestartEntrance();
            } else if (animation_state_ == AnimationState::Exiting) {
                RestartEntrance();
            } else {
                animation_state_ = AnimationState::Exiting;
            }
            LOG_DEBUG(atom::core::LogChannel::SCREEN,
                      "Music card visibility transition requested; state=" + std::string{GetAnimationStateName()});
        }

        [[nodiscard]] auto GetCurrentTrack() const -> const Track& {
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
                "C:/Windows/Fonts/NotoSansSC-VF.ttf",
                "C:/Windows/Fonts/msyh.ttc",
                "C:/Windows/Fonts/simhei.ttf",
            };
#else
            constexpr std::array font_candidates{
                "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
                "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
            };
#endif // _WIN32
            for (const auto* font_path : font_candidates) {
                if (!std::filesystem::exists(font_path)) {
                    continue;
                }
                interface_font_ = atom::debugger::ImGuiFontLoader::LoadFromFile(
                    font_path, {.size_pixels = 18.0f,
                                .glyph_preset = atom::debugger::ImGuiGlyphPreset::ChineseFull,
                                .set_as_default = true});
                if (interface_font_ != nullptr) {

                    return true;
                }
            }
            LOG_WARNING(atom::debugger::LogChannel::IMGUI,
                        "No suitable CJK interface font was loaded; SDL fallback text cannot render Chinese glyphs");
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

        auto Render(atom::render::IRenderTarget& target) -> void override {
            target.Clear(atom::render::Color{8, 10, 18, 255});
            const auto* extensions = dynamic_cast<atom::backend::sdl3::ISDL3WindowExtensions*>(&target);
            if (extensions == nullptr || extensions->GetNativeRenderer() == nullptr) {
                return;
            }

            auto painter = SdlPainter{*extensions->GetNativeRenderer()};
            DrawBackground(painter, target);
            UpdateLayout(target);
            DrawCard(painter);
            DrawInstructions(painter, target);
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
        auto BuildLayoutTree() -> void {
            auto card_style = atom::layout::LayoutStyle{};
            card_style.width = atom::layout::Length::Points(430.0f);
            card_style.height = atom::layout::Length::Points(132.0f);
            card_style.padding = atom::layout::Edges::All(atom::layout::Length::Points(14.0f));
            card_style.column_gap = 16.0f;
            card_style.position_type = atom::layout::PositionType::Absolute;
            card_.SetStyle(card_style);

            auto cover_style = atom::layout::LayoutStyle{};
            cover_style.width = atom::layout::Length::Points(104.0f);
            cover_style.height = atom::layout::Length::Points(104.0f);
            cover_.SetStyle(cover_style);

            auto details_style = atom::layout::LayoutStyle{};
            details_style.flex_direction = atom::layout::FlexDirection::Column;
            details_style.flex_grow = 1.0f;
            details_style.padding.top = atom::layout::Length::Points(4.0f);
            details_style.padding.bottom = atom::layout::Length::Points(2.0f);
            details_style.row_gap = 5.0f;
            details_.SetStyle(details_style);

            auto title_style = atom::layout::LayoutStyle{};
            title_style.height = atom::layout::Length::Points(22.0f);
            title_.SetStyle(title_style);

            auto author_style = atom::layout::LayoutStyle{};
            author_style.height = atom::layout::Length::Points(15.0f);
            author_.SetStyle(author_style);

            auto controls_style = atom::layout::LayoutStyle{};
            controls_style.height = atom::layout::Length::Points(42.0f);
            controls_style.margin.top = atom::layout::Length::Auto();
            controls_style.column_gap = 10.0f;
            controls_style.align_items = atom::layout::Align::Center;
            controls_.SetStyle(controls_style);

            auto button_style = atom::layout::LayoutStyle{};
            button_style.width = atom::layout::Length::Points(34.0f);
            button_style.height = atom::layout::Length::Points(34.0f);
            previous_button_.SetStyle(button_style);
            next_button_.SetStyle(button_style);
            button_style.width = atom::layout::Length::Points(42.0f);
            button_style.height = atom::layout::Length::Points(42.0f);
            play_button_.SetStyle(button_style);

            root_.AppendChild(card_);
            card_.AppendChild(cover_);
            card_.AppendChild(details_);
            details_.AppendChild(title_);
            details_.AppendChild(author_);
            details_.AppendChild(controls_);
            controls_.AppendChild(previous_button_);
            controls_.AppendChild(play_button_);
            controls_.AppendChild(next_button_);
        }

        auto UpdateLayout(atom::render::IRenderTarget& target) -> void {
            const auto window_size = target.GetSize();
            auto root_style = atom::layout::LayoutStyle{};
            root_style.width = atom::layout::Length::Points(window_size.GetX());
            root_style.height = atom::layout::Length::Points(window_size.GetY());
            root_.SetStyle(root_style);

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
            card_.SetStyle(card_style);
            root_.CalculateLayout();
        }

        [[nodiscard]] auto GlobalRect(const atom::layout::LayoutNode& node, const FloatRect& parent) const
            -> FloatRect {
            const auto layout = node.GetLayout();
            return {parent.x + layout.left, parent.y + layout.top, layout.width, layout.height};
        }

        auto DrawBackground(SdlPainter& painter, atom::render::IRenderTarget& target) const -> void {
            const auto size = target.GetSize();
            constexpr auto band_count = 12;
            const auto band_height = size.GetY() / static_cast<float>(band_count);
            for (auto index = 0; index < band_count; ++index) {
                const auto blend = static_cast<float>(index) / static_cast<float>(band_count - 1);
                const auto color = atom::render::Color{
                    static_cast<uint8_t>(8.0f + blend * 8.0f),
                    static_cast<uint8_t>(10.0f + blend * 9.0f),
                    static_cast<uint8_t>(18.0f + blend * 17.0f),
                    255,
                };
                painter.FillRect({0.0f, band_height * static_cast<float>(index), size.GetX(), band_height + 1.0f},
                                 color);
            }

            painter.FillCircle(size.GetX() * 0.72f, size.GetY() * 0.30f, 150.0f, atom::render::Color{43, 36, 84, 34});
            painter.FillCircle(size.GetX() * 0.23f, size.GetY() * 0.68f, 110.0f, atom::render::Color{21, 87, 96, 25});
        }

        auto DrawCard(SdlPainter& painter) -> void {
            previous_hitbox_ = {};
            play_hitbox_ = {};
            next_hitbox_ = {};
            if (animation_state_ == AnimationState::Hidden || tracks_.empty()) {
                return;
            }

            const auto card_layout = card_.GetLayout();
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
            painter.FillRoundedRect(card, 24.0f, WithAlpha(atom::render::Color{22, 25, 37, 242}, panel_opacity));
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
            const auto& track = tracks_[current_track_];

            DrawInterfaceText(painter, {title_x, title_final.y, title_final.width, title_final.height}, track.title,
                              21.0f,
                              WithAlpha(atom::render::Color{245, 247, 255, 255}, title_progress * panel_opacity));
            DrawInterfaceText(painter, {author_x, author_final.y, author_final.width, author_final.height},
                              track.artist, 14.0f,
                              WithAlpha(atom::render::Color{157, 164, 184, 255}, author_progress * panel_opacity));

            const auto previous_final = GlobalRect(previous_button_, controls_final);
            const auto play_final = GlobalRect(play_button_, controls_final);
            const auto next_final = GlobalRect(next_button_, controls_final);
            const auto controls_y = (1.0f - controls_progress) * 18.0f;
            previous_hitbox_ = OffsetRect(previous_final, 0.0f, controls_y);
            play_hitbox_ = OffsetRect(play_final, 0.0f, controls_y);
            next_hitbox_ = OffsetRect(next_final, 0.0f, controls_y);
            DrawControls(painter, controls_progress * panel_opacity);
        }

        auto DrawInterfaceText(SdlPainter& painter, const FloatRect& rect, const std::string& text,
                               const float font_size, const atom::render::Color color) const -> void {
            if (interface_font_ == nullptr) {
                painter.DrawText(rect.x, rect.y, ToAscii(text, 32), font_size / 12.0f, color);
                return;
            }

            const auto clip = ImVec4{rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
            ImGui::GetForegroundDrawList()->AddText(interface_font_, font_size, ImVec2{rect.x, rect.y},
                                                    IM_COL32(color.r, color.g, color.b, color.a), text.c_str(), nullptr,
                                                    0.0f, &clip);
        }

        auto DrawAlbumCover(SdlPainter& painter, const FloatRect& rect, const float opacity) -> void {
            const auto& palette = tracks_[current_track_].palette;
            painter.FillRoundedRect(OffsetRect(rect, 0.0f, 5.0f), 18.0f,
                                    WithAlpha(atom::render::Color{0, 0, 0, 90}, opacity));
            painter.FillRoundedRect(rect, 18.0f, WithAlpha(palette.primary, opacity));

#ifdef _WIN32
            if (!artwork_texture_attempted_[current_track_]) {
                artwork_texture_attempted_[current_track_] = true;
                artwork_textures_[current_track_] =
                    CreateArtworkTexture(painter.GetRenderer(), tracks_[current_track_].artwork_data);
            }
            if (artwork_textures_[current_track_] != nullptr) {
                const auto image_rect = FloatRect{rect.x + 2.0f, rect.y + 2.0f, rect.width - 4.0f, rect.height - 4.0f};
                painter.DrawTexture(*artwork_textures_[current_track_], image_rect, opacity);
                painter.FillRoundedRect({rect.x, rect.y, rect.width, 2.0f}, 1.0f,
                                        WithAlpha(atom::render::Color{255, 255, 255, 70}, opacity));
                return;
            }
#else
            if (!artwork_texture_attempted_[current_track_]) {
                artwork_texture_attempted_[current_track_] = true;
                if (!tracks_[current_track_].artwork_data.empty()) {
                    LOG_WARNING(
                        atom::audio::LogChannel::METADATA,
                        "Embedded artwork is available, but this example has no decoder for the current platform; "
                        "using the generated cover fallback");
                }
            }
#endif // _WIN32

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

        auto DrawControls(SdlPainter& painter, const float opacity) const -> void {
            const auto secondary = WithAlpha(atom::render::Color{189, 195, 214, 255}, opacity);
            const auto primary = WithAlpha(tracks_[current_track_].palette.accent, opacity);
            painter.FillCircle(play_hitbox_.x + play_hitbox_.width * 0.5f, play_hitbox_.y + play_hitbox_.height * 0.5f,
                               play_hitbox_.width * 0.5f, primary);

            painter.DrawText(previous_hitbox_.x + 8.0f, previous_hitbox_.y + 10.0f, "<<", 1.15f, secondary);
            painter.DrawText(next_hitbox_.x + 8.0f, next_hitbox_.y + 10.0f, ">>", 1.15f, secondary);
            painter.DrawText(play_hitbox_.x + (is_playing_ ? 13.0f : 15.0f), play_hitbox_.y + 13.0f,
                             is_playing_ ? "||" : ">", 1.1f, WithAlpha(atom::render::Color{16, 18, 27, 255}, opacity));
        }

        auto DrawInstructions(SdlPainter& painter, atom::render::IRenderTarget& target) const -> void {
            const auto size = target.GetSize();
            painter.DrawText(28.0f, size.GetY() - 24.0f,
                             "LEFT/RIGHT TRACK   SPACE PLAY/STOP   C CORNER   H HIDE/SHOW   ESC EXIT", 1.0f,
                             atom::render::Color{104, 111, 133, 220});
            if (!HasLoadedTracks()) {
                painter.DrawText(28.0f, 24.0f, "AUDIO FILE FAILED TO LOAD", 1.0f,
                                 atom::render::Color{201, 161, 92, 230});
            }
        }

        [[nodiscard]] auto HasLoadedTracks() const -> bool {
            return std::ranges::any_of(tracks_, [](const Track& track) { return track.is_loaded; });
        }

        auto RequestRelativeTrack(const int offset) -> void {
            if (tracks_.empty()) {
                return;
            }
            const auto count = static_cast<int>(tracks_.size());
            pending_track_ = static_cast<std::size_t>((static_cast<int>(current_track_) + offset + count) % count);
            LOG_DEBUG(atom::core::LogChannel::SCREEN,
                      "Music card track transition requested: " + std::to_string(current_track_) + " -> " +
                          std::to_string(pending_track_));
            if (animation_state_ == AnimationState::Hidden) {
                CompleteExit();
            } else {
                animation_state_ = AnimationState::Exiting;
            }
        }

        auto CompleteExit() -> void {
            if (pending_track_ < tracks_.size()) {
                StopCurrentTrack();
                current_track_ = pending_track_;
                pending_track_ = tracks_.size();
                StartCurrentTrack();
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
            is_playing_ = true;
            if (tracks_[current_track_].is_loaded) {
                LOG_INFO(atom::audio::LogChannel::MUSIC, "Music card playing track: " + tracks_[current_track_].title);
                music_.Play(tracks_[current_track_].id);
            } else {
                LOG_WARNING(atom::audio::LogChannel::MUSIC,
                            "Music music card cannot play unloaded track: " + tracks_[current_track_].path);
            }
        }

        auto StopCurrentTrack() -> void {
            if (!tracks_.empty() && tracks_[current_track_].is_loaded) {
                LOG_INFO(atom::audio::LogChannel::MUSIC, "Music card stopping track: " + tracks_[current_track_].title);
                music_.Stop(tracks_[current_track_].id);
            }
            is_playing_ = false;
        }

        atom::MusicPlayer& music_;
        std::vector<Track> tracks_;
        std::size_t current_track_ = 0;
        std::size_t pending_track_ = tracks_.size();
        bool is_playing_ = false;
        CardCorner corner_ = CardCorner::BottomLeft;
        AnimationState animation_state_ = AnimationState::Entering;
        float animation_progress_ = 0.0f;
        float visible_time_ = 0.0f;

        ImFont* interface_font_ = nullptr;
        std::vector<SDL_Texture*> artwork_textures_;
        std::vector<bool> artwork_texture_attempted_;

        atom::layout::LayoutConfig layout_config_;
        atom::layout::LayoutNode root_;
        atom::layout::LayoutNode card_;
        atom::layout::LayoutNode cover_;
        atom::layout::LayoutNode details_;
        atom::layout::LayoutNode title_;
        atom::layout::LayoutNode author_;
        atom::layout::LayoutNode controls_;
        atom::layout::LayoutNode previous_button_;
        atom::layout::LayoutNode play_button_;
        atom::layout::LayoutNode next_button_;

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
            ImGui::Text("FPS: %.1f / target 165", static_cast<double>(GetFPS()));
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
                const auto& track = screen_.GetCurrentTrack();
                ImGui::Text("Title: %s", track.title.c_str());
                ImGui::Text("Artist: %s", track.artist.c_str());
                ImGui::Text("Audio: %s", track.is_loaded ? "loaded" : "visual demo only");
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

[[nodiscard]] auto LoadTracks(atom::MusicPlayer& music) -> std::vector<Track> {
    constexpr std::array music_paths{kMusic1Path, kMusic2Path};
    constexpr std::array palettes{
        Palette{{52, 45, 111, 255}, {92, 74, 168, 255}, {132, 219, 214, 255}},
        Palette{{24, 82, 96, 255}, {39, 134, 139, 255}, {245, 188, 111, 255}},
    };
    auto tracks = std::vector<Track>{};
    tracks.reserve(music_paths.size());
    for (auto index = std::size_t{0}; index < music_paths.size(); ++index) {
        const auto path = std::string{music_paths[index]};
        auto metadata = atom::audio::AudioMetadataReader::Read(path);
        auto title =
            metadata && !metadata->title.empty() ? metadata->title : std::filesystem::path{path}.stem().string();
        auto artist = metadata && !metadata->artist.empty() ? metadata->artist : std::string{"UNKNOWN ARTIST"};
        const auto id = "music_card_track_" + std::to_string(index);
        const auto is_loaded = music.Load(id, path);
        auto artwork_mime_type = metadata ? std::move(metadata->artworkMimeType) : std::string{};
        auto artwork_data = metadata ? std::move(metadata->artworkData) : std::vector<uint8_t>{};
        tracks.push_back({id, std::move(title), std::move(artist), path, palettes[index], is_loaded,
                          std::move(artwork_mime_type), std::move(artwork_data)});
    }
    return tracks;
}
} // namespace

auto main() -> int {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32
    atom::AudioMixer mixer;
    atom::MusicPlayer music{mixer};
    auto tracks = LoadTracks(music);

    auto screen = std::make_unique<MusicCardScreen>(music, std::move(tracks));
    auto* screen_pointer = screen.get();
    atom::ScreenManager::GetInstance().LoadScreen("MusicCard", std::move(screen));
    atom::ScreenManager::GetInstance().SwitchScreen("MusicCard");

    auto& window = atom::RenderWindow::GetInstance();
    window.Initialize("Atom - Music Card Layout", atom::algo::Vec2{1100.0f, 720.0f});
    window.SetVSync(false);
    window.SetFPS(165);

    MusicCardDebugger debugger{*screen_pointer};
    debugger.Attach(window);
    screen_pointer->LoadInterfaceFont();

    window.Run();
    return 0;
}
