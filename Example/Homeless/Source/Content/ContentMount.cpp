#include "ContentMount.hpp"

#include <Filesystem/Vfs.hpp>
#include <Log/LogSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace homeless {

auto MountContent(const std::filesystem::path& directory, bool packaged) -> bool {
    const auto path = atom::PathToUtf8(directory / (packaged ? "Packages/Homeless.apkg" : "Content"));
    auto& vfs = atom::fs::Vfs::GetInstance();
    const auto result = packaged ? vfs.MountPackage("res", 200, path) : vfs.MountNative("res", 200, path);
    if (result == atom::fs::Result::Success)
        return true;
    LOG_ERROR(atom::log::asset::Resource, atom::fs::ToError(result, "Mount Homeless content", path).Message());
    return false;
}

} // namespace homeless
