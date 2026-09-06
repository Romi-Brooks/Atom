/**
  * @file           : AudioMetadata.cpp
  * @author         : Romi Brooks
  * @brief          : Read audio tags and properties via the engine's
  *                   AudioMetadataReader (TagLib is an engine detail).
  * @attention      :
  * @date           : 2026/8/20
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include <iostream>
#include <string>

#include <Media/Audio/Metadata/AudioMetadataReader.hpp>
#include <Log/LogSystem.hpp>

namespace {
constexpr const char* kSampleFiles[] = {
    // replace it
    R"(E:\Music\我的歌声里 - 曲婉婷.mp3)", R"(E:\Music\滴滴 - 覆予.mp3)",  R"(E:\Music\YOASOBI - 夜に駆ける.mp3)",
    R"(E:\Music\Doja Cat - Say So.flac)",  R"(E:\Music\Glorb - LOIS.mp3)",
};
} // namespace

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();
    for (const auto* path : kSampleFiles) {
        std::cout << "========================================" << std::endl;
        std::cout << path << std::endl;
        std::cout << "========================================" << std::endl;

        const auto meta = atom::audio::AudioMetadataReader::Read(path);
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
