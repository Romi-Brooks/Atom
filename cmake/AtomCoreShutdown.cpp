#include <Atom/Build/CoreExport.hpp>
#include <Atom/Core/Concurrency/JobSystem.hpp>
#include <Atom/Core/Log/ProcessShutdown.hpp>

extern "C" ATOM_CORE_API auto AtomShutdownCore() -> void {
    atom::job::JobSystem::GetInstance().Shutdown();
    atom::detail::ShutdownLogForProcessExit();
}
