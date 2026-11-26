#include <Atom/Backend/Composition/BackendRuntime.hpp>
#include <Atom/Build/BackendsExport.hpp>

// Called by Atom.Runtime's process-exit shim while every DLL is still loaded.
// Quitting the SDL audio subsystem here (normal thread context) avoids the
// DllMain-time WASAPI join that used to block process shutdown forever.
extern "C" ATOM_BACKENDS_API auto AtomShutdownBackends() -> void {
    if (auto* runtime = atom::backend::BackendRuntime::TryGetInstance())
        runtime->Shutdown();
}
