#include "SDL3RenderWindow.hpp"

#include <Backend/SDL3/Core/SDLRuntime.hpp>
#include <Backend/SDL3/Render/SDL3Texture.hpp>

#include <cstring>
#include <utility>

namespace atom {

// ── Helper: translate SDL3 event to IEvent ──────────────────────────
static auto TranslateEvent(const SDL_Event& ev) -> IEvent {
    IEvent result{};

    switch (ev.type) {
    case SDL_EVENT_QUIT:
        result.type = EventType::Closed;
        break;

    case SDL_EVENT_KEY_DOWN:
        result.type = EventType::KeyPressed;
        result.data =
            KeyEvent{static_cast<int32_t>(ev.key.scancode), static_cast<int32_t>(ev.key.key),
                     static_cast<bool>(ev.key.mod & SDL_KMOD_ALT), static_cast<bool>(ev.key.mod & SDL_KMOD_CTRL),
                     static_cast<bool>(ev.key.mod & SDL_KMOD_SHIFT)};
        break;

    case SDL_EVENT_KEY_UP:
        result.type = EventType::KeyReleased;
        result.data =
            KeyEvent{static_cast<int32_t>(ev.key.scancode), static_cast<int32_t>(ev.key.key),
                     static_cast<bool>(ev.key.mod & SDL_KMOD_ALT), static_cast<bool>(ev.key.mod & SDL_KMOD_CTRL),
                     static_cast<bool>(ev.key.mod & SDL_KMOD_SHIFT)};
        break;

    case SDL_EVENT_MOUSE_MOTION:
        result.type = EventType::MouseMoved;
        result.data = MouseEvent{ev.motion.x, ev.motion.y, 0};
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        result.type = EventType::MouseButtonPressed;
        result.data = MouseEvent{ev.button.x, ev.button.y, static_cast<int32_t>(ev.button.button)};
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        result.type = EventType::MouseButtonReleased;
        result.data = MouseEvent{ev.button.x, ev.button.y, static_cast<int32_t>(ev.button.button)};
        break;

    case SDL_EVENT_WINDOW_RESIZED:
        result.type = EventType::Resized;
        result.data = ResizeEvent{static_cast<uint32_t>(ev.window.data1), static_cast<uint32_t>(ev.window.data2)};
        break;

    default:
        // Unhandled event — leave type in default state (will be ignored).
        break;
    }

    return result;
}

// ── CircleKey hash ──────────────────────────────────────────────────
auto SDL3RenderWindow::CircleHash::operator()(const CircleKey& k) const -> std::size_t {
    auto h1 = std::hash<float>{}(k.radius);
    auto h2 = (static_cast<std::size_t>(k.color.r) << 24) | (static_cast<std::size_t>(k.color.g) << 16) |
              (static_cast<std::size_t>(k.color.b) << 8) | static_cast<std::size_t>(k.color.a);
    return h1 ^ (h2 << 1);
}

// ── Lifecycle ───────────────────────────────────────────────────────
SDL3RenderWindow::~SDL3RenderWindow() {
    Shutdown();
}

auto SDL3RenderWindow::Initialize(const std::string& title, Vec2 resolution) -> void {
    SDLSubsystemLease video_runtime(SDLSubsystem::Video);
    SDLSubsystemLease events_runtime(SDLSubsystem::Events);
    if (!video_runtime.IsValid() || !events_runtime.IsValid()) {
        return;
    }

    window_ = SDL_CreateWindow(title.c_str(), static_cast<int>(resolution.GetX()), static_cast<int>(resolution.GetY()),
                               SDL_WINDOW_RESIZABLE);

    if (!window_) {
        return;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return;
    }

    video_runtime_ = std::move(video_runtime);
    events_runtime_ = std::move(events_runtime);
    open_ = true;
}

auto SDL3RenderWindow::Shutdown() -> void {
    if (!window_ && !renderer_ && !video_runtime_.IsValid() && !events_runtime_.IsValid())
        return;

    for (auto& [key, tex] : circle_cache_) {
        SDL_DestroyTexture(tex);
    }
    circle_cache_.clear();

    if (renderer_)
        SDL_DestroyRenderer(renderer_);
    if (window_)
        SDL_DestroyWindow(window_);
    renderer_ = nullptr;
    window_ = nullptr;
    open_ = false;

    events_runtime_.Reset();
    video_runtime_.Reset();
}

auto SDL3RenderWindow::IsOpen() const -> bool {
    return open_;
}

// ── Render target ───────────────────────────────────────────────────
auto SDL3RenderWindow::Clear(const Color& color) -> void {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer_);
}

auto SDL3RenderWindow::Display() -> void {
    SDL_RenderPresent(renderer_);
}

auto SDL3RenderWindow::GetSize() const -> Vec2 {
    int w, h;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    return {static_cast<float>(w), static_cast<float>(h)};
}

