/**
 * @file           : MusicCardScreen.cpp
 * @brief          : Implementation of the MusicCard screen declared in
 *                   MusicCardScreen.hpp. Only the entry point (MusicCard.cpp)
 *                   and this translation unit are compiled; every other header
 *                   stays include-only.
 */

#include "MusicCardScreen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

#include <Algorithm/Interpolation/Easing.hpp>
#include <Concurrency/ThreadRegistry.hpp>
#include <Debugger/FontLoader.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/AudioBackend.hpp>
#include <Media/Audio/Metadata/AudioMetadataReader.hpp>
#include <Media/Image/Analysis/ColorStatistics.hpp>
#include <Media/Image/ImageDecoder.hpp>
#include <Utilities/Utf8/Utf8.hpp>
#include <Window/RenderWindow.hpp>

#ifdef _WIN32
// Only this translation unit needs the process working-set query for the
// resource telemetry line. Kept out of the headers, mirroring LogSystem.cpp.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
// winuser.h maps DrawText to DrawTextA/W; Renderer2D has a DrawText member, so
// undo that macro the moment the Win32 headers are in.
#undef DrawText
#endif // _WIN32

namespace {

// Presentation policy: a 104px card needs no multi-megapixel embedded art.
// This also places a strict upper bound on the GPU work the card can request.
constexpr uint32_t cover_thumbnail_max_extent = 256;

// Marquee (scrolling title/artist) policy. Text that fits stays still; text
// that overflows its column pauses, then scrolls left until it clears the
// container, then snaps back and repeats.
constexpr float marquee_speed = 24.0f;  // pixels per second while scrolling
constexpr float marquee_pause = 1.4f;   // seconds shown at rest before scrolling
constexpr float marquee_gap = 40.0f;    // gap left between loop iterations

// TEMP experiment: long title/artist text (and the per-frame MeasureText +
// marquee it triggers) is the current suspect for the per-track instantaneous
// frame drops. Set to false to skip drawing title/artist entirely so the
// hypothesis can be A/B tested. Flip back to true once confirmed.
//
// Root cause confirmed: the per-frame re-layout. Renderer2D now caches text
// layout, so this is left true (text renders normally).
constexpr bool draw_card_text_experiment = true;

[[nodiscard]] auto MakeCoverThumbnail(atom::image::DecodedImage source) -> atom::image::DecodedImage {
    if (!source.IsValid() || std::max(source.width, source.height) <= cover_thumbnail_max_extent)
        return source;

    const auto longest = std::max(source.width, source.height);
    const auto scale = static_cast<float>(cover_thumbnail_max_extent) / static_cast<float>(longest);
    const auto target_width = std::max(1u, static_cast<uint32_t>(std::lround(source.width * scale)));
    const auto target_height = std::max(1u, static_cast<uint32_t>(std::lround(source.height * scale)));
    atom::image::DecodedImage thumbnail{};
    thumbnail.width = target_width;
    thumbnail.height = target_height;
    thumbnail.rgba.resize(static_cast<std::size_t>(target_width) * target_height * 4u);

    // Bilinear filtering is cheap on the worker and avoids the visibly harsh
    // nearest-neighbour result at the card's small cover size.
    const auto sample = [&](const uint32_t x, const uint32_t y, const uint32_t channel) -> uint8_t {
        return source.rgba[(static_cast<std::size_t>(y) * source.width + x) * 4u + channel];
    };
    for (uint32_t y = 0; y < target_height; ++y) {
        const float source_y = (static_cast<float>(y) + 0.5f) / scale - 0.5f;
        const auto y0 = static_cast<uint32_t>(std::clamp(static_cast<int>(std::floor(source_y)), 0,
                                                          static_cast<int>(source.height) - 1));
        const auto y1 = std::min(y0 + 1u, source.height - 1u);
        const float fy = std::clamp(source_y - std::floor(source_y), 0.0f, 1.0f);
        for (uint32_t x = 0; x < target_width; ++x) {
            const float source_x = (static_cast<float>(x) + 0.5f) / scale - 0.5f;
            const auto x0 = static_cast<uint32_t>(std::clamp(static_cast<int>(std::floor(source_x)), 0,
                                                              static_cast<int>(source.width) - 1));
            const auto x1 = std::min(x0 + 1u, source.width - 1u);
            const float fx = std::clamp(source_x - std::floor(source_x), 0.0f, 1.0f);
            for (uint32_t channel = 0; channel < 4; ++channel) {
                const float top = static_cast<float>(sample(x0, y0, channel)) * (1.0f - fx) +
                                  static_cast<float>(sample(x1, y0, channel)) * fx;
                const float bottom = static_cast<float>(sample(x0, y1, channel)) * (1.0f - fx) +
                                     static_cast<float>(sample(x1, y1, channel)) * fx;
                thumbnail.rgba[(static_cast<std::size_t>(y) * target_width + x) * 4u + channel] =
                    static_cast<uint8_t>(std::lround(top * (1.0f - fy) + bottom * fy));
            }
        }
    }
    return thumbnail;
}

} // namespace

MusicCardScreen::MusicCardScreen(atom::MusicPlayer& music, std::vector<std::string> paths,
                                 std::shared_ptr<atom::fs::NativeFileSystem> filesystem)
    : music_{music}, filesystem_{std::move(filesystem)} {
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
    // Make sure the engine job scheduler is up: metadata and audio loading are
    // both dispatched onto it so neither TagLib parsing nor decoder/stream
    // allocation ever runs on the render thread.
    auto& jobs = atom::job::JobSystem::GetInstance();
    if (!jobs.IsRunning()) {
        jobs.Configure();
        jobs.Start();
    }
    // The first track is NOT loaded synchronously here. The background loader's
    // first pass resolves current (0) ± 1, so the card simply shows a
    // "Loading..." placeholder for the first frame or two instead of stalling
    // the render thread on TagLib + decoder open. Kick off the initial prefetch.
    {
        std::lock_guard lock{tracks_mutex_};
        prefetch_requested_ = true;
    }
    loader_thread_ = std::jthread{[this](std::stop_token st) { LoaderLoop(st); }};
    // MusicCard caches "resolved" state per track, so it must know when a
    // backend switch invalidates everything it cached (see
    // OnAudioBackendChanging). MusicPlayer is registered separately and drops
    // the ids it owns.
    atom::audio::AddAudioBackendListener(*this);
    LOG_INFO(atom::log::core::Screen, "Music card initialized with " + std::to_string(tracks_.size()) +
                                          " track path(s); lazy metadata + audio loading enabled");
}

MusicCardScreen::~MusicCardScreen() {
    atom::audio::RemoveAudioBackendListener(*this);
    Shutdown();
}

auto MusicCardScreen::OnAudioBackendChanging() -> void {
    std::size_t invalidated = 0;
    {
        std::lock_guard lock{tracks_mutex_};
        ++cache_generation_;
        for (auto& track : tracks_) {
            if (track.metadata_loaded || track.is_loaded)
                ++invalidated;
            track.metadata_loaded = false;
            track.is_loaded = false;
            track.artwork_image.reset();
            track.artwork_decode_requested = false;
            ++track.artwork_revision;
        }
        prefetch_requested_ = true;
    }
    start_after_load_ = false;
    is_playing_ = false;
    play_start_pending_ = false;
    loader_cv_.notify_one();
    LOG_INFO(atom::log::audio::Music, "Audio backend changing: music card invalidated " +
                                          std::to_string(invalidated) + " cached track(s); reload will follow");
}

auto MusicCardScreen::OnAudioBackendChanged() -> void {
    {
        std::lock_guard lock{tracks_mutex_};
        prefetch_requested_ = true;
    }
    loader_cv_.notify_one();
    LOG_INFO(atom::log::audio::Music,
             "Audio backend changed (generation " + std::to_string(atom::audio::GetAudioBackendGeneration()) +
                 "); music card re-resolving tracks");
}

