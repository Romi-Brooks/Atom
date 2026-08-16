/**
  * @file           : Debugger.hpp
  * @author         : Romi Brooks
  * @brief          : Abstract base class for debug overlays (ImGui)
  * @attention      : Backend-agnostic: inherit this class and override
  *                   OnDrawOverlay() to create custom debug content for any
  *                   RenderWindow. The ImGui platform/render backend is
  *                   selected internally through the window's backend
  *                   extension; include <Window/Overlay.hpp> for the ImGui API.
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_DEBUGGER_HPP
#define ATOM_DEBUGGER_HPP

#include <cstddef>
#include <memory>

#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>

namespace atom {

class RenderWindow;
class ListenerConnection;

// Abstract base class for debug overlays.
// Manages the ImGui lifecycle through IDebugImGuiBackend; override
// OnDrawOverlay() to supply content. Listens to RenderWindow through RAII
// ListenerConnection members, so multiple debuggers/overlays can coexist and
// Detach() only removes this debugger's own listeners.
class Debugger {
private:
    bool attached_ = false;
    RenderWindow* target_window_ = nullptr;
    std::unique_ptr<IDebugImGuiBackend> imgui_backend_;
    std::unique_ptr<ListenerConnection> raw_event_connection_;
    std::unique_ptr<ListenerConnection> update_connection_;
    std::unique_ptr<ListenerConnection> overlay_connection_;
    std::unique_ptr<ListenerConnection> shutdown_connection_;
    bool imgui_shutdown_ = false;

    // FPS tracking
    std::size_t frame_count_ = 0;
    float fps_accumulator_ = 0.0f;
    float fps_display_ = 0.0f;

public:
    Debugger() = default;
    virtual ~Debugger();

    Debugger(const Debugger&) = delete;
    auto operator=(const Debugger&) -> Debugger& = delete;

    // Attach to a RenderWindow (selects the ImGui backend, hooks callbacks)
    auto Attach(RenderWindow& window) -> void;

    // Detach from its RenderWindow (clears callbacks, shuts down backend)
    auto Detach() -> void;

    [[nodiscard]] auto IsAttached() const -> bool {
        return attached_;
    }

    [[nodiscard]] auto GetFPS() const -> float {
        return fps_display_;
    }

protected:
    // Override this to draw your debug overlay content.
    // Called every frame inside the overlay. Use ImGui::Begin/End here.
    virtual auto OnDrawOverlay() -> void {}
};

} // namespace atom

#endif // ATOM_DEBUGGER_HPP
