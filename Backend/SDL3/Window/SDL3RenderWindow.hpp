#ifndef ATOM_SDL3_RENDER_WINDOW_HPP
#define ATOM_SDL3_RENDER_WINDOW_HPP

#include <SDL3/SDL.h>
#include <Backend/Contracts/Render/IRenderWindow.hpp>
#include <Backend/SDL3/Core/SDLRuntime.hpp>

#include <unordered_map>
#include <memory>
#include <functional>

namespace atom {

class SDL3RenderWindow : public IRenderWindow {
public:
    SDL3RenderWindow() = default;
    ~SDL3RenderWindow() override;

    SDL3RenderWindow(const SDL3RenderWindow&) = delete;
    auto operator=(const SDL3RenderWindow&) -> SDL3RenderWindow& = delete;

    // IRenderWindow
    auto Initialize(const std::string& title, Vec2 resolution) -> void override;
    [[nodiscard]] auto IsOpen() const -> bool override;
    auto Shutdown() -> void override;
    [[nodiscard]] auto PollEvent() -> std::optional<IEvent> override;
    auto SetFPS(uint32_t fps) -> void override;
    [[nodiscard]] auto GetFPS() const -> uint32_t override;

    // IRenderTarget
    auto Clear(const Color& color) -> void override;
    auto Display() -> void override;
    [[nodiscard]] auto GetSize() const -> Vec2 override;
    auto SetViewport(const Rect& viewport) -> void override;
    [[nodiscard]] auto GetViewport() const -> Rect override;

    // Drawing (IRenderTarget)
    auto DrawTexture(ITexture& texture, float x, float y) -> void override;
    auto DrawCircle(float cx, float cy, float radius, const Color& color) -> void override;
    auto DrawRect(float x, float y, float w, float h, const Color& color) -> void override;

    // Native handles (ImGui interop)
    [[nodiscard]] auto GetNativeWindowHandle() const -> void* override;
    [[nodiscard]] auto GetNativeRendererHandle() const -> void* override;

    // Callback hooks (used by Debugger / ImGui overlay)
    // Called with the raw SDL_Event *before* translation (for ImGui event processing).
    std::function<void(const SDL_Event&)> on_pre_process_sdl_event_;
    // Called with the translated IEvent after pre-processing (for game-level hooks).
    std::function<void(IEvent&)> on_process_event_;
    std::function<void(float)> on_update_;
    std::function<void()> on_render_overlay_;
    std::function<void()> on_shutdown_;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    uint32_t fps_limit_ = 60;
    bool open_ = false;
    SDLSubsystemLease video_runtime_;
    SDLSubsystemLease events_runtime_;

    // Circle texture cache — avoids regenerating identical circles.
    struct CircleKey {
        float radius;
        Color color;
    };
    struct CircleHash {
        auto operator()(const CircleKey& k) const -> std::size_t;
    };
    friend auto operator==(const CircleKey& a, const CircleKey& b) -> bool {
        return a.radius == b.radius && a.color.r == b.color.r && a.color.g == b.color.g && a.color.b == b.color.b &&
               a.color.a == b.color.a;
    }
    std::unordered_map<CircleKey, SDL_Texture*, CircleHash> circle_cache_;

    auto GetOrCreateCircleTexture(float radius, const Color& color) -> SDL_Texture*;
};

} // namespace atom

#endif // ATOM_SDL3_RENDER_WINDOW_HPP
