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
#include <vector>

#include <Concurrency/JobSystem.hpp>
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
    constexpr const char* music_dir = R"(E:\Music)";
    std::unique_ptr<atom::fs::NativeFileSystem> filesystem{};
    if (atom::fs::NativeFileSystem::Create("res", atom::PathToUtf8(music_dir), filesystem) !=
            atom::fs::Result::Success ||
        !filesystem) {
        std::cout << "Music directory unavailable: " << music_dir << std::endl;
        return 1;
    }
    // Register the mount on the process-wide default Vfs so the string
    // convenience entry point resolves against it.
    atom::fs::Vfs::GetInstance().Mount("res", 0, std::move(filesystem));

    // Resolve every file's metadata in parallel through the engine's job
    // system. Each Read() opens its own file handle (TagLib-backed, VFS-
    // mediated), so the reads are independent and safe to run concurrently;
    // scheduling them as jobs demonstrates the engine's generic task API for
    // exactly the kind of off-main-thread work that would otherwise stall a
    // frame in a real application.
    auto& jobs = atom::job::JobSystem::GetInstance();
    jobs.Configure(); // default: hardware_concurrency() - 1 workers
    jobs.Start();

    std::vector<atom::job::JobHandle> handles;
    handles.reserve(std::size(sample_files));
    std::vector<std::optional<atom::audio::AudioMetadata>> results(std::size(sample_files));
    for (std::size_t i = 0; i < std::size(sample_files); ++i) {
        handles.push_back(jobs.Schedule([i, &results] {
            const std::string asset_path = std::string{"res://"} + sample_files[i];
            results[i] = atom::audio::AudioMetadataReader::Read(asset_path);
        }));
    }
    // Block until all metadata reads finish; the main thread helps run pending
    // jobs rather than idling.
    jobs.WaitAll(handles);

    for (std::size_t i = 0; i < std::size(sample_files); ++i) {
        const auto* filename = sample_files[i];
        std::cout << "========================================" << std::endl;
        std::cout << filename << std::endl;
        std::cout << "========================================" << std::endl;

        const auto& meta = results[i];
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

    jobs.Shutdown();
    return 0;
}
