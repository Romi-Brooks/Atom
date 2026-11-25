#include <Atom/Core/Concurrency/JobSystem.hpp>
#include <Atom/Core/Log/ProcessShutdown.hpp>
#include <Atom/Runtime/Application.hpp>
#include <Atom/Runtime/ScreenManager.hpp>

extern "C" ATOM_CORE_API auto AtomShutdownProcess() -> void {
    atom::runtime::Application::GetInstance().Shutdown();
    atom::ScreenManager::GetInstance().ClearScreens();
    atom::job::JobSystem::GetInstance().Shutdown();
    atom::detail::ShutdownLogForProcessExit();
}
