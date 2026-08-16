#ifndef ATOM_SDL3_DEBUG_IMGUI_BACKEND_HPP
#define ATOM_SDL3_DEBUG_IMGUI_BACKEND_HPP

#include <memory>

namespace atom {

class IRenderWindow;
class IDebugImGuiBackend;

// Creates the SDL3 ImGui platform/render backend if the window is SDL3-backed
// (queries ISDL3WindowExtensions), otherwise returns nullptr. Engine-internal:
// this is the debug-layer backend selection seam for the SDL3 render backend.
auto CreateDebugImGuiBackend(IRenderWindow& window) -> std::unique_ptr<IDebugImGuiBackend>;

} // namespace atom

#endif // ATOM_SDL3_DEBUG_IMGUI_BACKEND_HPP
