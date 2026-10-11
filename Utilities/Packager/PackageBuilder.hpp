/**
  * @file           : PackageBuilder.hpp
  * @brief          : APKG package writer.
  * @attention      : Writes the layout defined in Filesystem/PackageFormat.hpp;
  *                   never restate format constants here. The v1 Packager class
  *                   stays untouched for compatibility.
  *
  * Build pipeline: AddFile/AddDirectory collect inputs -> Build validates the
  * whole plan (duplicate AssetPaths fail before any byte is written) -> writes
  * a temporary file -> re-reads and verifies header/manifest/chunk hashes ->
  * atomically replaces the target. A failed build never leaves a half-written
  * package behind.
  * @date           : 2026/10/7
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_PACKAGE_BUILDER_HPP
#define ATOM_PACKAGE_BUILDER_HPP

// Standard Library
#include <string>
#include <vector>

namespace atom::tools {

class PackageBuilder {
    public:
        struct Config {
                bool verbose = false;
                bool overwrite = true;

                Config();
        };

        enum class Result {
            Success,
            ErrorOpenOutput,
            ErrorReadFile,
            ErrorInvalidPath,
            ErrorEmptyPackage,
            ErrorWriteFailed,
            ErrorDuplicatePath,
            ErrorVerificationFailed,
        };

        PackageBuilder() = default;
        ~PackageBuilder() = default;

        PackageBuilder(const PackageBuilder&) = delete;
        PackageBuilder& operator=(const PackageBuilder&) = delete;

        // Queues one native file under an explicit package path
        // ("textures/ui/button.png"). The package path must satisfy
        // package_format::IsValidPackagePath.
        auto AddFile(const std::string& native_path_utf8, const std::string& package_path) -> Result;

        // Recursively queues a native directory; package paths are the
        // UTF-8 '/'-separated paths relative to the directory root, optionally
        // under a package prefix ("audio" -> "audio/song.wav").
        auto AddDirectory(const std::string& native_dir_utf8, const std::string& package_prefix = "") -> Result;

        // Writes the package. Validation of the full plan happens before any
        // byte hits the disk; the output file is replaced atomically on
        // success and left untouched on failure.
        auto Build(const std::string& output_utf8, const Config& config = Config()) -> Result;

        [[nodiscard]] auto PlannedFiles() const -> std::vector<std::string>;

    private:
        struct PlannedFile {
                std::string native_path;
                std::string package_path;
        };

        std::vector<PlannedFile> files_{};
};

} // namespace atom::tools

#endif // ATOM_PACKAGE_BUILDER_HPP
