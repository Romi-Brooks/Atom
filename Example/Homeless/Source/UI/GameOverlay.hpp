#ifndef ATOM_HOMELESS_GAMEOVERLAY_HPP
#define ATOM_HOMELESS_GAMEOVERLAY_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <Atom/Core/Algorithm/Geometry/Rect.hpp>
#include <Atom/Backend/Contracts/Window/IWindow.hpp>
#include <Atom/Core/Concurrency/Job.hpp>
#include <Atom/Resources/Media/Audio/Mixing/AudioMixer.hpp>
#include <Atom/Presentation/Render/Renderer2D/Renderer2D.hpp>

#include "Content/LocalPlaylist.hpp"

namespace homeless {

struct TrackPresentation;

struct OverlayFonts {
        std::vector<char> latin;
        std::vector<char> chinese;
        std::vector<char> korean;
};

struct InteractionPrompt {
        std::string_view name{};
        std::string_view fallback_name{};
        bool checked = false;
};

class GameOverlay final {
    public:
        GameOverlay(atom::render::Renderer2D& renderer, atom::AudioMixer& mixer);
        [[nodiscard]] static auto ReadFonts() -> OverlayFonts;
        auto Load(const OverlayFonts& fonts, const std::filesystem::path& music_directory) -> bool;
        auto StopMusic() -> void {
            playlist_.Stop();
        }
        auto Update(float seconds) -> void;
        auto Draw(atom::render::IRenderDevice& device, InteractionPrompt prompt) -> bool;
        auto HandleEvent(const atom::window::IEvent& event) -> bool;
        auto ToggleEffects() -> void;
        [[nodiscard]] auto IsMenuOpen() const -> bool {
            return page_ != Page::Game;
        }

    private:
        enum class Page { Game, Pause, Settings };
        enum class Action {
            Continue,
            Settings,
            Quit,
            Back,
            WindowMode,
            Resolution,
            FrameRate,
            PresentMode,
            Music,
            Sfx
        };

        auto DrawPlayer() -> void;
        auto DrawMenu() -> void;
        auto DrawInteractionPrompt(InteractionPrompt prompt) -> void;
        auto Activate(Action action, int direction) -> void;
        auto HandlePlayerClick() -> bool;
        auto UpdateTrackDisplay() -> void;
        auto Text(std::string_view text, float x, float y, float size) -> void;
        auto Label(atom::render::Font& font, std::string_view text, float x, float y, float size,
                   atom::color::Color color) -> void;
        auto Fill(const atom::algo::Rect& rect, atom::color::Color color) -> void;
        auto Outline(const atom::algo::Rect& rect, atom::color::Color color) -> void;
        [[nodiscard]] auto ToPixels(const atom::algo::Rect& rect) const -> atom::algo::Rect;
        auto Button(const atom::algo::Rect& bounds, std::string_view label) -> void;
        [[nodiscard]] auto ToUi(float x, float y) const -> atom::algo::Vec2;
        [[nodiscard]] auto MenuRect() const -> atom::algo::Rect;
        [[nodiscard]] auto PlayerScreen() const -> atom::algo::Rect;
        [[nodiscard]] auto PlayButton() const -> atom::algo::Rect;
        [[nodiscard]] auto SkipButton(int direction) const -> atom::algo::Rect;
        [[nodiscard]] auto MenuRow(int index) const -> atom::algo::Rect;

        atom::render::Renderer2D& renderer_;
        atom::AudioMixer& mixer_;
        LocalPlaylist playlist_;
        atom::render::Font* font_ = nullptr;
        atom::render::Font* chinese_font_ = nullptr;
        atom::render::Font* korean_font_ = nullptr;
        atom::render::Font* track_font_ = nullptr;
        atom::render::Renderer2D::Texture* cover_ = nullptr;
        std::shared_ptr<TrackPresentation> prepared_track_{};
        atom::job::JobHandle prepare_track_job_{};
        std::string selected_uri_{};
        std::string artwork_uri_{};
        std::string title_ = "No track";
        std::string artist_{};
        atom::algo::Vec2 ui_size_{};
        atom::algo::Vec2 mouse_{};
        float ui_scale_ = 1.0f;
        float cover_angle_ = 0.0f;
        Page page_ = Page::Game;
        float last_sfx_ = 100.0f;
        bool show_cover_ = false;
};

} // namespace homeless

#endif // ATOM_HOMELESS_GAMEOVERLAY_HPP
