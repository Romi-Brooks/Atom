#ifndef ATOM_IRENDER_WINDOW_HPP
#define ATOM_IRENDER_WINDOW_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Render/IRenderTarget.hpp>

namespace atom {

enum class EventType {
    None = 0, Closed, KeyPressed, KeyReleased,
    MouseMoved, MouseButtonPressed, MouseButtonReleased, Resized,
};

struct KeyEvent     { int32_t scancode = 0, keycode = 0; bool alt = false, ctrl = false, shift = false; };
struct MouseEvent   { float x = 0, y = 0; int32_t button = 0; };
struct ResizeEvent  { uint32_t width = 0, height = 0; };

struct IEvent {
    EventType type;
    std::variant<KeyEvent, MouseEvent, ResizeEvent> data;
};

class IRenderWindow : public IRenderTarget {
public:
    ~IRenderWindow() override = default;

    virtual auto Initialize(const std::string& title, Vec2 resolution) -> void = 0;
    [[nodiscard]] virtual auto IsOpen() const -> bool = 0;
    virtual auto Shutdown() -> void = 0;
    [[nodiscard]] virtual auto PollEvent() -> std::optional<IEvent> = 0;
    virtual auto SetFPS(uint32_t fps) -> void = 0;
    [[nodiscard]] virtual auto GetFPS() const -> uint32_t = 0;

    // Required for ImGui SDL3 backend interop.
    [[nodiscard]] virtual auto GetNativeWindowHandle() const -> void* = 0;
    [[nodiscard]] virtual auto GetNativeRendererHandle() const -> void* = 0;
};

} // namespace atom

#endif // ATOM_IRENDER_WINDOW_HPP