auto MusicCardScreen::Shutdown() -> void {
    loader_thread_.request_stop();
    loader_cv_.notify_all();
    if (loader_thread_.joinable())
        loader_thread_.join();

    // Wait for any in-flight audio jobs before tearing the screen down. Those
    // lambdas capture `this` (music_ and filesystem_), so they must be finished
    // before members are destroyed. Swap the list out under the lock so a Wait()
    // (which can run other jobs, and those jobs could push more handles) never
    // races or deadlocks against a concurrent push.
    std::vector<atom::job::JobHandle> pending_audio_jobs;
    {
        std::lock_guard lock{audio_jobs_mutex_};
        pending_audio_jobs.swap(audio_jobs_);
    }
    for (const auto& handle : pending_audio_jobs)
        handle.Wait();

    std::vector<atom::job::JobHandle> pending_artwork_jobs;
    {
        std::lock_guard lock{artwork_jobs_mutex_};
        pending_artwork_jobs.swap(artwork_jobs_);
    }
    for (const auto& handle : pending_artwork_jobs)
        handle.Wait();

    background_texture_ = {};
    background_load_ = {};
    background_filesystem_.reset();
    background_wallpaper_path_.clear();
    for (auto& handle : artwork_textures_)
        handle = {};
    std::fill(artwork_texture_attempted_.begin(), artwork_texture_attempted_.end(), false);
    interface_font_ = nullptr;
    // Releases the GPU textures whose last handle just went away. Must run
    // outside a frame, which is true on both call paths.
    texture_cache_.EvictAll();
    texture_cache_.FlushDeferredDestroys();
    renderer_.Shutdown();
}

auto MusicCardScreen::Play() -> void {
    if (tracks_.empty())
        return;
    if (!is_playing_)
        StartCurrentTrack();
    if (animation_state_ == AnimationState::Hidden)
        RestartEntrance();
}

auto MusicCardScreen::TogglePlayback() -> void {
    if (tracks_.empty())
        return;
    if (is_playing_)
        PauseCurrentTrack();
    else
        Play();
}

auto MusicCardScreen::Stop() -> void {
    StopCurrentTrack();
}

auto MusicCardScreen::Previous() -> void {
    RequestRelativeTrack(-1);
}

auto MusicCardScreen::Next() -> void {
    RequestRelativeTrack(1);
}

auto MusicCardScreen::ToggleCorner() -> void {
    corner_ = corner_ == CardCorner::BottomLeft ? CardCorner::TopRight : CardCorner::BottomLeft;
    LOG_INFO(atom::log::core::Screen, "Music card corner changed to " + std::string{GetCornerName()});
    if (animation_state_ != AnimationState::Hidden)
        RestartEntrance();
}

auto MusicCardScreen::ToggleCardVisibility() -> void {
    pending_track_ = tracks_.size();
    if (animation_state_ == AnimationState::Hidden || animation_state_ == AnimationState::Exiting)
        RestartEntrance();
    else
        animation_state_ = AnimationState::Exiting;
    LOG_DEBUG(atom::log::core::Screen,
              "Music card visibility transition requested; state=" + std::string{GetAnimationStateName()});
}

auto MusicCardScreen::GetCurrentDisplay() const -> TrackDisplay {
    std::lock_guard lock{tracks_mutex_};
    if (current_track_ >= tracks_.size())
        return {};
    const auto& t = tracks_[current_track_];
    return {t.title, t.artist, t.artwork_mime_type, t.artwork_data.size(), t.is_loaded, t.metadata_loaded, t.theme};
}

auto MusicCardScreen::HasTracks() const -> bool {
    return !tracks_.empty();
}

auto MusicCardScreen::IsPlaying() const -> bool {
    return is_playing_;
}

auto MusicCardScreen::IsCardVisible() const -> bool {
    return animation_state_ != AnimationState::Hidden && animation_state_ != AnimationState::Exiting;
}

auto MusicCardScreen::GetNowPlayingId() const -> std::string {
    return music_.GetNowPlaying();
}

auto MusicCardScreen::GetPlaybackPosition(const std::string& id) const -> float {
    return music_.GetPlayingOffset(id);
}

auto MusicCardScreen::GetPlaybackDuration(const std::string& id) const -> float {
    return music_.GetDuration(id);
}

auto MusicCardScreen::CanSeek(const std::string& id) const -> bool {
    return music_.IsSeekable(id);
}

auto MusicCardScreen::Seek(const std::string& id, const float seconds) -> bool {
    if (id.empty() || !music_.IsLoaded(id))
        return false;
    // A seek tears down and restarts the streaming source. It is a valid
    // operation, but never a render-thread operation; it enters the same
    // ordered control lane used by Play/Stop/Pause.
    DispatchSeekOnJobSystem(id, seconds);
    return true;
}

auto MusicCardScreen::GetPlaybackInfo() const -> const PlaybackInfo& {
    return playback_cache_;
}

auto MusicCardScreen::RefreshPlaybackCache() -> void {
    const auto id = music_.GetNowPlaying();
    if (id.empty()) {
        playback_cache_.id.clear();
        playback_cache_.position = 0.0f;
        playback_cache_.duration = 0.0f;
        playback_cache_.can_seek = false;
        return;
    }
    playback_cache_.id = id;
    playback_cache_.duration = music_.GetDuration(id);
    playback_cache_.position = music_.GetPlayingOffset(id);
    playback_cache_.can_seek = music_.IsSeekable(id);
}

auto MusicCardScreen::UpdateSeekDragFromMouse(const float mouse_x) -> void {
    if (seek_bar_rect_.width <= 0.0f || playback_cache_.duration <= 0.0f)
        return;
    const float fraction =
        std::clamp((mouse_x - seek_bar_rect_.x) / seek_bar_rect_.width, 0.0f, 1.0f);
    seek_drag_value_ = fraction * playback_cache_.duration;
}

auto MusicCardScreen::CommitSeekDrag() -> void {
    if (!playback_cache_.can_seek || playback_cache_.id.empty())
        return;
    (void)Seek(playback_cache_.id, seek_drag_value_);
    // Reflect the new position promptly instead of waiting out the 10 Hz cycle.
    playback_query_timer_ = 1.0f;
}

auto MusicCardScreen::LoadInterfaceFont() -> bool {
#ifdef _WIN32
    constexpr std::array font_candidates{
        "C:/Windows/Fonts/msyh.ttc",    "C:/Windows/Fonts/msyhbd.ttc",   "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",  "C:/Windows/Fonts/msgothic.ttc", "C:/Windows/Fonts/meiryo.ttc",
        "C:/Windows/Fonts/meiryob.ttc", "C:/Windows/Fonts/yugothib.ttf", "C:/Windows/Fonts/yugothic.ttf",
        "C:/Windows/Fonts/malgun.ttf",
    };
#else
    constexpr std::array font_candidates{
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    };
#endif // _WIN32
    for (const auto* font_path : font_candidates) {
        if (!std::filesystem::exists(font_path))
            continue;
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

        const auto* debug_font = atom::debugger::FontLoader::LoadFromFile(
            font_path, {.size_pixels = 18.0f,
                        .glyph_preset = atom::debugger::GlyphPreset::ChineseFull,
                        .set_as_default = true});
        if (!debug_font)
            LOG_WARNING(atom::log::debugger::ImGui, "CJK font loaded for Renderer2D, but not for the ImGui debugger");
        LOG_INFO(atom::log::core::Screen, "Music card loaded interface font: " + std::string{font_path});
        return true;
    }
    LOG_WARNING(atom::log::core::Screen, "No suitable CJK interface font was loaded; on-card text will be hidden");
    return false;
}

auto MusicCardScreen::GetCornerName() const -> std::string_view {
    return corner_ == CardCorner::BottomLeft ? "Bottom left" : "Top right";
}

