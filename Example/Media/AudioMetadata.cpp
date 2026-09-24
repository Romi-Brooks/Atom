/**
  * @file           : AudioMetadata.cpp
  * @author         : Romi Brooks
  * @brief          : Read audio tags and properties via the engine's
  *                   AudioMetadataReader (TagLib is an engine detail).
  * @attention      : Metadata is read through the VFS: the music directory is
  *                   mounted as "res://" and each file is referenced by its
  *                   asset path, never a raw native path.
  * @date           : 2026/8/20
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <iostream>
#include <string>

#include <Filesystem/FileSystem.hpp>
#include <Filesystem/Vfs.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/Metadata/AudioMetadataReader.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace {
// Relative filenames inside the mounted music directory.
constexpr const char* sample_files[] = {
    R"(我的歌声里 - 曲婉婷.mp3)",
    R"(YOASOBI - 夜に駆ける.mp3)",
    R"(Doja Cat - Say So.flac)",
};
} // namespace

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();

    // Mount the music directory as res:// so metadata reads go through the VFS
    // contract instead of a raw path.
    constexpr const char* kMusicDir = R"(E:\Music)";
    std::unique_ptr<atom::fs::NativeFileSystem> filesystem{};
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(kMusicDir), filesystem) !=
            atom::fs::Result::Success ||
        !filesystem) {
        std::cout << "Music directory unavailable: " << kMusicDir << std::endl;
        return 1;
    }
    // Register the mount on the process-wide default Vfs so the string
    // convenience entry point resolves against it.
    atom::fs::Vfs::GetInstance().Mount("res", 0, std::move(filesystem));

    for (const auto* filename : sample_files) {
        std::cout << "========================================" << std::endl;
        std::cout << filename << std::endl;
        std::cout << "========================================" << std::endl;

        const std::string asset_path = std::string{"res://"} + filename;
        const auto meta = atom::audio::AudioMetadataReader::Read(asset_path);
        if (!meta) {
            std::cout << "(no metadata)" << std::endl << std::endl;
            continue;
        }

        std::cout << "Title:    " << meta->title << std::endl;
        std::cout << "Artist:   " << meta->artist << std::endl;
        std::cout << "Album:    " << meta->album << std::endl;
        std::cout << "Genre:    " << meta->genre << std::endl;
        std::cout << "Year:     " << meta->year << std::endl;
        std::cout << "Track:    " << meta->track << std::endl;
        std::cout << "Duration: " << meta->durationSeconds << "s, " << meta->bitrateKbps << " kbps, "
                  << meta->sampleRate << " Hz, " << meta->channels << " ch" << std::endl;
        std::cout << "Artwork:  "
                  << (meta->artworkData.empty()
                          ? "none"
                          : meta->artworkMimeType + ", " + std::to_string(meta->artworkData.size()) + " bytes")
                  << std::endl;
        std::cout << std::endl;
    }

    return 0;
}
