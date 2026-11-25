#include <Atom/Build/RuntimeExport.hpp>
#include <Atom/Runtime/Application.hpp>
#include <Atom/Runtime/ScreenManager.hpp>

extern "C" ATOM_RUNTIME_API auto AtomShutdownRuntime() -> void {
    atom::runtime::Application::GetInstance().Shutdown();
    atom::ScreenManager::GetInstance().ClearScreens();
}
