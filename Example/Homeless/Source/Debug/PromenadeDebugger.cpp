#include "PromenadeDebugger.hpp"

#include <variant>

#include <Debugger/Panels/FpsDebugger.hpp>
#include <Debugger/Panels/JobDebugger.hpp>
#include <Debugger/Panels/LogDebugger.hpp>
#include <Debugger/Panels/RenderDebugger.hpp>
#include <Debugger/Panels/ThreadDebugger.hpp>
#include <Log/LogSystem.hpp>
#include <Runtime/Application.hpp>
#include <Window/RenderWindow.hpp>

namespace homeless {

auto RunWithDebugger(atom::runtime::Application& app, bool show_debugger, [[maybe_unused]] bool jobs_started) -> void {
#if ATOM_ENABLE_DEBUGGER
    atom::debugger::FpsDebugger fps;
    atom::debugger::JobDebugger job_panel;
    atom::debugger::RenderDebugger render;
    atom::debugger::ThreadDebugger threads;
    atom::debugger::LogDebugger logs;
    const auto set_panels = [&](bool visible) {
        fps.SetEnabled(visible);
        job_panel.SetEnabled(visible && jobs_started);
        render.SetEnabled(visible);
        threads.SetEnabled(visible);
        logs.SetEnabled(visible);
    };
    set_panels(show_debugger);
    const auto detach_panels = app.AddShutdownObserver([&] {
        logs.Detach();
        job_panel.Detach();
        threads.Detach();
        render.Detach();
        fps.Detach();
    });
    fps.Attach(app.GetWindow());
    if (jobs_started)
        job_panel.Attach(app.GetWindow());
    render.Attach(app.GetWindow());
    threads.Attach(app.GetWindow());
    logs.Attach(app.GetWindow());
    const auto toggle = app.AddEventObserver([&](const atom::window::IEvent& event) {
        const auto* key = std::get_if<atom::window::KeyEvent>(&event.data);
        if (event.type == atom::window::EventType::KeyPressed && key && !key->is_repeat &&
            key->key == atom::input::Key::F3) {
            show_debugger = !show_debugger;
            set_panels(show_debugger);
        }
    });
#else
    if (show_debugger)
        LOG_WARNING(atom::log::debugger::ImGui, "Debugger support is disabled in this build");
#endif
    app.Run();
}

} // namespace homeless
