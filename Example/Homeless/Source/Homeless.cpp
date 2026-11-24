#include <charconv>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#ifdef ATOM_WINDOWS_GUI_ENTRY
#include <SDL3/SDL_main.h>
#endif

#include <Concurrency/JobSystem.hpp>
#include <Log/LogSystem.hpp>
#include <Platform/ApplicationPaths.hpp>
#include <Runtime/Application.hpp>
#include <Runtime/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>

#include "Content/ContentMount.hpp"
#include "Debug/PromenadeDebugger.hpp"
#include "Scene/PromenadeScreen.hpp"

namespace {

struct JobSession final {
        atom::job::JobSystem& scheduler;
        ~JobSession() {
            scheduler.Shutdown();
        }
};

} // namespace

auto main(int argc, char** argv) -> int {
    bool packaged = false;
    bool show_debugger = false;
    auto window_mode = atom::window::WindowMode::Windowed;
    unsigned frame_limit = 0;
    auto driver = atom::backend::GpuDriver::Auto;
    for (int i = 1; i < argc; ++i) {
        const std::string_view option{argv[i]};
        if (option == "--package")
            packaged = true;
        else if (option == "--debugger")
            show_debugger = true;
        else if (option == "--windowed")
            window_mode = atom::window::WindowMode::Windowed;
        else if (option == "--fullscreen")
            window_mode = atom::window::WindowMode::ExclusiveFullscreen;
        else if (option == "--vulkan")
            driver = atom::backend::GpuDriver::Vulkan;
        else if (option.starts_with("--frames=")) {
            const auto value = option.substr(9);
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), frame_limit);
            if (error != std::errc{} || end != value.data() + value.size() || frame_limit == 0) {
                std::cerr << "--frames requires a positive integer\n";
                return 2;
            }
        } else {
            std::cerr << "Usage: Homeless [--package] [--vulkan] [--debugger] [--windowed|--fullscreen] [--frames=N]\n";
            return 2;
        }
    }

    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_INFO);
    const auto base = atom::platform::ApplicationBasePath();
    if (!base) {
        LOG_ERROR(atom::log::core::Main, base.error().Message());
        return 1;
    }
    const auto& directory = *base;
    if (!homeless::MountContent(directory, packaged))
        return 1;
    const auto local_music = homeless::MountLocalMusic(directory);

    auto& app = atom::runtime::Application::GetInstance();
    if (const auto started = app.Initialize({
            .title = "Homeless",
            .resolution = {1120.0f, 630.0f},
            .gpu_driver = driver,
            .window_mode = window_mode,
            .present_mode = atom::render::PresentMode::Immediate,
            .frames_in_flight = 3,
        });
        !started) {
        LOG_ERROR(atom::log::core::Main, started.error().Message());
        return 1;
    }
    app.GetWindow().SetFPS(0);

    bool render_failed = false;
    auto& screens = app.GetScreenManager();
    auto* device = app.GetWindow().GetRenderDevice();
    if (!device) {
        app.Shutdown();
        return 1;
    }

    auto& scheduler = atom::job::JobSystem::GetInstance();
    scheduler.Configure(std::thread::hardware_concurrency() > 2 ? 2 : 1);
    const JobSession jobs{scheduler};
    const bool jobs_started = scheduler.Start();
    if (!jobs_started)
        LOG_WARNING(atom::log::concurrency::JobSystem, "Homeless is loading assets synchronously");

    auto promenade =
        homeless::PromenadeScreen::Create(*device, directory / "Shaders", frame_limit, render_failed, local_music);
    if (!promenade) {
        app.Shutdown();
        return 1;
    }
    const auto loaded = screens.LoadScreen("Promenade", std::move(promenade));
    if (loaded != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::core::ScreenManager, "Cannot register Promenade: " + std::string{atom::ToString(loaded)});
        app.Shutdown();
        return 1;
    }
    const auto switched = screens.SwitchScreen("Promenade");
    if (switched != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::core::ScreenManager,
                  "Cannot activate Promenade: " + std::string{atom::ToString(switched)});
        app.Shutdown();
        return 1;
    }
    homeless::RunWithDebugger(app, show_debugger, jobs_started);
    if (frame_limit != 0 && jobs_started) {
        const auto stats = scheduler.Stats();
        LOG_INFO(atom::log::concurrency::JobSystem,
                 "Homeless jobs submitted/completed: " + std::to_string(stats.submitted) + '/' +
                     std::to_string(stats.completed));
    }
    return render_failed ? 1 : 0;
}
