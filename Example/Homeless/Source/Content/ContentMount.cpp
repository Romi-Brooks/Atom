#include "ContentMount.hpp"

#include <cstdlib>

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

auto MountLocalMusic(const std::filesystem::path& directory) -> std::string {
    std::filesystem::path path;
#ifdef _WIN32
    if (const auto* custom = _wgetenv(L"HOMELESS_MUSIC_FILE"); custom && *custom)
        path = custom;
#else
    if (const auto* custom = std::getenv("HOMELESS_MUSIC_FILE"); custom && *custom)
        path = atom::PathFromUtf8(custom);
#endif
    std::error_code error;
    if (path.empty()) {
        // The song is not part of the repository: it lives next to the staged
        // content. HOMELESS_MUSIC_FILE points at another local file instead.
        path = directory / atom::PathFromUtf8("Content/Audio/July - 바람에 쓰는 편지.mp3");
    }
    path = std::filesystem::absolute(path, error);
    if (error || !std::filesystem::is_regular_file(path, error)) {
        LOG_INFO(atom::log::audio::Music, "Local track unavailable: " + atom::PathToUtf8(path));
        return {};
    }

    if (atom::fs::Vfs::GetInstance().MountNative("localmusic", 200, atom::PathToUtf8(path.parent_path())) !=
        atom::fs::Result::Success)
        return {};
    return "localmusic://" + atom::PathToUtf8(path.filename());
}

} // namespace homeless
