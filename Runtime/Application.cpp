#include "Application.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

#include <Input/InputSystem.hpp>
#include <Display/DisplaySystem.hpp>
#include <Log/LogSystem.hpp>
#include <Runtime/ScreenManager.hpp>
#include <Time/TimeSystem.hpp>
#include <Window/RenderWindow.hpp>

namespace atom::runtime {

auto Application::GetInstance() -> Application& {
    static Application instance;
    return instance;
}

auto Application::Initialize(const ApplicationConfig& config) -> Result<void> {
    if (initialized_)
        return std::unexpected<Error>{MakeError(ErrorCode::AlreadyInitialized, Module::Core, "Application::Initialize")};

    auto& window = GetWindow();
    // One request struct all the way down: the runtime does not re-assemble the
    // backend's startup parameters from individual fields.
    backend::RenderBackendConfig backend_config{};
    backend_config.title = config.title;
    backend_config.resolution = config.resolution;
    backend_config.backend = config.render_backend;
    backend_config.driver = config.gpu_driver;
    backend_config.validation = config.validation;
    backend_config.window_mode = config.window_mode;
    backend_config.present_mode = config.present_mode;
    if (const auto result = window.Initialize(backend_config); !result)
        return result;

    // The backend came up with these values, so adopt them rather than queueing
    // a change: the display system must start in agreement with reality.
    display::DisplaySystem::GetInstance().Adopt(
        display::DisplaySettings{config.window_mode, config.present_mode});

    auto* source = window.GetTimeSource();
    if (!source) {
        window.Shutdown();
        return std::unexpected<Error>{MakeError(ErrorCode::TimeSourceUnavailable, Module::Time, "Application::Initialize")};
    }
    time::TimeSystem::GetInstance().Initialize(*source);
    initialized_ = true;
    shutdown_notified_ = false;
    return Ok();
}

auto Application::Initialize(std::string title, const algo::Vec2 resolution,
                             const backend::RenderBackendId render_backend) -> Result<void> {
    ApplicationConfig config{};
    config.title = std::move(title);
    config.resolution = resolution;
    config.render_backend = render_backend;
    return Initialize(config);
}

auto Application::Run() -> void {
    if (!initialized_)
        throw std::runtime_error("Application not initialized.");

    auto& screens = GetScreenManager();
    if (screens.GetCurrentScreenName().empty())
        throw std::runtime_error("No current screen set. Cannot run application.");

    auto& window = GetWindow();
    auto& time_system = time::TimeSystem::GetInstance();
    running_ = true;
    while (window.IsOpen()) {
        // One authoritative order: sample time, process input/events, fixed
        // simulation, variable presentation, render, tooling overlay, present.
        const auto frame = time_system.Tick();
        input::InputSystem::GetInstance().BeginFrame();
        window.PumpEvents([this](window::IEvent& event) { DispatchEvent(event); });
        if (!window.IsOpen())
            break;

        const auto* game_clock = time_system.FindDomain(time::domain::game);
        const auto* render_clock = time_system.FindDomain(time::domain::render);
        auto fixed_steps = 0U;
        auto fixed_step_seconds = 0.0F;
        auto interpolation_alpha = 0.0F;
        if (game_clock) {
            const auto& tick = game_clock->LastTick();
            fixed_steps = tick.steps;
            fixed_step_seconds = tick.fixed_step_ns > 0 ? time::ToFloatSeconds(tick.fixed_step_ns) : 0.0f;
            // How far the leftover accumulator reaches into the next step. Published so a
            // fixed-step screen can draw between the last two committed steps instead of
            // snapping to the newest one.
            interpolation_alpha = game_clock->Alpha();
        }
        // Publish the budget before dispatching it. A Screen that owns a fixed-step
        // world batches the steps into one ordered world frame from Update;
        // publishing it here is what lets it do that without guessing N (G0.1).
        screens.BeginFrame(fixed_steps, fixed_step_seconds, interpolation_alpha);
        for (uint32_t step = 0; step < fixed_steps; ++step) {
            screens.FixedUpdate(fixed_step_seconds);
            for (const auto& observer : fixed_update_observers_)
                observer.callback(fixed_step_seconds);
        }

        const auto variable_delta = render_clock ? render_clock->LastTick().delta_ns : frame.host_delta_ns;
        const auto delta_time = time::ToFloatSeconds(variable_delta);
        screens.Update(delta_time);
        for (const auto& observer : update_observers_)
            observer.callback(delta_time);

        // The frame boundary is the only safe place to reconfigure a swapchain:
        // the previous frame has been presented and this one has not acquired an
        // image yet. DisplaySystem applies at most one queued change per frame.
        if (auto* iwindow = window.GetIWindow()) {
            if (auto* device = window.GetRenderDevice())
                display::DisplaySystem::GetInstance().ApplyPending(*iwindow, *device);
        }
        if (!window.BeginFrame()) {
            window.WaitForNextFrame();
            continue;
        }
        auto* device = window.GetRenderDevice();
        if (device) {
            // Split the record window at the only boundary Runtime can see: the
            // screen's own drawing versus the debugger overlay. Published for the
            // Render panel, not logged: a per-frame number is something you watch,
            // not a line you grep. It exists because a backend can make one of the
            // two far more expensive without any single call looking slow.
            const auto screen_start = std::chrono::steady_clock::now();
            device->Clear(render::Color::Black());
            screens.Render(*device);
            const auto screen_end = std::chrono::steady_clock::now();
            for (const auto& observer : overlay_observers_)
                observer.callback();
            const auto overlay_end = std::chrono::steady_clock::now();
            frame_loop_stats_.screen_ms =
                std::chrono::duration<double, std::milli>(screen_end - screen_start).count();
            frame_loop_stats_.overlay_ms =
                std::chrono::duration<double, std::milli>(overlay_end - screen_end).count();
        }
        window.EndFrame();
        window.WaitForNextFrame();
    }
    running_ = false;
    Shutdown();
}

auto Application::GetFrameLoopStats() const -> const FrameLoopStats& {
    return frame_loop_stats_;
}
auto Application::Shutdown() -> void {
    if (!initialized_)
        return;
    if (!shutdown_notified_) {
        shutdown_notified_ = true;
        for (const auto& observer : shutdown_observers_)
            observer.callback();
    }
    // Screens may own renderer resources. Release them while the device still
    // exists; this ordering is Runtime policy, not a Window concern.
    GetScreenManager().ClearScreens();
    time::TimeSystem::GetInstance().Shutdown();
    GetWindow().Shutdown();
    initialized_ = false;
    running_ = false;
}

auto Application::RequestStop() -> void {
    GetWindow().RequestClose();
}

auto Application::GetWindow() -> RenderWindow& { return RenderWindow::GetInstance(); }
auto Application::GetScreenManager() -> ScreenManager& { return ScreenManager::GetInstance(); }

template <typename Entry, typename Callback>
auto Application::AddObserver(std::vector<Entry>& entries, Callback observer) -> RuntimeConnection {
    const auto id = next_observer_id_++;
    entries.push_back(Entry{id, std::move(observer)});
    return RuntimeConnection([this, &entries, id] {
        std::erase_if(entries, [id](const Entry& entry) { return entry.id == id; });
    });
}

auto Application::AddEventObserver(EventObserver observer) -> RuntimeConnection {
    return AddObserver(event_observers_, std::move(observer));
}
auto Application::AddUpdateObserver(UpdateObserver observer) -> RuntimeConnection {
    return AddObserver(update_observers_, std::move(observer));
}
auto Application::AddFixedUpdateObserver(FixedUpdateObserver observer) -> RuntimeConnection {
    return AddObserver(fixed_update_observers_, std::move(observer));
}
auto Application::AddOverlayObserver(OverlayObserver observer) -> RuntimeConnection {
    return AddObserver(overlay_observers_, std::move(observer));
}
auto Application::AddResizeObserver(ResizeObserver observer) -> RuntimeConnection {
    return AddObserver(resize_observers_, std::move(observer));
}
auto Application::AddShutdownObserver(ShutdownObserver observer) -> RuntimeConnection {
    return AddObserver(shutdown_observers_, std::move(observer));
}

auto Application::DispatchEvent(window::IEvent& event) -> void {
    input::InputSystem::GetInstance().ProcessEvent(event);
    for (const auto& observer : event_observers_)
        observer.callback(event);
    if (event.type == window::EventType::Resized || event.type == window::EventType::ResizeSettled) {
        const auto& resize = std::get<window::ResizeEvent>(event.data);
        for (const auto& observer : resize_observers_)
            observer.callback(resize.width, resize.height);
    }
    GetScreenManager().HandleEvent(event);
}

} // namespace atom::runtime