auto MusicCardScreen::GetAnimationStateName() const -> std::string_view {
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

auto MusicCardScreen::Render(atom::render::IRenderDevice& device) -> void {
    if (!renderer_.IsInitialized() && !renderer_.Initialize(device, ATOM_SHADER_OUTPUT_DIR)) {
        if (!renderer_initialization_failed_) {
            LOG_ERROR(atom::log::core::Screen, "Music card: Renderer2D initialization failed");
            renderer_initialization_failed_ = true;
        }
        return;
    }
    if (!interface_font_attempted_ && !interface_font_data_.empty()) {
        interface_font_attempted_ = true;
        interface_font_ = renderer_.LoadFontFromMemory(interface_font_data_);
        if (!interface_font_)
            LOG_ERROR(atom::log::core::Screen, "Music card: Renderer2D rejected the selected interface font");
        // Renderer2D copies the face into its own Font (about 20 MB for
        // msyh.ttc), so this screen's copy is dead weight from here on.
        interface_font_data_.clear();
        interface_font_data_.shrink_to_fit();
    }
    EnsureBackgroundTexture();
    // Finalize any in-flight async texture decode (materialize on the render
    // thread) and reclaim GPU textures whose last handle was dropped since the
    // previous frame. Both must run outside BeginFrame/EndFrame.
    texture_cache_.PumpAsync();
    texture_cache_.FlushDeferredDestroys();
    device.Clear(atom::render::Color{8, 10, 18, 255});
    const auto size = device.GetOutputSize();
    window_width_ = size.GetX();
    window_height_ = size.GetY();
    if (window_width_ <= 0.0f || window_height_ <= 0.0f)
        return;
    UpdateLayout();
    UpdateCardPostProcessRegion();
    if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f))
        return;
    auto painter = CardPainter{renderer_};
    DrawBackground(painter);
    renderer_.SetPostProcess({});
    if (!renderer_.EndFrame() && !renderer_frame_failed_) {
        LOG_ERROR(atom::log::core::Screen, "Music card: background frame submission failed");
        renderer_frame_failed_ = true;
    }

    // Render the wallpaper into the reusable offscreen target and resolve only
    // the popup region through a Gaussian blur (the glass panel backdrop).
    if (animation_state_ != AnimationState::Hidden && current_card_rect_.width > 0.0f &&
        current_card_rect_.height > 0.0f) {
        if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f))
            return;
        DrawBackground(painter);
        atom::render::Renderer2D::PostProcessParams blur{};
        blur.effect = atom::render::PostProcess2DEffect::GaussianBlur;
        blur.has_region = true;
        blur.region = current_card_bounds_;
        blur.corner_radius = 24.0f;
        blur.feather = 8.0f;
        blur.amount = 16.0f;
        renderer_.SetPostProcess(blur);
        if (!renderer_.EndFrame() && !renderer_frame_failed_) {
            LOG_ERROR(atom::log::core::Screen, "Music card: backdrop blur submission failed");
            renderer_frame_failed_ = true;
        }
    }

    // Render only the popup into the post-process target.
    if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f))
        return;
    DrawCard(painter, false);
    atom::render::Renderer2D::PostProcessParams postprocess{};
    postprocess.time = effect_time_;
    postprocess.has_region = true;
    postprocess.region = current_card_rect_;
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
        LOG_ERROR(atom::log::core::Screen, "Music card: Renderer2D frame submission failed");
        renderer_frame_failed_ = true;
    }
    // Text and the bottom-centre seek bar are submitted after the card
    // post-process so they sit above it. The bar is application UI (not a
    // debugger control) and stays visible while a track exists.
    const bool needs_text = animation_state_ != AnimationState::Hidden;
    if (needs_text || current_track_ < tracks_.size()) {
        if (!renderer_.BeginFrame(0.0f, 0.0f, 1.0f))
            return;
        if (needs_text)
            DrawCardText();
        DrawSeekBar(painter);
        renderer_.SetPostProcess({});
        if (!renderer_.EndFrame() && !renderer_frame_failed_) {
            LOG_ERROR(atom::log::core::Screen, "Music card: overlay frame submission failed");
            renderer_frame_failed_ = true;
        }
    }
}

// INPUT: this screen stays on the event path on purpose. FindAction() matches
// the event's modifier snapshot, so {Space, None} fires for Space alone and not
// for Shift+Space. The polling path (IsActionPressed) checks the modifiers held
// at read time instead, and is for continuous input: see
// Example/Input/InputDemo.cpp.
auto MusicCardScreen::HandleEvent(const atom::window::IEvent& event) -> bool {
    if (event.type == atom::window::EventType::KeyPressed) {
        const auto& key = std::get<atom::window::KeyEvent>(event.data);
        const auto action = GetMusicCardBindings().FindAction(key);
        if (!action)
            return false;
        switch (*action) {
        case MusicCardAction::Quit:
            atom::RenderWindow::GetInstance().RequestClose();
            return true;
        case MusicCardAction::Previous:
            Previous();
            return true;
        case MusicCardAction::Next:
            Next();
            return true;
        case MusicCardAction::TogglePlayback:
            TogglePlayback();
            return true;
        case MusicCardAction::ToggleCorner:
            ToggleCorner();
            return true;
        case MusicCardAction::ToggleVisibility:
            ToggleCardVisibility();
            return true;
        }
    }

    if (event.type == atom::window::EventType::MouseButtonPressed) {
        const auto& mouse = std::get<atom::window::MouseEvent>(event.data);
        if (mouse.button != atom::input::MouseButton::Left)
            return false;
        if (seek_bar_rect_.Contains(mouse.x, mouse.y)) {
            seek_dragging_ = true;
            UpdateSeekDragFromMouse(mouse.x);
            return true;
        }
        if (previous_hitbox_.Contains(mouse.x, mouse.y)) {
            Previous();
            return true;
        }
        if (play_hitbox_.Contains(mouse.x, mouse.y)) {
            TogglePlayback();
            return true;
        }
        if (next_hitbox_.Contains(mouse.x, mouse.y)) {
            Next();
            return true;
        }
    }

    if (event.type == atom::window::EventType::MouseButtonReleased) {
        const auto& mouse = std::get<atom::window::MouseEvent>(event.data);
        if (mouse.button != atom::input::MouseButton::Left || !seek_dragging_)
            return false;
        seek_dragging_ = false;
        // Debounced: a seek is dispatched only here, on release — never while
        // the user is still dragging the bar.
        CommitSeekDrag();
        return true;
    }

    if (event.type == atom::window::EventType::MouseMoved) {
        if (!seek_dragging_)
            return false;
        const auto& mouse = std::get<atom::window::MouseEvent>(event.data);
        UpdateSeekDragFromMouse(mouse.x);
        return true;
    }

    return false;
}

auto MusicCardScreen::Update(const float delta_time) -> void {
    effect_time_ += std::max(0.0f, delta_time);
    scroll_time_ += std::max(0.0f, delta_time);
    HandlePlaybackCompletion();
    // Throttled playback cache refresh (~10 Hz). The position query takes
    // SDL's audio-stream lock, so it must not run every frame.
    playback_query_timer_ += std::max(0.0f, delta_time);
    if (playback_query_timer_ >= 0.1f) {
        playback_query_timer_ = 0.0f;
        RefreshPlaybackCache();
    }
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
        if (animation_progress_ <= 0.0f)
            CompleteExit();
        break;
    case AnimationState::Hidden:
        break;
    }
    MaintainResidency();
}

auto MusicCardScreen::IsWithinResidencyWindow(const std::size_t index, const std::size_t center,
                                              const std::size_t count) -> bool {
    if (count == 0)
        return false;
    const auto forward = (index + count - center) % count;
    const auto backward = (center + count - index) % count;
    return std::min(forward, backward) <= residency_window;
}

auto MusicCardScreen::MaintainResidency() -> void {
    const auto count = tracks_.size();
    if (count == 0)
        return;
    const auto center = pending_track_ < count ? pending_track_ : current_track_;
    {
        std::lock_guard lock{tracks_mutex_};
        if (center == resident_center_)
            return;
        resident_center_ = center;
    }

    auto released = std::size_t{0};
    for (auto index = std::size_t{0}; index < count; ++index) {
        if (IsWithinResidencyWindow(index, center, count))
            continue;
        if (artwork_textures_[index]) {
            artwork_textures_[index] = {};
            artwork_texture_attempted_[index] = false;
        }
        std::string id;
        {
            std::lock_guard lock{tracks_mutex_};
            auto& track = tracks_[index];
            if (!track.metadata_loaded && !track.is_loaded)
                continue;
            id = track.id;
            track.is_loaded = false;
            track.metadata_loaded = false;
            track.title.clear();
            track.artist.clear();
            track.artwork_mime_type.clear();
            track.artwork_data.clear();
            track.artwork_data.shrink_to_fit();
            track.artwork_image.reset();
            track.artwork_decode_requested = false;
            ++track.artwork_revision;
        }
        {
            std::lock_guard lock{tracks_mutex_};
            pending_unloads_.push_back(std::move(id));
        }
        ++released;
    }
    if (released > 0) {
        loader_cv_.notify_one();
        LOG_DEBUG(atom::log::core::Screen,
                  "Music card residency window moved to " + std::to_string(center) + ": queued " +
                      std::to_string(released) + " track(s) for background release");
    }
}

