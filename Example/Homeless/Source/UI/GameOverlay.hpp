#ifndef ATOM_HOMELESS_GAMEOVERLAY_HPP
#define ATOM_HOMELESS_GAMEOVERLAY_HPP

#include <memory>
#include <string>
#include <string_view>

#include <Algorithm/Geometry/Rect.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>
#include <Concurrency/Job.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Media/Audio/Playback/SFXPlayer.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>

namespace homeless {

struct TrackPresentation;

class GameOverlay final {
    public:
        GameOverlay(atom::render::Renderer2D& renderer, atom::AudioMixer& mixer, atom::MusicPlayer& music,
                    atom::SFXPlayer& effects);
        auto Load(const std::string& track_uri) -> bool;
        auto Update(float seconds) -> void;
        auto Draw(atom::render::IRenderDevice& device) -> bool;
        auto HandleEvent(const atom::window::IEvent& event) -> bool;
        auto ToggleEffects() -> void;
        auto ApplyAudioLevels() -> void;
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
        auto Activate(Action action, int direction) -> void;
        auto HandlePlayerClick() -> bool;
        auto ToggleTrack() -> void;
        auto ApplyPreparedTrack() -> void;
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
        [[nodiscard]] auto MenuRow(int index) const -> atom::algo::Rect;

        atom::render::Renderer2D& renderer_;
        atom::AudioMixer& mixer_;
        atom::MusicPlayer& music_;
        atom::SFXPlayer& effects_;
        atom::render::Font* font_ = nullptr;
        atom::render::Font* track_font_ = nullptr;
        atom::render::Renderer2D::Texture* cover_ = nullptr;
        std::shared_ptr<TrackPresentation> prepared_track_{};
        atom::job::JobHandle prepare_track_job_{};
        std::string title_ = "No track";
        std::string artist_{};
        atom::algo::Vec2 ui_size_{};
        atom::algo::Vec2 mouse_{};
        float ui_scale_ = 1.0f;
        float cover_angle_ = 0.0f;
        Page page_ = Page::Game;
        float last_sfx_ = 100.0f;
        bool track_available_ = false;
        bool playing_ = false;
};

} // namespace homeless

#endif // ATOM_HOMELESS_GAMEOVERLAY_HPP
