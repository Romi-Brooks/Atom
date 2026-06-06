/**
  * @file           : Debugger.hpp
  * @author         : Romi Brooks
  * @brief          : Abstract base class for debug overlays (ImGui-SFML backend)
  * @attention      : Inherit from this class and override OnDrawOverlay()
  *                   to create custom debug content for any RenderWindow.
  * @date           : 2026/6/6
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_DEBUGGER_HPP
#define ATOM_DEBUGGER_HPP

// Third Party Library
#include <SFML/Graphics.hpp>

namespace atom {

class RenderWindow;

// Abstract base class for debug overlays.
// Manages the ImGui-SFML lifecycle; override OnDrawOverlay() to supply content.
class Debugger {
    private:
        bool attached_ = false;
        RenderWindow* target_window_ = nullptr;

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

    protected:
        // Override this to draw your debug overlay content.
        // Called every frame inside the overlay. Use ImGui::Begin/End here.
        // 重写此方法以绘制调试叠加层内容。每帧调用。
        virtual auto OnDrawOverlay() -> void {};
};

} // namespace atom

#endif // ATOM_DEBUGGER_HPP
