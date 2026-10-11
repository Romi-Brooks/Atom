/**
 * @file           : MusicCardScreen.hpp
 * @brief          : The MusicCard screen: layout, rendering, input, and the
 *                   background metadata/audio prefetch loader.
 *
 * Declarations only; the implementation lives in MusicCardScreen.cpp. Shared
 * types are in MusicCardTypes.hpp, the ImGui debugger panel is in
 * MusicCardDebugger.hpp and the entry point is MusicCard.cpp.
 */

#ifndef ATOM_EXAMPLE_MUSIC_CARD_SCREEN_HPP
#define ATOM_EXAMPLE_MUSIC_CARD_SCREEN_HPP

#include "MusicCardTypes.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <Backend/Runtime/IAudioBackendChangeListener.hpp>
#include <Concurrency/JobSystem.hpp>
#include <Layout/LayoutTree.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Render/Resources/TextureCache.hpp>
#include <Window/Screen.hpp>

class MusicCardScreen final : public atom::Screen, public atom::backend::IAudioBackendChangeListener {
    public:
        MusicCardScreen(atom::MusicPlayer& music, std::vector<std::string> paths,
                        std::shared_ptr<atom::fs::NativeFileSystem> filesystem);
        ~MusicCardScreen() override;

        // Backend switching invalidates every cached track resolution.
        auto OnAudioBackendChanging() -> void override;
        auto OnAudioBackendChanged() -> void override;

        auto Shutdown() -> void;

        auto Play() -> void;
        auto TogglePlayback() -> void;
        auto Stop() -> void;
        auto Previous() -> void;
        auto Next() -> void;
        auto ToggleCorner() -> void;
        auto ToggleCardVisibility() -> void;

        // Snapshot of the current track's display fields. The full Track is
        // never exposed by reference because the background loader may write
        // metadata at any time. The embedded artwork bytes are deliberately
        // *not* part of this snapshot (see the implementation for why).
        struct TrackDisplay {
                std::string title;
                std::string artist;
                std::string artwork_mime_type;
                std::size_t artwork_bytes = 0;
                bool is_loaded = false;
                bool metadata_loaded = false;
        };

        [[nodiscard]] auto GetCurrentDisplay() const -> TrackDisplay;
        [[nodiscard]] auto HasTracks() const -> bool;
        [[nodiscard]] auto IsPlaying() const -> bool;
        [[nodiscard]] auto IsCardVisible() const -> bool;

        [[nodiscard]] auto GetNowPlayingId() const -> std::string;
        [[nodiscard]] auto GetPlaybackPosition(const std::string& id) const -> float;
        [[nodiscard]] auto GetPlaybackDuration(const std::string& id) const -> float;
        [[nodiscard]] auto CanSeek(const std::string& id) const -> bool;
        auto Seek(const std::string& id, float seconds) -> bool;

        auto LoadInterfaceFont() -> bool;
        [[nodiscard]] auto GetCornerName() const -> std::string_view;
        [[nodiscard]] auto GetAnimationStateName() const -> std::string_view;

        auto Render(atom::render::IRenderDevice& device) -> void override;
        auto HandleEvent(const atom::window::IEvent& event) -> bool override;
        auto Update(float delta_time) -> void override;

    private:
        // How many neighbours of the current track stay resolved.
        static constexpr std::size_t residency_window = 1;

        [[nodiscard]] static auto IsWithinResidencyWindow(std::size_t index, std::size_t center,
                                                          std::size_t count) -> bool;

        auto MaintainResidency() -> void;
        [[nodiscard]] auto IsResidentLocked(std::size_t index) const -> bool;

        auto EnsureBackgroundTexture() -> void;
        auto BuildLayoutTree() -> void;
        auto UpdateLayout() -> void;
        [[nodiscard]] auto GlobalRect(atom::layout::LayoutTree::NodeId node, const atom::algo::Rect& parent) const
            -> atom::algo::Rect;

        auto DrawBackground(CardPainter& painter) -> void;
        [[nodiscard]] auto ComputeAnimatedCardRect() const -> atom::algo::Rect;
        auto UpdateCardPostProcessRegion() -> void;
        auto DrawCard(CardPainter& painter, bool draw_text = true) -> void;
        auto DrawCardText() -> void;
        auto DrawInterfaceText(const atom::algo::Rect& rect, const std::string& text, float font_size,
                               atom::render::Color color) -> void;
        auto DrawAlbumCover(CardPainter& painter, const atom::algo::Rect& rect, float opacity) -> void;
        auto DrawControls(CardPainter& painter, float opacity) const -> void;
        auto DrawTriangleGlyph(CardPainter& painter, const atom::algo::Rect& box, bool points_right,
                               atom::render::Color color) const -> void;
        auto DrawPlayPauseGlyph(CardPainter& painter, atom::render::Color color) const -> void;

