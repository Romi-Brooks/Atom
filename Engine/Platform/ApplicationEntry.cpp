#include "ApplicationEntry.hpp"

#ifdef ATOM_WINDOWS_GUI_ENTRY
#include <SDL3/SDL_main.h>
#endif

auto main(const int argc, char** argv) -> int {
    return atom::platform::ApplicationMain(argc, argv);
}
