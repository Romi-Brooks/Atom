/**
  * @file           : Debugger.hpp
  * @author         : Romi Brooks
  * @brief          : Abstract base class for debug overlays (ImGui SDL3 backend)
  * @attention      : Inherit from this class and override OnDrawOverlay()
  *                   to create custom debug content for any RenderWindow.
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_DEBUGGER_HPP
#define ATOM_DEBUGGER_HPP

#include <cstddef>

namespace atom {

class RenderWindow;

// Abstract base class for debug overlays.
// Manages the ImGui SDL3 lifecycle; override OnDrawOverlay() to supply content.
class Debugger {
    private:
        bool attached_ = false;
        RenderWindow* target_window_ = nullptr;
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

        // Attach to a RenderWindow (initializes backend, hooks callbacks)
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
        // 重写此方法以绘制调试叠加层内容。每帧调用。
        virtual auto OnDrawOverlay() -> void {}
};

} // namespace atom

#endif // ATOM_DEBUGGER_HPP
