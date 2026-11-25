#include <Atom/Build/BackendsExport.hpp>
#include <Atom/Build/RuntimeExport.hpp>
#include <Atom/Runtime/Application.hpp>
#include <Atom/Runtime/ScreenManager.hpp>

extern "C" ATOM_BACKENDS_API auto AtomShutdownBackends() -> void;

extern "C" ATOM_RUNTIME_API auto AtomShutdownRuntime() -> void {
    // Screens own the playback sources, so release them (and the window) first;
    // then stop the audio backend while SDL is still safely callable.
    atom::runtime::Application::GetInstance().Shutdown();
    atom::ScreenManager::GetInstance().ClearScreens();
    AtomShutdownBackends();
}
