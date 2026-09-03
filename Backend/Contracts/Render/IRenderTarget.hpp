#ifndef ATOM_IRENDER_TARGET_HPP
#define ATOM_IRENDER_TARGET_HPP

// Legacy compatibility contract retained only for the pre-SDL_GPU ECS API.
// New rendering code must use IRenderDevice/IRender2DContext/Renderer2D;
// this interface is not implemented by the SDL_GPU backend.

#include <cstdint>

#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Render/RenderTypes.hpp>

namespace atom::render {

class ITexture; // atom::render::ITexture

class IRenderTarget {
    public:
        virtual ~IRenderTarget() = default;

        virtual auto Clear(const Color& color = Color::Black()) -> void = 0;
        virtual auto Display() -> void = 0;

        [[nodiscard]] virtual auto GetSize() const -> algo::Vec2 = 0;
        virtual auto SetViewport(const Rect& viewport) -> void = 0;
        [[nodiscard]] virtual auto GetViewport() const -> Rect = 0;

        // Drawing primitives
        virtual auto DrawTexture(ITexture& texture, float x, float y) -> void = 0;
        virtual auto DrawCircle(float cx, float cy, float radius, const Color& color) -> void = 0;
        virtual auto DrawRect(float x, float y, float w, float h, const Color& color) -> void = 0;
};

} // namespace atom::render

#endif // ATOM_IRENDER_TARGET_HPP
