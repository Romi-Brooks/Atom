#include "UiCanvas.hpp"

#include <Render/Renderer2D/Renderer2D.hpp>
#include <UI/UiTree.hpp>

namespace atom::ui {

auto UiCanvas::RenderFrame(render::Renderer2D& renderer, UiTree& tree, const render::Font* font) -> bool {
    if (!renderer.BeginFrame(0.0f, 0.0f, 1.0f))
        return false;
    tree.Render(renderer, font);
    return renderer.EndFrame();
}

} // namespace atom::ui
