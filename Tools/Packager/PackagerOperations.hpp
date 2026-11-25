#pragma once

#include <string>
#include <functional>
#include <vector>

#include <Atom/Resources/Filesystem/PackageFileSystem.hpp>

namespace atom::packager {

struct BuildRequest {
    std::string input;
    std::string output;
    std::string pack_path;
    bool overwrite = true;
    bool verbose = false;
};

struct OperationResult {
    std::vector<fs::PackagedAssetInfo> assets;
    std::string message;
    int exit_code = 0;
    std::vector<std::string> directories;
};

enum class Phase { Write, Verify };
using Progress = std::function<void(Phase, std::size_t completed, std::size_t total)>;

[[nodiscard]] auto Inspect(const std::string& path, bool verify = false, const Progress& progress = {}) -> OperationResult;
[[nodiscard]] auto Build(const BuildRequest& request, const Progress& progress = {}) -> OperationResult;

} // namespace atom::packager
