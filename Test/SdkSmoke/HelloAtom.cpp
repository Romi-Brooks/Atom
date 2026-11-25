#include <Atom/AtomMain.hpp>
#include <Atom/Core/Log/LogSystem.hpp>

auto atom::platform::ApplicationMain(int argc, char** argv) -> int {
    if (argc < 1 || !argv || argv[argc] != nullptr) {
        return 1;
    }

    atom::Log::SetConsoleSinkEnabled(true);
    LOG_INFO(atom::log::core::Main, "Atom SDK smoke reached exit");
    return 0;
}
