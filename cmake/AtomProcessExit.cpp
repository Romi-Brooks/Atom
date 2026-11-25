#include <Atom/Build/CoreExport.hpp>

extern "C" ATOM_CORE_API auto AtomShutdownProcess() -> void;

namespace {
struct AtomProcessExit {
        ~AtomProcessExit() {
            AtomShutdownProcess();
        }
};

const AtomProcessExit process_exit;
} // namespace
