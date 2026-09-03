#ifndef ATOM_SDL3_DEBUG_IMGUI_BACKEND_HPP
#define ATOM_SDL3_DEBUG_IMGUI_BACKEND_HPP

#include <memory>

namespace atom::window {
class IRenderWindow;
}

namespace atom::debugger {
class IDebugImGuiBackend;
}

namespace atom::backend::sdl3 {

// Creates the SDL3 ImGui platform/render backend if the window is SDL3-backed
// (queries ISDL3WindowExtensions), otherwise returns nullptr. Engine-internal:
// this is the debug-layer backend selection seam for the SDL3 render backend.
auto CreateDebugImGuiBackend(window::IRenderWindow& window) -> std::unique_ptr<debugger::IDebugImGuiBackend>;

} // namespace atom::backend::sdl3

#endif // ATOM_SDL3_DEBUG_IMGUI_BACKEND_HPP
