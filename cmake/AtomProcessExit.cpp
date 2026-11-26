#include <Atom/Build/CoreExport.hpp>

#if defined(ATOM_SDK_RUNTIME_EXIT)
#include <Atom/Build/RuntimeExport.hpp>
extern "C" ATOM_RUNTIME_API auto AtomShutdownRuntime() -> void;
#endif

extern "C" ATOM_CORE_API auto AtomShutdownCore() -> void;

namespace {
struct AtomProcessExit {
        ~AtomProcessExit() {
#if defined(ATOM_SDK_RUNTIME_EXIT)
            AtomShutdownRuntime();
#endif
            AtomShutdownCore();
        }
};

const AtomProcessExit process_exit;
} // namespace
