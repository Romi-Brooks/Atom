#ifndef ATOM_BACKEND_CONTRACTS_DEBUG_IMGUI_BACKEND_HPP
#define ATOM_BACKEND_CONTRACTS_DEBUG_IMGUI_BACKEND_HPP

#include <Backend/Contracts/Window/IWindow.hpp>

namespace atom::debugger {

// Internal contract between the debug overlay layer (Debugger) and the ImGui
// platform/render backend bound to a render backend (SDL3 today, Vulkan later).
// Implementations own the ImGui context lifecycle; the Debugger only drives
// the frame hooks.
class IDebugImGuiBackend {
    public:
        virtual ~IDebugImGuiBackend() = default;

        // Creates the ImGui context and initializes platform + render backends.
        virtual auto Initialize() -> bool = 0;

        // Called once per frame before the scene renders (ImGui NewFrame chain).
        virtual auto NewFrame() -> void = 0;

        // Called once per frame after the scene renders; submits ImGui draw data.
        virtual auto Render() -> void = 0;

        // Feeds one normalized Atom event to the ImGui platform backend.
        virtual auto ProcessEvent(const window::IEvent& event) -> void = 0;

        // Tears down the ImGui context.
        virtual auto Shutdown() -> void = 0;
};

} // namespace atom::debugger

#endif // ATOM_BACKEND_CONTRACTS_DEBUG_IMGUI_BACKEND_HPP
