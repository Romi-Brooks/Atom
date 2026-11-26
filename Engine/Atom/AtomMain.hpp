#ifndef ATOM_ATOM_MAIN_HPP
#define ATOM_ATOM_MAIN_HPP

namespace atom::platform {

// Implement this function in an executable configured with atom_application.
auto ApplicationMain(int argc, char** argv) -> int;

} // namespace atom::platform

#endif // ATOM_ATOM_MAIN_HPP
