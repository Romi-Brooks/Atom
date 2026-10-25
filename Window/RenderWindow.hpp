/**
  * @file           : RenderWindow.hpp
  * @author         : Romi Brooks
  * @brief          : Platform window and render-frame host.
 * @attention      : Wraps an atom::render::IRenderBackend selected by the engine runtime.
  *                   behind a stable singleton API. Never depends on a concrete
  *                   backend type; pick one with RenderBackendId.
  * @date           : 2025/9/28
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_RENDER_WINDOW_HPP
#define ATOM_RENDER_WINDOW_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <functional>
#include <memory>

#include <Backend/Contracts/Render/IRenderBackend.hpp>
#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Backend/Contracts/Render/RenderBackendConfig.hpp>
#include <Backend/Contracts/Render/RenderBackendId.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>
#include <Backend/Contracts/Window/WindowMode.hpp>
#include <Error/Error.hpp>

namespace atom {

namespace runtime {
class Application;
}

class RenderWindow {
    private:
        friend class runtime::Application;
        std::unique_ptr<atom::render::IRenderBackend> backend_;
        std::string name_{};
        std::string backend_id_{};
        unsigned int fps_ = 60;
        bool shutdown_notified_ = false;

        struct PendingResize {
                uint32_t width = 0;
                uint32_t height = 0;
                double last_change_seconds = 0.0;
        };
        std::optional<PendingResize> pending_resize_{};
        static constexpr double resize_settle_delay_seconds = 0.2;

        RenderWindow() = default;
        ~RenderWindow();

        using EventSink = std::function<void(atom::window::IEvent&)>;

        [[nodiscard]] auto Initialize(const backend::RenderBackendConfig& config) -> Result<void>;
        auto PumpEvents(const EventSink& sink) -> void;
        [[nodiscard]] auto BeginFrame() -> bool;
        auto EndFrame() -> void;
        auto WaitForNextFrame() -> void;
        auto Shutdown() -> void;
        [[nodiscard]] auto GetTimeSource() -> atom::backend::ITimeSource*;

    public:
        RenderWindow(const RenderWindow&) = delete;
        auto operator=(const RenderWindow&) -> RenderWindow& = delete;

        [[nodiscard]] static auto GetInstance() -> RenderWindow&;

        // Runtime/Application owns initialization, the frame loop, time
        // domains, ScreenStack, and shutdown. This public surface is limited
        // to frame-rate pacing, stop requests, and debugger access.
        //
        // Window placement and present mode are NOT here: they are the
        // DisplaySystem's policy (Display/DisplaySystem.hpp), because a
        // swapchain reconfiguration has to happen at the frame boundary and a
        // settings page, a config file and a debug panel all need to request it.
        // Keeping that state in this facade would give it two owners.
        auto SetFPS(unsigned int fps) -> void;
        [[nodiscard]] auto GetFPS() const -> unsigned;

        [[nodiscard]] auto IsOpen() const -> bool;
        // Requests the window to close without releasing resources. Screens use
        // this to exit from event handling; Runtime tears down at a frame
        // boundary, so a screen is never destroyed on its own stack.
        auto RequestClose() -> void;

        // Window name (the title passed to Initialize). Used by debugger logs.
        [[nodiscard]] auto GetName() const -> const std::string&;

        // Backend access
        [[nodiscard]] auto GetIWindow() -> atom::window::IWindow*;
        [[nodiscard]] auto GetRenderDevice() -> atom::render::IRenderDevice*;
        [[nodiscard]] auto GetBackendId() const -> const std::string&;
};

} // namespace atom

#endif // ATOM_RENDER_WINDOW_HPP