        [[nodiscard]] auto HasLoadedTracks() const -> bool;
        auto RequestRelativeTrack(int offset) -> void;
        auto CompleteExit() -> void;
        auto RestartEntrance() -> void;
        auto TryStartAfterLoad() -> void;
        auto HandlePlaybackCompletion() -> void;
        auto PauseCurrentTrack() -> void;
        auto StartCurrentTrack() -> void;
        auto StopCurrentTrack() -> void;

        auto BuildTrackStubs(std::vector<std::string> paths) -> void;
        auto LoadTrackMetadata(std::size_t index) -> void;
        auto LoadTrackAudio(std::size_t index) -> void;
        auto DrainPendingUnloads() -> void;
        auto LoaderLoop(std::stop_token stop) -> void;

        // Schedules a Play() on the job system so the (potentially blocking)
        // stream-buffer fill never runs on the render thread.
        auto DispatchPlayOnJobSystem(const std::string& id) -> void;

        atom::MusicPlayer& music_;
        std::shared_ptr<atom::fs::NativeFileSystem> filesystem_;
        std::vector<Track> tracks_;
        std::size_t current_track_ = 0;
        std::size_t pending_track_ = tracks_.size();
        bool is_playing_ = false;
        bool start_after_load_ = false;
        // Lazy-loading state. tracks_mutex_ protects every mutable field of
        // Track (title, artist, is_loaded, artwork_*, metadata_loaded); id,
        // path and theme are immutable after construction.
        mutable std::mutex tracks_mutex_;
        std::uint64_t cache_generation_ = 0;
        std::size_t resident_center_ = 0;
        std::condition_variable_any loader_cv_;
        std::jthread loader_thread_;
        bool prefetch_requested_ = false;
        // Track ids the render thread wants released, drained by the loader
        // thread. Guarded by tracks_mutex_.
        std::vector<std::string> pending_unloads_;
        // Handles for in-flight audio loads (music_.Load) so the screen can
        // observe completion without joining anything on the render thread.
        std::vector<atom::job::JobHandle> audio_jobs_;
        CardCorner corner_ = CardCorner::BottomLeft;
        AnimationState animation_state_ = AnimationState::Entering;
        float animation_progress_ = 0.0f;
        float visible_time_ = 0.0f;
        float effect_time_ = 0.0f;
        float glitch_direction_ = 1.0f;
        float window_width_ = 0.0f;
        float window_height_ = 0.0f;
        atom::algo::Rect current_card_rect_{};
        atom::algo::Rect current_card_bounds_{};
        float reported_layout_width_ = -1.0f;
        float reported_layout_height_ = -1.0f;

        atom::render::Renderer2D renderer_;
        atom::render::resources::TextureCache texture_cache_{renderer_};
        atom::render::Font* interface_font_ = nullptr;
        std::vector<std::byte> interface_font_data_{};
        bool interface_font_attempted_ = false;
        bool renderer_initialization_failed_ = false;
        bool renderer_frame_failed_ = false;
        atom::render::resources::TextureHandle background_texture_{};
        bool background_texture_attempted_ = false;
        std::vector<atom::render::resources::TextureHandle> artwork_textures_;
        std::vector<bool> artwork_texture_attempted_;

        atom::layout::LayoutTree layout_tree_;
        atom::layout::LayoutTree::NodeId root_ = atom::layout::LayoutTree::invalid_node;
        atom::layout::LayoutTree::NodeId card_ = atom::layout::LayoutTree::invalid_node;
        atom::layout::LayoutTree::NodeId cover_ = atom::layout::LayoutTree::invalid_node;
        atom::layout::LayoutTree::NodeId details_ = atom::layout::LayoutTree::invalid_node;
        atom::layout::LayoutTree::NodeId title_ = atom::layout::LayoutTree::invalid_node;
        atom::layout::LayoutTree::NodeId author_ = atom::layout::LayoutTree::invalid_node;
        atom::layout::LayoutTree::NodeId controls_ = atom::layout::LayoutTree::invalid_node;
        atom::layout::LayoutTree::NodeId previous_button_ = atom::layout::LayoutTree::invalid_node;
        atom::layout::LayoutTree::NodeId play_button_ = atom::layout::LayoutTree::invalid_node;
        atom::layout::LayoutTree::NodeId next_button_ = atom::layout::LayoutTree::invalid_node;

        atom::algo::Rect previous_hitbox_;
        atom::algo::Rect play_hitbox_;
        atom::algo::Rect next_hitbox_;
};

#endif // ATOM_EXAMPLE_MUSIC_CARD_SCREEN_HPP
