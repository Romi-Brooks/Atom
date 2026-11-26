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

#include <Atom/AtomMain.hpp>
#include <Atom/Core/Concurrency/JobSystem.hpp>
#include <Atom/Resources/Filesystem/FileSystem.hpp>
#include <Atom/Resources/Filesystem/Vfs.hpp>
#include <Atom/Core/Log/LogSystem.hpp>
#include <Atom/Resources/Media/Audio/Metadata/AudioMetadataReader.hpp>
#include <Atom/Core/Utilities/Utf8/Utf8.hpp>

namespace {
// Relative filenames inside the mounted music directory.
constexpr const char* sample_files[] = {
    R"(我的歌声里 - 曲婉婷.mp3)",
    R"(YOASOBI - 夜に駆ける.mp3)",
    R"(Doja Cat - Say So.flac)",
};
} // namespace

auto atom::platform::ApplicationMain(int, char**) -> int {
    atom::Log::SetConsoleOutputUtf8();

    // Mount the music directory as res:// so metadata reads go through the VFS
    // contract instead of a raw path.
    constexpr const char* music_dir = R"(E:\Music)";
    if (atom::fs::Vfs::GetInstance().MountNative("res", 0, atom::PathToUtf8(music_dir)) != atom::fs::Result::Success) {
        std::cout << "Music directory unavailable: " << music_dir << std::endl;
        return 1;
    }

    // Resolve every file's metadata in parallel through the engine's job
    // system. ReadBatch schedules one job per file and blocks until all finish;
    // the parallelism now lives in the engine rather than being hand-rolled
    // here, so the example just declares *what* to read and lets the engine
    // decide *how* to spread it across workers.
    auto& jobs = atom::job::JobSystem::GetInstance();
    jobs.Configure(); // default: hardware_concurrency() - 1 workers
    jobs.Start();

    std::vector<std::string> asset_paths;
    asset_paths.reserve(std::size(sample_files));
    for (const auto* file : sample_files) {
        asset_paths.emplace_back(std::string{"res://"} + file);
    }
    const auto results = atom::audio::AudioMetadataReader::ReadBatch(asset_paths);

    for (std::size_t i = 0; i < std::size(sample_files); ++i) {
        const auto* filename = sample_files[i];
        std::cout << "========================================" << std::endl;
        std::cout << filename << std::endl;
        std::cout << "========================================" << std::endl;

        const auto& meta = results[i];
        if (!meta) {
            // Result carries the reason: "file missing" and "no tags" are
            // different problems, and the example does not have to guess.
            std::cout << "(no metadata: " << meta.error().Message() << ")" << std::endl << std::endl;
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
