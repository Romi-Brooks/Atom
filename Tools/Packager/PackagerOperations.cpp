#include "PackagerOperations.hpp"

#include <filesystem>

#include <Log/LogSystem.hpp>
#include <Utilities/Packager/PackageBuilder.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace atom::packager {
namespace {

using tools::PackageBuilder;

auto BuilderError(PackageBuilder::Result result) -> std::string_view {
    using enum PackageBuilder::Result;
    switch (result) {
        case Success: return "success";
        case ErrorOpenOutput: return "cannot open the output location";
        case ErrorReadFile: return "failed to read an input";
        case ErrorInvalidPath: return "invalid or conflicting source/output path";
        case ErrorEmptyPackage: return "no files to pack";
        case ErrorWriteFailed: return "could not publish the archive (destination locked or already exists)";
        case ErrorDuplicatePath: return "conflicting paths in the archive";
        case ErrorVerificationFailed: return "archive verification failed";
    }
    return "unknown error";
}

auto Fail(std::string message, int code = 1) -> OperationResult {
    LOG_ERROR(atom::log::utilities::Packager, message);
    return {{}, std::move(message), code};
}

} // namespace

auto Inspect(const std::string& path, const bool verify, const Progress& progress) -> OperationResult {
    std::unique_ptr<fs::PackageFileSystem> package;
    const auto opened = fs::PackageFileSystem::Create("inspect", path, package);
    if (opened != fs::Result::Success)
        return Fail(fs::ToError(opened, "Open APKG v2", path).Message());
    fs::PackageFileSystem::VerifyProgress notify;
    if (progress)
        notify = [&](std::size_t completed, std::size_t total) { progress(Phase::Verify, completed, total); };
    if (verify && package->VerifyContents(notify) != fs::Result::Success)
        return Fail("Content verification failed for " + path);

    OperationResult result;
    if (package->EnumerateAssets(result.assets) != fs::Result::Success)
        return Fail("Could not enumerate " + path);
    package->EnumerateDirectories(result.directories);
    result.message = (verify ? "Verified " : "Opened ") + std::to_string(result.assets.size()) + " assets";
    LOG_INFO(atom::log::utilities::Packager, result.message + " | " + path);
    return result;
}

auto Build(const BuildRequest& request, const Progress& progress) -> OperationResult {
    if (!IsValidUtf8(request.input) || request.input.find('\0') != std::string::npos)
        return Fail("Invalid input path");

    PackageBuilder builder;
    std::error_code error;
    const auto input = PathFromUtf8(request.input);
    auto added = PackageBuilder::Result::ErrorInvalidPath;
    if (std::filesystem::is_directory(input, error) && !error) {
        added = builder.AddDirectory(request.input, request.pack_path);
    } else if (std::filesystem::is_regular_file(input, error) && !error) {
        if (request.pack_path.empty())
            return Fail("A single-file input requires a package path", 2);
        added = builder.AddFile(request.input, request.pack_path);
    }
    if (added != PackageBuilder::Result::Success)
        return Fail("Failed to queue input: " + std::string{BuilderError(added)});

    PackageBuilder::Config config;
    config.verbose = request.verbose;
    config.overwrite = request.overwrite;
    if (progress) {
        config.progress = [&](PackageBuilder::Phase phase, std::size_t completed, std::size_t total) {
            progress(phase == PackageBuilder::Phase::Write ? Phase::Write : Phase::Verify, completed, total);
        };
    }
    const auto built = builder.Build(request.output, config);
    if (built != PackageBuilder::Result::Success)
        return Fail("APKG packing failed: " + std::string{BuilderError(built)});
    return Inspect(request.output);
}

} // namespace atom::packager
