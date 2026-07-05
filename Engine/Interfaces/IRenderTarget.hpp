#ifndef ATOM_IRENDER_TARGET_HPP
#define ATOM_IRENDER_TARGET_HPP

#include <cstdint>

namespace atom {

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    static constexpr Color Black()   { return {0, 0, 0}; }
    static constexpr Color White()   { return {255, 255, 255}; }
    static constexpr Color Red()     { return {255, 0, 0}; }
    static constexpr Color Green()   { return {0, 255, 0}; }
    static constexpr Color Blue()    { return {0, 0, 255}; }
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
};

class Vec2; // forward decl from Algorithm/Vector/Vec2.hpp
class ITexture;

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    virtual auto Clear(const Color& color = Color::Black()) -> void = 0;
    virtual auto Display() -> void = 0;

    [[nodiscard]] virtual auto GetSize() const -> Vec2 = 0;
    virtual auto SetViewport(const Rect& viewport) -> void = 0;
    [[nodiscard]] virtual auto GetViewport() const -> Rect = 0;

    // Drawing primitives
    virtual auto DrawTexture(ITexture& texture, float x, float y) -> void = 0;
    virtual auto DrawCircle(float cx, float cy, float radius, const Color& color) -> void = 0;
    virtual auto DrawRect(float x, float y, float w, float h, const Color& color) -> void = 0;
};

} // namespace atom

#endif // ATOM_IRENDER_TARGET_HPP
