/**
 * @file UiCanvas.hpp
 * @brief Screen-space bridge from retained UI to Renderer2D.
 */
#ifndef ATOM_UI_UI_CANVAS_HPP
#define ATOM_UI_UI_CANVAS_HPP

namespace atom::render {
class Font;
class Renderer2D;
}
namespace atom::ui {
class UiTree;

// A UI canvas always begins Renderer2D at origin (0, 0), zoom 1. It makes the
// screen-space contract explicit and prevents world-camera state from leaking
// into a game menu/HUD. A separate canvas submission can be rendered after a
// world renderer in the same device frame.
class UiCanvas final {
    public:
        [[nodiscard]] static auto RenderFrame(render::Renderer2D& renderer, UiTree& tree,
                                              const render::Font* font = nullptr) -> bool;
};

} // namespace atom::ui

#endif // ATOM_UI_UI_CANVAS_HPP
