#ifndef ATOM_HOMELESS_CONTENTMOUNT_HPP
#define ATOM_HOMELESS_CONTENTMOUNT_HPP

#include <filesystem>

namespace homeless {

auto MountContent(const std::filesystem::path& directory, bool packaged) -> bool;

} // namespace homeless

#endif // ATOM_HOMELESS_CONTENTMOUNT_HPP
