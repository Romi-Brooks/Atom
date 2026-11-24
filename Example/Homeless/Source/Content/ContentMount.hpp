#ifndef ATOM_HOMELESS_CONTENTMOUNT_HPP
#define ATOM_HOMELESS_CONTENTMOUNT_HPP

#include <filesystem>
#include <string>

namespace homeless {

auto MountContent(const std::filesystem::path& directory, bool packaged) -> bool;
auto MountLocalMusic(const std::filesystem::path& directory) -> std::string;

} // namespace homeless

#endif // ATOM_HOMELESS_CONTENTMOUNT_HPP