auto MusicCardScreen::IsResidentLocked(const std::size_t index) const -> bool {
    return IsWithinResidencyWindow(index, resident_center_, tracks_.size());
}

auto MusicCardScreen::LogResourceTelemetry(const char* event) const -> void {
    const auto threads = atom::concurrency::ThreadRegistry::GetInstance().LiveCount();
    const auto& jobs = atom::job::JobSystem::GetInstance();
    const auto stats = jobs.Stats();
    const auto live_textures = texture_cache_.LiveCount();
    const auto cache_capacity = texture_cache_.CacheCapacity();

    std::string memory = "n/a";
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        memory = std::to_string(counters.WorkingSetSize / (1024 * 1024)) + " MB";
    }
#endif

    LOG_INFO(atom::log::core::Screen,
             std::string{"[telemetry] "} + event +
                 " threads=" + std::to_string(threads) +
                 " jobs(pending=" + std::to_string(stats.pending) + ", total=" + std::to_string(stats.submitted) +
                 ")" +
                 " textures(live=" + std::to_string(live_textures) + ", cap=" + std::to_string(cache_capacity) + ")" +
                 " ws=" + memory);
}

auto MusicCardScreen::EnsureBackgroundTexture() -> void {
    if (background_texture_ || !renderer_.IsInitialized())
        return;

    // A load is already in flight: poll it and materialize the result on the
    // render thread. The decode itself ran on a worker (via LoadAsync), so this
    // frame never blocks on stb decoding.
    if (background_load_) {
        if (!background_load_.IsReady())
            return;
        background_texture_attempted_ = true;
        const auto result = background_load_.Get();
        if (result.has_value())
            background_texture_ = result.value();
        if (!background_texture_) {
            LOG_ERROR(atom::log::core::Screen,
                      "Music card failed to upload wallpaper texture: " + background_wallpaper_path_);
        } else {
            LOG_INFO(atom::log::core::Screen,
                     "Music card wallpaper loaded (" + std::to_string(background_texture_.GetWidth()) + "x" +
                         std::to_string(background_texture_.GetHeight()) + "): " + background_wallpaper_path_);
        }
        return;
    }

    if (background_texture_attempted_)
        return;
    background_texture_attempted_ = true;

    const char* override_path = std::getenv("ATOM_MUSIC_CARD_WALLPAPER");
    background_wallpaper_path_ = override_path && *override_path ? override_path : WallpaperPath;

    const auto native = atom::PathFromUtf8(background_wallpaper_path_);
    const auto parent = native.parent_path();
    const auto filename = native.filename();
    if (parent.empty() || filename.empty()) {
        LOG_WARNING(atom::log::core::Screen,
                    "Music card wallpaper path is not a file path; using procedural background: " +
                        background_wallpaper_path_);
        return;
    }
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(parent), background_filesystem_) !=
            atom::fs::Result::Success ||
        !background_filesystem_) {
        LOG_WARNING(atom::log::core::Screen,
                    "Music card wallpaper directory unavailable; using procedural background: " +
                        background_wallpaper_path_);
        return;
    }
    const auto asset = atom::fs::AssetPath::Parse("res://" + atom::PathToUtf8(filename));
    if (!asset) {
        LOG_WARNING(atom::log::core::Screen,
                    "Music card wallpaper filename is not a valid asset path ('" + atom::PathToUtf8(filename) +
                        "'): " + asset.error().Message());
        return;
    }
    // Decode off the render thread; the texture is materialized by PumpAsync()
    // above once the worker finishes.
    background_load_ = texture_cache_.LoadAsync(*background_filesystem_, *asset);
}

auto MusicCardScreen::BuildLayoutTree() -> void {
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

auto MusicCardScreen::UpdateLayout() -> void {
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
        LOG_DEBUG(atom::log::core::Screen,
                  "Music card layout resolved: viewport=" + std::to_string(window_width_) + "x" +
                      std::to_string(window_height_) + ", card=" + std::to_string(resolved.left) + "," +
                      std::to_string(resolved.top) + " " + std::to_string(resolved.width) + "x" +
                      std::to_string(resolved.height));
        reported_layout_width_ = window_width_;
        reported_layout_height_ = window_height_;
    }
}

auto MusicCardScreen::GlobalRect(const atom::layout::LayoutTree::NodeId node, const atom::algo::Rect& parent) const
    -> atom::algo::Rect {
    const auto layout = layout_tree_.GetLayout(node).value_or(atom::layout::Rect{});
    return {parent.x + layout.left, parent.y + layout.top, layout.width, layout.height};
}

auto MusicCardScreen::DrawBackground(CardPainter& painter) -> void {
    if (background_texture_) {
        const auto texture_width = static_cast<float>(background_texture_.GetWidth());
        const auto texture_height = static_cast<float>(background_texture_.GetHeight());
        const auto window_aspect = window_width_ / std::max(window_height_, 1.0f);
        const auto texture_aspect = texture_width / std::max(texture_height, 1.0f);
        atom::algo::Rect source{0.0f, 0.0f, texture_width, texture_height};
        if (texture_aspect > window_aspect) {
            source.width = texture_height * window_aspect;
            source.x = (texture_width - source.width) * 0.5f;
        } else {
            source.height = texture_width / window_aspect;
            source.y = (texture_height - source.height) * 0.5f;
        }
        const auto source_rect = source;
        renderer_.DrawTexture(*background_texture_.Get(), {0.0f, 0.0f, window_width_, window_height_},
                              atom::render::Color{255, 255, 255, 255}, &source_rect);
        painter.FillRect({0.0f, 0.0f, window_width_, window_height_}, atom::render::Color{8, 10, 18, 28});
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
        painter.FillRect({0.0f, band_height * static_cast<float>(index), window_width_, band_height + 1.0f}, color);
    }

    painter.FillCircle(window_width_ * 0.72f, window_height_ * 0.30f, 150.0f, atom::render::Color{43, 36, 84, 34});
    painter.FillCircle(window_width_ * 0.23f, window_height_ * 0.68f, 110.0f, atom::render::Color{21, 87, 96, 25});
}

auto MusicCardScreen::ComputeAnimatedCardRect() const -> atom::algo::Rect {
    const auto layout = layout_tree_.GetLayout(card_).value_or(atom::layout::Rect{});
    const auto final_card = atom::algo::Rect{layout.left, layout.top, layout.width, layout.height};
    const auto direction = corner_ == CardCorner::BottomLeft ? -1.0f : 1.0f;
    const auto panel_progress =
        atom::algo::easing::OutCubic(atom::algo::easing::IntervalProgress(animation_progress_, 0.0f, 0.62f));
    return final_card.Translated(direction * (1.0f - panel_progress) * 86.0f, 0.0f);
}

auto MusicCardScreen::UpdateCardPostProcessRegion() -> void {
    if (animation_state_ == AnimationState::Hidden || tracks_.empty()) {
        current_card_bounds_ = {};
        current_card_rect_ = {};
        return;
    }
    const auto card = ComputeAnimatedCardRect();
    current_card_bounds_ = card;
    current_card_rect_ = {card.x - 16.0f, card.y - 16.0f, card.width + 32.0f, card.height + 32.0f};
}