auto SDL3RenderWindow::SetViewport(const Rect& viewport) -> void {
    SDL_Rect r;
    r.x = static_cast<int>(viewport.x);
    r.y = static_cast<int>(viewport.y);
    r.w = static_cast<int>(viewport.w);
    r.h = static_cast<int>(viewport.h);
    SDL_SetRenderViewport(renderer_, &r);
}

auto SDL3RenderWindow::GetViewport() const -> Rect {
    SDL_Rect r;
    SDL_GetRenderViewport(renderer_, &r);
    return {static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h)};
}

// ── Drawing ─────────────────────────────────────────────────────────
auto SDL3RenderWindow::DrawTexture(ITexture& texture, float x, float y) -> void {
    auto& sdlTex = dynamic_cast<SDL3Texture&>(texture);
    SDL_Texture* native = sdlTex.GetNativeTexture();
    if (!native)
        return;

    const auto size = sdlTex.GetSize();
    SDL_FRect dst = {x, y, size.GetX(), size.GetY()};
    SDL_RenderTexture(renderer_, native, nullptr, &dst);
}

auto SDL3RenderWindow::DrawCircle(float cx, float cy, float radius, const Color& color) -> void {
    SDL_Texture* tex = GetOrCreateCircleTexture(radius, color);
    if (!tex)
        return;

    float tw, th;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst = {cx - radius, cy - radius, tw, th};
    SDL_RenderTexture(renderer_, tex, nullptr, &dst);
}

auto SDL3RenderWindow::DrawRect(float x, float y, float w, float h, const Color& color) -> void {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
}

// ── Circle texture generation (CPU-side pixel buffer) ───────────────
auto SDL3RenderWindow::GetOrCreateCircleTexture(float radius, const Color& color) -> SDL_Texture* {
    const CircleKey key{radius, color};
    auto it = circle_cache_.find(key);
    if (it != circle_cache_.end()) {
        return it->second;
    }

    const int diameter = static_cast<int>(radius * 2.0f + 1.0f);
    const float r_sq = radius * radius;

    // Allocate pixel buffer (RGBA8888)
    auto* pixels = new uint8_t[static_cast<std::size_t>(diameter) * diameter * 4];
    std::memset(pixels, 0, static_cast<std::size_t>(diameter) * diameter * 4);

    const float center = radius; // center of the circle in pixel coords

    for (int py = 0; py < diameter; ++py) {
        for (int px = 0; px < diameter; ++px) {
            const float dx = static_cast<float>(px) - center;
            const float dy = static_cast<float>(py) - center;
            if (dx * dx + dy * dy <= r_sq) {
                const size_t idx = static_cast<size_t>((py * diameter + px) * 4);
                pixels[idx + 0] = color.r;
                pixels[idx + 1] = color.g;
                pixels[idx + 2] = color.b;
                pixels[idx + 3] = color.a;
            }
        }
    }

    SDL_Texture* tex =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, diameter, diameter);

    if (tex) {
        const int pitch = diameter * 4;
        SDL_UpdateTexture(tex, nullptr, pixels, pitch);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }

    delete[] pixels;

    // Cache even if null (avoid retrying failed allocations every frame)
    circle_cache_[key] = tex;
    return tex;
}

// ── Events ──────────────────────────────────────────────────────────
auto SDL3RenderWindow::PollEvent() -> std::optional<IEvent> {
    SDL_Event ev;
    if (!SDL_PollEvent(&ev))
        return std::nullopt;

    // Pre-process raw event (platform adapter / ImGui hook) before translation.
    if (raw_event_hook_) {
        raw_event_hook_(&ev);
    }

    IEvent result = TranslateEvent(ev);

    if (ev.type == SDL_EVENT_QUIT) {
        open_ = false;
    }

    return result;
}

// ── FPS ─────────────────────────────────────────────────────────────
auto SDL3RenderWindow::SetFPS(uint32_t fps) -> void {
    fps_limit_ = fps;
    // SDL3 doesn't have a built-in framerate limit; the main loop
    // is responsible for timing.
}

auto SDL3RenderWindow::GetFPS() const -> uint32_t {
    return fps_limit_;
}

auto SDL3RenderWindow::SetRawEventHook(std::function<void(const void*)> hook) -> void {
    raw_event_hook_ = std::move(hook);
}

// ── Native handles (ISDL3WindowExtensions) ──────────────────────────
auto SDL3RenderWindow::GetNativeWindow() const -> SDL_Window* {
    return window_;
}

auto SDL3RenderWindow::GetNativeRenderer() const -> SDL_Renderer* {
    return renderer_;
}

// ── Resize ──────────────────────────────────────────────────────────
auto SDL3RenderWindow::HandleResize(uint32_t /*width*/, uint32_t /*height*/) -> void {
    // SDL3 renderer tracks window size changes internally; nothing to do.
    // A Vulkan backend would recreate its swapchain here.
}

} // namespace atom
