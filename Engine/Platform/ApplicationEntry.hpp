#ifndef ATOM_PLATFORM_APPLICATIONENTRY_HPP
#define ATOM_PLATFORM_APPLICATIONENTRY_HPP

namespace atom::platform {

// Implement this function in each executable configured with atom_use_application_entry.
auto ApplicationMain(int argc, char** argv) -> int;

} // namespace atom::platform

#endif // ATOM_PLATFORM_APPLICATIONENTRY_HPP