auto MusicCardScreen::DrawCard(CardPainter& painter, const bool draw_text) -> void {
    previous_hitbox_ = {};
    play_hitbox_ = {};
    next_hitbox_ = {};
    if (animation_state_ == AnimationState::Hidden || tracks_.empty())
        return;

    const auto card_layout = layout_tree_.GetLayout(card_).value_or(atom::layout::Rect{});
    const auto final_card = atom::algo::Rect{card_layout.left, card_layout.top, card_layout.width, card_layout.height};
    const auto direction = corner_ == CardCorner::BottomLeft ? -1.0f : 1.0f;
    const auto panel_progress =
        atom::algo::easing::OutCubic(atom::algo::easing::IntervalProgress(animation_progress_, 0.0f, 0.62f));
    const auto cover_progress =
        atom::algo::easing::OutBack(atom::algo::easing::IntervalProgress(animation_progress_, 0.10f, 0.72f));
    const auto title_progress =
        atom::algo::easing::OutCubic(atom::algo::easing::IntervalProgress(animation_progress_, 0.28f, 0.78f));
    const auto author_progress =
        atom::algo::easing::OutCubic(atom::algo::easing::IntervalProgress(animation_progress_, 0.38f, 0.86f));
    const auto controls_progress =
        atom::algo::easing::OutBack(atom::algo::easing::IntervalProgress(animation_progress_, 0.48f, 1.0f));
    const auto exit_softening = animation_state_ == AnimationState::Exiting
                                    ? 1.0f - 0.12f * atom::algo::easing::InCubic(1.0f - animation_progress_)
                                    : 1.0f;

    const auto card = final_card.Translated(direction * (1.0f - panel_progress) * 86.0f, 0.0f);
    const auto panel_opacity = panel_progress * exit_softening;
    for (auto layer = 3; layer >= 1; --layer) {
        const auto offset = static_cast<float>(layer) * 4.0f;
        painter.FillRoundedRect(card.Translated(0.0f, offset), 24.0f,
                                atom::render::Color{0, 0, 0, static_cast<uint8_t>(18 * layer * panel_opacity)});
    }
    painter.FillRoundedRect(card, 24.0f,
                            atom::color::ApplyOpacity(atom::render::Color{38, 46, 78, 78}, panel_opacity));
    painter.FillRoundedRect({card.x + 1.0f, card.y + 1.0f, card.width - 2.0f, 1.0f}, 1.0f,
                            atom::color::ApplyOpacity(atom::render::Color{255, 255, 255, 38}, panel_opacity));

    const auto cover_final = GlobalRect(cover_, card);
    const auto cover_offset = direction * (1.0f - cover_progress) * 54.0f;
    const auto cover_rect = cover_final.Translated(cover_offset, 0.0f).ScaledAboutCenter(0.76f + cover_progress * 0.24f);
    DrawAlbumCover(painter, cover_rect, cover_progress * panel_opacity);

    const auto details_final = GlobalRect(details_, card);
    const auto title_final = GlobalRect(title_, details_final);
    const auto author_final = GlobalRect(author_, details_final);
    const auto controls_final = GlobalRect(controls_, details_final);
    const auto title_x = title_final.x + direction * (1.0f - title_progress) * 28.0f;
    const auto author_x = author_final.x + direction * (1.0f - author_progress) * 34.0f;
    const auto display = GetCurrentDisplay();
    const auto& theme = display.theme;
    const auto title_text = display.metadata_loaded ? display.title : std::string{"Loading..."};
    const auto artist_text = display.metadata_loaded ? display.artist : std::string{"resolving metadata"};

    if (draw_text) {
        DrawInterfaceText({title_x, title_final.y, title_final.width, title_final.height}, title_text, 21.0f,
                          atom::color::Blend(theme.accent, atom::render::Color::White(), 0.58f));
        DrawInterfaceText({author_x, author_final.y, author_final.width, author_final.height}, artist_text, 14.0f,
                          atom::color::Blend(theme.secondary, atom::render::Color::White(), 0.38f));
    }

    const auto previous_final = GlobalRect(previous_button_, controls_final);
    const auto play_final = GlobalRect(play_button_, controls_final);
    const auto next_final = GlobalRect(next_button_, controls_final);
    const auto controls_y = (1.0f - controls_progress) * 18.0f;
    previous_hitbox_ = previous_final.Translated(0.0f, controls_y);
    play_hitbox_ = play_final.Translated(0.0f, controls_y);
    next_hitbox_ = next_final.Translated(0.0f, controls_y);
    DrawControls(painter, controls_progress * panel_opacity, theme.accent);
}

auto MusicCardScreen::DrawCardText() -> void {
    if (animation_state_ == AnimationState::Hidden || tracks_.empty() || interface_font_ == nullptr)
        return;
    // TEMP experiment: title/artist are skipped while this is false, so a
    // build with it off isolates the long-text/marquee cost from everything
    // else on the card.
    if (!draw_card_text_experiment)
        return;
    const auto card_layout = layout_tree_.GetLayout(card_).value_or(atom::layout::Rect{});
    const auto final_card = atom::algo::Rect{card_layout.left, card_layout.top, card_layout.width, card_layout.height};
    const auto direction = corner_ == CardCorner::BottomLeft ? -1.0f : 1.0f;
    const auto title_progress =
        atom::algo::easing::OutCubic(atom::algo::easing::IntervalProgress(animation_progress_, 0.28f, 0.78f));
    const auto author_progress =
        atom::algo::easing::OutCubic(atom::algo::easing::IntervalProgress(animation_progress_, 0.38f, 0.86f));
    const auto panel_progress =
        atom::algo::easing::OutCubic(atom::algo::easing::IntervalProgress(animation_progress_, 0.0f, 0.62f));
    const auto card = final_card.Translated(direction * (1.0f - panel_progress) * 86.0f, 0.0f);
    const auto details_final = GlobalRect(details_, card);
    const auto title_final = GlobalRect(title_, details_final);
    const auto author_final = GlobalRect(author_, details_final);
    const auto title_x = title_final.x + direction * (1.0f - title_progress) * 28.0f;
    const auto author_x = author_final.x + direction * (1.0f - author_progress) * 34.0f;
    const auto display = GetCurrentDisplay();
    const auto& theme = display.theme;
    const auto title_text = display.metadata_loaded ? display.title : std::string{"Loading..."};
    const auto artist_text = display.metadata_loaded ? display.artist : std::string{"resolving metadata"};
    DrawInterfaceText({title_x, title_final.y, title_final.width, title_final.height}, title_text, 21.0f,
                      atom::color::Blend(theme.accent, atom::render::Color::White(), 0.58f));
    DrawInterfaceText({author_x, author_final.y, author_final.width, author_final.height}, artist_text, 14.0f,
                      atom::color::Blend(theme.secondary, atom::render::Color::White(), 0.38f));
}

auto MusicCardScreen::DrawInterfaceText(const atom::algo::Rect& rect, const std::string& text, const float font_size,
                                        atom::render::Color color) -> void {
    if (interface_font_ == nullptr || text.empty())
        return;
    color.a = 255;
    auto shadow = atom::render::Color::Black();
    shadow.a = 210;

    const auto draw_shadowed = [&](const float x, const float y, const float max_width) {
        renderer_.DrawText(*interface_font_, text, x + 1.2f, y + 1.2f, shadow, font_size, max_width);
        renderer_.DrawText(*interface_font_, text, x, y, color, font_size, max_width);
    };

    // Measure with wrapping disabled to get the single-line advance. Text that
    // fits stays left-aligned; text that overflows scrolls.
    const auto measured = renderer_.MeasureText(*interface_font_, text, font_size, 0.0f);
    if (measured.width <= rect.width) {
        draw_shadowed(rect.x, rect.y, rect.width);
        return;
    }

    // The clip is widened internally for glyph side bearings (see Renderer2D),
    // so the first/last glyph survives while the run scrolls through the edge.
    const float offset = MarqueeOffset(measured.width, rect.width);
    renderer_.PushClip(rect);
    draw_shadowed(rect.x - offset, rect.y, 0.0f);
    renderer_.PopClip();
}

auto MusicCardScreen::MarqueeOffset(const float text_width, const float container_width) const -> float {
    const float scroll_distance = text_width + marquee_gap;
    const float scroll_duration = scroll_distance / marquee_speed;
    const float period = marquee_pause + scroll_duration;
    const float phase = std::fmod(scroll_time_, period);
    if (phase < marquee_pause)
        return 0.0f;
    return (phase - marquee_pause) * marquee_speed;
}

