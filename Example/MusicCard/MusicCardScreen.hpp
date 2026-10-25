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

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <Backend/Composition/IAudioBackendChangeListener.hpp>
#include <Concurrency/Future.hpp>
#include <Concurrency/JobSystem.hpp>
#include <Layout/LayoutTree.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Render/Resources/TextureCache.hpp>
#include <Runtime/Screen.hpp>

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
                MusicCardTheme theme{};
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

        // Cached playback snapshot for the on-card seek bar and any readout.
        // The position query takes SDL's audio-stream lock, so the screen
        // refreshes it at ~10 Hz (and immediately on a track change) instead of
        // per frame; consumers read this cache, never the live query.
        struct PlaybackInfo {
                std::string id;
                float position = 0.0f;
                float duration = 0.0f;
                bool can_seek = false;
        };
        [[nodiscard]] auto GetPlaybackInfo() const -> const PlaybackInfo&;

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

        // One-line resource telemetry, logged on track switches so a slow
        // leak/accumulation can be correlated across switches: live thread
        // count, job-system counters, texture-cache occupancy, and the process
        // working set.
        auto LogResourceTelemetry(const char* event) const -> void;

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
        [[nodiscard]] auto MarqueeOffset(float text_width, float container_width) const -> float;
        auto DrawAlbumCover(CardPainter& painter, const atom::algo::Rect& rect, float opacity) -> void;
        auto DrawControls(CardPainter& painter, float opacity, atom::render::Color accent) const -> void;
        auto DrawTriangleGlyph(CardPainter& painter, const atom::algo::Rect& box, bool points_right,
                               atom::render::Color color) const -> void;
        auto DrawPlayPauseGlyph(CardPainter& painter, atom::render::Color color) const -> void;

        // Application-level seek bar (bottom centre). Drawing stores the grab
        // hitbox; input is handled in HandleEvent so a seek only fires on
        // mouse release (debounced), never per-frame while dragging.
        auto DrawSeekBar(CardPainter& painter) -> void;
        auto RefreshPlaybackCache() -> void;
        auto UpdateSeekDragFromMouse(float mouse_x) -> void;
        auto CommitSeekDrag() -> void;

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
        auto DecodeTrackArtwork(std::size_t index, std::uint64_t generation) -> void;
        auto LoadTrackAudio(std::size_t index) -> void;
        auto DrainPendingUnloads() -> void;
        auto LoaderLoop(std::stop_token stop) -> void;

        // Controls are serialized into one per-card command stream. The
        // engine serializes each source too, but this makes the user-facing
        // transition deterministic: stop(old) completes before play(new).
        auto QueueAudioControl(std::string name, std::function<void()> action) -> void;
        auto DispatchPlayOnJobSystem(const std::string& id, std::uint64_t request) -> void;
        // Stop/Pause used to run synchronously on the render thread, where the
        // backend joins the decode thread (up to a full sleep interval). They
        // are dispatched onto the job system the same way Play() is, so no
        // audio control call can stall a frame.
        auto DispatchStopOnJobSystem(const std::string& id) -> void;
        auto DispatchPauseOnJobSystem(const std::string& id) -> void;
        auto DispatchSeekOnJobSystem(std::string id, float seconds) -> void;

        struct PendingSeek {
                std::string id;
                float seconds = 0.0f;
        };

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
        // Handles for in-flight audio jobs (music_.Load / Play / Stop / Pause)
        // so the screen can observe completion without joining anything on the
        // render thread. Guarded by audio_jobs_mutex_: jobs are pushed from both
        // the render thread (Play/Stop/Pause) and the loader thread (Load), and
        // pruned every frame by TryStartAfterLoad.
        std::vector<atom::job::JobHandle> audio_jobs_;
        mutable std::mutex audio_jobs_mutex_;
        // Cover decode jobs own no GPU state and run at low priority. They
        // have separate lifetime tracking because shutdown must wait for their
        // captures before Track storage goes away.
        std::vector<atom::job::JobHandle> artwork_jobs_;
        mutable std::mutex artwork_jobs_mutex_;
        atom::job::JobHandle audio_command_tail_{};
        // Debug sliders can emit many values per drag. Keep only the newest
        // pending value instead of replaying stale seeks after release.
        std::optional<PendingSeek> pending_seek_{};
        bool seek_command_queued_ = false;
        // Play is queued behind any previous stop.  Do not let the next frame
        // mistake the still-stopped source for an unexpected stop before that
        // queued Play command has actually run.
        bool play_start_pending_ = false;
        std::uint64_t next_play_request_ = 0;
        std::uint64_t pending_play_request_ = 0;
        std::atomic<std::uint64_t> completed_play_request_{0};
        CardCorner corner_ = CardCorner::BottomLeft;
        AnimationState animation_state_ = AnimationState::Entering;
        float animation_progress_ = 0.0f;
        float visible_time_ = 0.0f;
        float effect_time_ = 0.0f;
        // Monotonic clock driving title/artist marquee scrolling. Reset on
        // track change so each track's text starts from the pause phase.
        float scroll_time_ = 0.0f;
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
        // The wallpaper's async load (ARCH-107 P3): the decode runs on a worker,
        // and the render thread materializes the texture via PumpAsync(). The
        // NativeFileSystem that owns the wallpaper directory must outlive the
        // in-flight request, so it is held here instead of as a local.
        std::unique_ptr<atom::fs::NativeFileSystem> background_filesystem_{};
        atom::async::Future<atom::render::resources::TextureHandle> background_load_{};
        std::string background_wallpaper_path_{};
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

        // Playback cache + seek-bar interaction. All updated on the render
        // thread only, so no lock is needed.
        mutable PlaybackInfo playback_cache_{};
        float playback_query_timer_ = 1.0f;   // forces an immediate first refresh
        atom::algo::Rect seek_bar_rect_{};
        bool seek_dragging_ = false;
        float seek_drag_value_ = 0.0f;
};

#endif // ATOM_EXAMPLE_MUSIC_CARD_SCREEN_HPP