auto MusicCardScreen::DrawAlbumCover(CardPainter& painter, const atom::algo::Rect& rect, const float opacity) -> void {
    MusicCardTheme theme{};
    std::shared_ptr<const atom::image::DecodedImage> artwork;
    {
        std::lock_guard lock{tracks_mutex_};
        if (current_track_ >= tracks_.size())
            return;
        theme = tracks_[current_track_].theme;
        if (!artwork_texture_attempted_[current_track_] && tracks_[current_track_].artwork_image) {
            artwork_texture_attempted_[current_track_] = true;
            artwork = tracks_[current_track_].artwork_image;
        }
    }
    if (artwork) {
        artwork_textures_[current_track_] =
            LoadArtworkTexture(texture_cache_, "artwork:" + std::to_string(current_track_), *artwork);
    }
    if (artwork_textures_[current_track_]) {
        const auto image_rect = atom::algo::Rect{rect.x + 2.0f, rect.y + 2.0f, rect.width - 4.0f, rect.height - 4.0f};
        painter.DrawTexture(*artwork_textures_[current_track_].Get(), image_rect, opacity);
        painter.FillRoundedRect({rect.x, rect.y, rect.width, 2.0f}, 1.0f,
                                atom::color::ApplyOpacity(atom::render::Color{255, 255, 255, 70}, opacity));
        return;
    }

    constexpr auto stripe_count = 7;
    const auto stripe_height = rect.height / static_cast<float>(stripe_count);
    for (auto stripe = 0; stripe < stripe_count; ++stripe) {
        const auto stripe_opacity = stripe % 2 == 0 ? 0.22f : 0.10f;
        painter.FillRect({rect.x, rect.y + stripe_height * static_cast<float>(stripe), rect.width, stripe_height + 1.0f},
                         atom::color::ApplyOpacity(theme.secondary, opacity * stripe_opacity));
    }

    painter.FillCircle(rect.x + rect.width * 0.68f, rect.y + rect.height * 0.34f, rect.width * 0.31f,
                       atom::color::ApplyOpacity(theme.accent, opacity * 0.78f));
    painter.FillCircle(rect.x + rect.width * 0.30f, rect.y + rect.height * 0.73f, rect.width * 0.23f,
                       atom::color::ApplyOpacity(atom::render::Color{255, 255, 255, 110}, opacity));
    painter.FillCircle(rect.x + rect.width * 0.68f, rect.y + rect.height * 0.34f, rect.width * 0.065f,
                       atom::color::ApplyOpacity(atom::render::Color{24, 27, 39, 235}, opacity));
}

auto MusicCardScreen::DrawControls(CardPainter& painter, const float opacity, const atom::render::Color accent) const
    -> void {
    const auto secondary = atom::color::ApplyOpacity(atom::render::Color{189, 195, 214, 255}, opacity);
    const auto primary = atom::color::ApplyOpacity(accent, opacity);
    painter.FillCircle(play_hitbox_.x + play_hitbox_.width * 0.5f, play_hitbox_.y + play_hitbox_.height * 0.5f,
                       play_hitbox_.width * 0.5f, primary);

    const auto button_alpha = atom::color::ApplyOpacity(atom::render::Color{255, 255, 255, 255}, opacity);
    DrawTriangleGlyph(painter, previous_hitbox_, false, secondary);
    DrawTriangleGlyph(painter, next_hitbox_, true, secondary);
    DrawPlayPauseGlyph(painter, button_alpha);
}

auto MusicCardScreen::DrawTriangleGlyph(CardPainter& painter, const atom::algo::Rect& box, const bool points_right,
                                        const atom::render::Color color) const -> void {
    const float cx = box.x + box.width * 0.5f;
    const float cy = box.y + box.height * 0.5f;
    const float r = std::min(box.width, box.height) * 0.24f;
    const float tip_x = points_right ? cx + r : cx - r;
    const float base_x = points_right ? cx - r : cx + r;
    const float y0 = cy - r;
    const float y1 = cy + r;
    painter.DrawLine(tip_x, cy, base_x, y0, color, 2.0f);
    painter.DrawLine(base_x, y0, base_x, y1, color, 2.0f);
    painter.DrawLine(base_x, y1, tip_x, cy, color, 2.0f);
}

auto MusicCardScreen::DrawPlayPauseGlyph(CardPainter& painter, const atom::render::Color color) const -> void {
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
        painter.DrawLine(tip_x, cy, base_x, y0, color, 2.0f);
        painter.DrawLine(base_x, y0, base_x, y1, color, 2.0f);
        painter.DrawLine(base_x, y1, tip_x, cy, color, 2.0f);
    }
}

auto MusicCardScreen::DrawSeekBar(CardPainter& painter) -> void {
    if (tracks_.empty() || window_width_ <= 0.0f || window_height_ <= 0.0f)
        return;

    const float bar_width = std::min(window_width_ * 0.34f, 480.0f);
    if (bar_width < 60.0f)
        return;
    constexpr float bar_height = 4.0f;
    constexpr float bottom_margin = 28.0f;
    const float x = (window_width_ - bar_width) * 0.5f;
    const float y = window_height_ - bottom_margin - bar_height;

    const float duration = playback_cache_.duration;
    // While dragging, show the grab position rather than the cached playback
    // position (which only advances at the 10 Hz refresh rate).
    const float position = seek_dragging_ ? seek_drag_value_ : playback_cache_.position;
    const float fraction = duration > 0.0f ? std::clamp(position / duration, 0.0f, 1.0f) : 0.0f;
    const bool active = playback_cache_.can_seek && !playback_cache_.id.empty();

    // A tall, invisible grab hitbox around the thin visible bar so it is easy
    // to grab; stored for HandleEvent's press/move/release handling.
    constexpr float hitbox_height = 18.0f;
    seek_bar_rect_ = {x, y - (hitbox_height - bar_height) * 0.5f, bar_width, hitbox_height};

    painter.FillRoundedRect({x, y, bar_width, bar_height}, bar_height * 0.5f,
                            atom::render::Color{255, 255, 255, active ? uint8_t{40} : uint8_t{24}});

    const float fill_width = bar_width * fraction;
    if (fill_width > 0.0f) {
        painter.FillRoundedRect({x, y, fill_width, bar_height}, bar_height * 0.5f,
                                atom::render::Color{255, 255, 255, active ? uint8_t{210} : uint8_t{90}});
    }

    painter.FillCircle(x + fill_width, y + bar_height * 0.5f, seek_dragging_ ? 7.0f : 4.5f,
                       atom::render::Color{255, 255, 255, active ? uint8_t{235} : uint8_t{120}});
}

auto MusicCardScreen::HasLoadedTracks() const -> bool {
    std::lock_guard lock{tracks_mutex_};
    return std::ranges::any_of(tracks_, [](const Track& track) { return track.is_loaded; });
}

auto MusicCardScreen::RequestRelativeTrack(const int offset) -> void {
    if (tracks_.empty())
        return;
    const auto count = static_cast<int>(tracks_.size());
    const auto requested_track = static_cast<std::size_t>((static_cast<int>(current_track_) + offset + count) % count);
    {
        std::lock_guard lock{tracks_mutex_};
        pending_track_ = requested_track;
    }
    glitch_direction_ = offset >= 0 ? 1.0f : -1.0f;
    LOG_DEBUG(atom::log::core::Screen,
              "Music card track transition requested: " + std::to_string(current_track_) + " -> " +
                  std::to_string(pending_track_));
    {
        std::lock_guard lock{tracks_mutex_};
        prefetch_requested_ = true;
    }
    loader_cv_.notify_one();
    if (animation_state_ == AnimationState::Hidden)
        CompleteExit();
    else
        animation_state_ = AnimationState::Exiting;
}

auto MusicCardScreen::CompleteExit() -> void {
    if (pending_track_ < tracks_.size()) {
        StopCurrentTrack();
        {
            std::lock_guard lock{tracks_mutex_};
            current_track_ = pending_track_;
            pending_track_ = tracks_.size();
        }
        start_after_load_ = true;
        RestartEntrance();
        LogResourceTelemetry("track-switch");
        return;
    }
    animation_state_ = AnimationState::Hidden;
}

auto MusicCardScreen::RestartEntrance() -> void {
    animation_progress_ = 0.0f;
    visible_time_ = 0.0f;
    scroll_time_ = 0.0f;
    animation_state_ = AnimationState::Entering;
}

auto MusicCardScreen::TryStartAfterLoad() -> void {
    // Prune completed audio-job handles so audio_jobs_ does not grow unbounded
    // across many track switches. Guarded because the loader thread pushes Load
    // handles concurrently.
    {
        std::lock_guard lock{audio_jobs_mutex_};
        std::erase_if(audio_jobs_, [](const atom::job::JobHandle& h) { return h.IsComplete(); });
    }
    {
        std::lock_guard lock{artwork_jobs_mutex_};
        std::erase_if(artwork_jobs_, [](const atom::job::JobHandle& h) { return h.IsComplete(); });
    }

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
        LOG_DEBUG(atom::log::core::Screen,
                  "Music card deferred track start completed without blocking the render thread");
    }
}

auto MusicCardScreen::HandlePlaybackCompletion() -> void {
    if (!is_playing_ || current_track_ >= tracks_.size())
        return;
    std::string id;
    std::string title;
    {
        std::lock_guard lock{tracks_mutex_};
        id = tracks_[current_track_].id;
        title = tracks_[current_track_].title;
    }
    const auto playback_state = music_.GetState(id);
    if (play_start_pending_) {
        if (completed_play_request_.load(std::memory_order_acquire) != pending_play_request_)
            return;
        play_start_pending_ = false;
    }
    if (playback_state == atom::audio::AudioSourceState::Playing ||
        playback_state == atom::audio::AudioSourceState::Paused)
        return;

    const auto finished = music_.IsFinished(id);
    is_playing_ = false;
    if (finished) {
        LOG_INFO(atom::log::audio::Music, "Music card reached EOF; advancing from track: " + title);
        RequestRelativeTrack(1);
    } else {
        LOG_WARNING(atom::log::audio::Music, "Music card playback stopped before EOF: " + title);
    }
}

auto MusicCardScreen::PauseCurrentTrack() -> void {
    std::string id;
    {
        std::lock_guard lock{tracks_mutex_};
        if (current_track_ >= tracks_.size())
            return;
        id = tracks_[current_track_].id;
    }
    DispatchPauseOnJobSystem(id);
    play_start_pending_ = false;
    is_playing_ = false;
}

auto MusicCardScreen::StartCurrentTrack() -> void {
    std::string id;
    std::string title;
    bool is_loaded = false;
    {
        std::lock_guard lock{tracks_mutex_};
        if (current_track_ >= tracks_.size())
            return;
        is_loaded = tracks_[current_track_].is_loaded;
        id = tracks_[current_track_].id;
        title = tracks_[current_track_].title;
    }
    if (is_loaded) {
        LOG_INFO(atom::log::audio::Music, "Music card playing track: " + title);
        // Do NOT call music_.Play() here: the streaming source's first
        // Play() can fill its decode buffer synchronously, which is exactly
        // the hitch we are trying to avoid on the render thread. Hand it to a
        // worker and optimistically mark the card as playing.
        pending_play_request_ = ++next_play_request_;
        play_start_pending_ = true;
        DispatchPlayOnJobSystem(id, pending_play_request_);
        is_playing_ = true;
    } else {
        play_start_pending_ = false;
        is_playing_ = false;
        LOG_WARNING(atom::log::audio::Music, "Music card cannot play track (audio not loaded yet): " + title);
    }
}

auto MusicCardScreen::QueueAudioControl(std::string name, std::function<void()> action) -> void {
    auto& jobs = atom::job::JobSystem::GetInstance();
    std::lock_guard lock{audio_jobs_mutex_};
    std::vector<atom::job::JobHandle> dependencies;
    if (audio_command_tail_.IsValid())
        dependencies.push_back(audio_command_tail_);
    const auto handle = jobs.Schedule(std::move(name), std::move(action), atom::job::Priority::High,
                                      std::move(dependencies));
    audio_command_tail_ = handle;
    audio_jobs_.push_back(handle);
}

auto MusicCardScreen::DispatchPlayOnJobSystem(const std::string& id, const std::uint64_t request) -> void {
    QueueAudioControl(
        "musiccard:play", [this, id, request] {
            music_.Play(id);
            const auto state = music_.GetState(id);
            if (state != atom::audio::AudioSourceState::Playing &&
                state != atom::audio::AudioSourceState::Paused) {
                LOG_WARNING(atom::log::audio::Music,
                            "Music card could not start track (id not registered on the active audio backend): " +
                                id);
            }
            completed_play_request_.store(request, std::memory_order_release);
        });
}

auto MusicCardScreen::DispatchStopOnJobSystem(const std::string& id) -> void {
    QueueAudioControl("musiccard:stop", [this, id] { music_.Stop(id); });
}

auto MusicCardScreen::DispatchPauseOnJobSystem(const std::string& id) -> void {
    QueueAudioControl("musiccard:pause", [this, id] { music_.Pause(id); });
}

auto MusicCardScreen::DispatchSeekOnJobSystem(std::string id, const float seconds) -> void {
    bool schedule = false;
    {
        std::lock_guard lock{audio_jobs_mutex_};
        pending_seek_ = PendingSeek{std::move(id), seconds};
        if (!seek_command_queued_) {
            seek_command_queued_ = true;
            schedule = true;
        }
    }
    if (!schedule)
        return;

    QueueAudioControl("musiccard:seek", [this] {
        std::optional<PendingSeek> request;
        {
            std::lock_guard lock{audio_jobs_mutex_};
            request = std::move(pending_seek_);
            seek_command_queued_ = false;
        }
        if (request)
            (void)music_.Seek(request->id, request->seconds);
    });
}

auto MusicCardScreen::StopCurrentTrack() -> void {
    start_after_load_ = false;
    play_start_pending_ = false;
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
        LOG_INFO(atom::log::audio::Music, "Music card stopping track: " + title);
        DispatchStopOnJobSystem(id);
    }
    is_playing_ = false;
}

auto MusicCardScreen::BuildTrackStubs(std::vector<std::string> paths) -> void {
    constexpr std::array themes{
        MusicCardTheme{{52, 45, 111, 255}, {92, 74, 168, 255}, {132, 219, 214, 255}},
        MusicCardTheme{{24, 82, 96, 255}, {39, 134, 139, 255}, {245, 188, 111, 255}},
    };
    tracks_.reserve(paths.size());
    for (auto i = std::size_t{0}; i < paths.size(); ++i) {
        atom::fs::AssetPath asset_path{};
        const auto filename = atom::PathToUtf8(std::filesystem::path{paths[i]}.filename());
        if (!filename.empty())
            (void)atom::fs::AssetPath::TryParse("res://" + filename, asset_path);
        Track stub{};
        stub.id = "music_card_track_" + std::to_string(i);
        stub.path = std::move(paths[i]);
        stub.asset_path = asset_path;
        stub.theme = themes[i % themes.size()];
        tracks_.push_back(std::move(stub));
    }
}

auto MusicCardScreen::LoadTrackMetadata(std::size_t index) -> void {
    if (index >= tracks_.size())
        return;
    std::uint64_t generation = 0;
    {
        std::lock_guard lock{tracks_mutex_};
        if (tracks_[index].metadata_loaded)
            return;
        generation = cache_generation_;
    }
    // Snapshot immutable fields outside the lock.
    const std::string path = tracks_[index].path;
    const atom::fs::AssetPath asset_path = tracks_[index].asset_path;
    const std::string id = tracks_[index].id;

    LOG_DEBUG(atom::log::audio::Metadata, "Lazy-loading metadata for track " + std::to_string(index) + ": " + path);
    {
        std::lock_guard lock{tracks_mutex_};
        std::erase(pending_unloads_, id);
    }

    // Phase 1: metadata only. Reading tags via TagLib is comparatively cheap
    // and produces everything the card needs to render (title, artist, artwork
    // and its derived theme). Audio decoding/stream allocation is deliberately
    // deferred to LoadTrackAudio so the artwork can reach the renderer first.
    auto metadata = filesystem_ && asset_path.IsValid()
                        ? atom::audio::AudioMetadataReader::Read(*filesystem_, asset_path)
                        : std::optional<atom::audio::AudioMetadata>{std::nullopt};
    auto title = metadata && !metadata->title.empty() ? metadata->title : std::filesystem::path{path}.stem().string();
    auto artist = metadata && !metadata->artist.empty() ? metadata->artist : std::string{"UNKNOWN ARTIST"};
    auto artwork_mime_type = metadata ? std::move(metadata->artworkMimeType) : std::string{};
    auto artwork_data = metadata ? std::move(metadata->artworkData) : std::vector<uint8_t>{};

    bool discarded = false;
    {
        std::lock_guard lock{tracks_mutex_};
        if (generation != cache_generation_) {
            LOG_INFO(atom::log::audio::Metadata,
                     "Discarding metadata for track " + std::to_string(index) +
                         " resolved for a previous audio backend; it will be re-resolved");
            prefetch_requested_ = true;
            discarded = true;
        } else if (!IsResidentLocked(index)) {
            LOG_DEBUG(atom::log::audio::Metadata,
                      "Discarding metadata for track " + std::to_string(index) + " resolved outside the resident window");
            discarded = true;
        } else {
            auto& t = tracks_[index];
            t.title = std::move(title);
            t.artist = std::move(artist);
            t.artwork_mime_type = std::move(artwork_mime_type);
            t.artwork_data = std::move(artwork_data);
            t.artwork_image.reset();
            t.artwork_decode_requested = false;
            ++t.artwork_revision;
            t.metadata_loaded = true;
        }
    }
    if (discarded)
        return;
    LOG_INFO(atom::log::audio::Metadata, "Resolved metadata for track " + std::to_string(index));
    DecodeTrackArtwork(index, generation);
    // Phase 2: defer audio load. It is scheduled at Low priority so it never
    // competes with higher-priority work and cannot stall a frame.
    LoadTrackAudio(index);
}

auto MusicCardScreen::DecodeTrackArtwork(const std::size_t index, const std::uint64_t generation) -> void {
    std::vector<uint8_t> encoded;
    std::uint64_t revision = 0;
    {
        std::lock_guard lock{tracks_mutex_};
        if (index >= tracks_.size() || generation != cache_generation_ || !IsResidentLocked(index))
            return;
        auto& track = tracks_[index];
        if (!track.metadata_loaded || track.artwork_decode_requested || track.artwork_data.empty())
            return;
        track.artwork_decode_requested = true;
        revision = track.artwork_revision;
        encoded = track.artwork_data;
    }

    auto& jobs = atom::job::JobSystem::GetInstance();
    const auto handle = jobs.Schedule(
        "musiccard:decode-cover",
        [this, index, generation, revision, encoded = std::move(encoded)] {
            auto decoded = MakeCoverThumbnail(atom::image::DecodeImageMemory(std::as_bytes(std::span{encoded}), false));
            const auto theme = decoded.IsValid() ? BuildMusicCardTheme(decoded) : std::optional<MusicCardTheme>{};
            auto artwork = decoded.IsValid()
                               ? std::make_shared<const atom::image::DecodedImage>(std::move(decoded))
                               : std::shared_ptr<const atom::image::DecodedImage>{};

            std::lock_guard lock{tracks_mutex_};
            if (index >= tracks_.size() || generation != cache_generation_ || !IsResidentLocked(index))
                return;
            auto& track = tracks_[index];
            if (!track.metadata_loaded || track.artwork_revision != revision)
                return;
            track.artwork_decode_requested = false;
            track.artwork_data.clear();
            track.artwork_data.shrink_to_fit();
            track.artwork_image = std::move(artwork);
            if (theme.has_value()) {
                track.theme = *theme;
                LOG_INFO(atom::log::audio::Metadata,
                         "Built MusicCard theme from background-decoded artwork for track " + std::to_string(index));
            }
        },
        atom::job::Priority::Low);
    {
        std::lock_guard lock{artwork_jobs_mutex_};
        artwork_jobs_.push_back(handle);
    }
}

auto MusicCardScreen::LoadTrackAudio(std::size_t index) -> void {
    if (index >= tracks_.size())
        return;
    std::string id;
    atom::fs::AssetPath asset_path;
    bool already_loaded = false;
    {
        std::lock_guard lock{tracks_mutex_};
        const auto& t = tracks_[index];
        id = t.id;
        asset_path = t.asset_path;
        already_loaded = t.is_loaded;
    }
    if (already_loaded)
        return;
    if (!filesystem_ || !asset_path.IsValid())
        return;

    // Run the (potentially blocking) decoder open + stream-buffer allocation on
    // a worker thread, at Low priority. The result is folded back into the
    // track's is_loaded flag so TryStartAfterLoad can pick it up next frame.
    auto& jobs = atom::job::JobSystem::GetInstance();
    const auto handle = jobs.Schedule(
        "musiccard:load-audio",
        [this, id = std::move(id), asset_path = std::move(asset_path)] {
            // A previous incarnation of this id may still be resident in the
            // player (e.g. the track was evicted and re-entered the window
            // before its pending unload drained). Drop it first so Load() does
            // not short-circuit onto a source that is about to be released.
            // Unload() returning false means the id was not resident, which is
            // exactly the state this branch is trying to reach, so it is not a
            // failure worth reporting.
            if (music_.IsLoaded(id) && !music_.Unload(id)) {
                LOG_DEBUG(atom::log::audio::Music, "Track audio was already released: " + id);
            }
            const bool loaded = music_.Load(id, *filesystem_, asset_path);
            {
                std::lock_guard lock{tracks_mutex_};
                // Find by id and mark it loaded only if still resident.
                for (auto& t : tracks_) {
                    if (t.id == id) {
                        t.is_loaded = loaded;
                        break;
                    }
                }
            }
            LOG_DEBUG(atom::log::audio::Music,
                      "Track audio " + (loaded ? std::string{"loaded"} : std::string{"failed to load"}) +
                          " on worker: " + id);
        },
        atom::job::Priority::Low);
    {
        std::lock_guard lock{audio_jobs_mutex_};
        audio_jobs_.push_back(handle);
    }
}

auto MusicCardScreen::DrainPendingUnloads() -> void {
    std::vector<std::string> to_unload;
    {
        std::lock_guard lock{tracks_mutex_};
        to_unload.swap(pending_unloads_);
    }
    for (const auto& id : to_unload) {
        // A rejected unload means the track was already gone - the same state this
        // drain is trying to reach - so it is reported at debug level rather than
        // treated as an error.
        if (!music_.Unload(id))
            LOG_DEBUG(atom::log::audio::Music, "Drain: track audio was already released: " + id);
    }
}

auto MusicCardScreen::LoaderLoop(std::stop_token stop) -> void {
    atom::concurrency::ThreadRegistry::ScopedRegistration registration{"MusicCardLoader"};
    LOG_DEBUG(atom::log::core::Screen, "Music card background loader started");
    while (!stop.stop_requested()) {
        bool has_prefetch = false;
        {
            std::unique_lock lock{tracks_mutex_};
            loader_cv_.wait(lock, stop, [this] { return prefetch_requested_ || !pending_unloads_.empty(); });
            if (stop.stop_requested())
                break;
            has_prefetch = prefetch_requested_;
            prefetch_requested_ = false;
        }
        DrainPendingUnloads();
        if (!has_prefetch || tracks_.empty())
            continue;
        const auto target = [&] {
            std::lock_guard lock{tracks_mutex_};
            return pending_track_ < tracks_.size() ? pending_track_ : current_track_;
        }();
        const auto count = tracks_.size();
        const std::size_t indices[] = {
            target,
            (target + count - 1) % count,
            (target + 1) % count,
        };
        for (const auto idx : indices) {
            if (stop.stop_requested())
                break;
            LoadTrackMetadata(idx);
        }
    }
    LOG_DEBUG(atom::log::core::Screen, "Music card background loader stopped");
}
