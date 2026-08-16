/**
  * @file           : AudioMetadataReader.cpp
  * @author         : Romi Brooks
  * @brief          : TagLib-backed implementation of AudioMetadataReader.
  * @attention      : TagLib headers are confined to this translation unit.
  * @date           : 2026/8/20
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "AudioMetadataReader.hpp"

#include <fileref.h>
#include <tag.h>

#include <Log/LogSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace atom {

auto AudioMetadataReader::Read(const std::string& path) -> std::optional<AudioMetadata> {
    try {
#ifdef _WIN32
        // TagLib's narrow-path constructor converts with the ANSI code page
        // (CP_ACP), which corrupts UTF-8 paths with non-ASCII characters.
        // Pass a wide path instead: FileName(const wchar_t*) stores it as
        // UTF-16 untouched.
        const auto wide_path = atom::Utf8ToWide(path);
        if (wide_path.empty()) {
            LOG_WARNING(LogChannel::ATOM_AUDIO_METADATA, "Failed to convert path to UTF-16: " + path);
            return std::nullopt;
        }
        TagLib::FileRef file(wide_path.c_str());
#else
        TagLib::FileRef file(path.c_str());
#endif
        if (file.isNull() || !file.tag()) {
            LOG_WARNING(LogChannel::ATOM_AUDIO_METADATA, "No metadata found: " + path);
            return std::nullopt;
        }

        AudioMetadata meta;
        const auto* tag = file.tag();
        meta.title = tag->title().to8Bit(true);
        meta.artist = tag->artist().to8Bit(true);
        meta.album = tag->album().to8Bit(true);
        meta.comment = tag->comment().to8Bit(true);
        meta.genre = tag->genre().to8Bit(true);
        meta.year = tag->year();
        meta.track = tag->track();

        if (file.audioProperties()) {
            const auto* props = file.audioProperties();
            meta.durationSeconds = static_cast<uint32_t>(props->lengthInSeconds());
            meta.bitrateKbps = static_cast<uint32_t>(props->bitrate());
            meta.sampleRate = static_cast<uint32_t>(props->sampleRate());
            meta.channels = static_cast<uint16_t>(props->channels());
        }

        LOG_INFO(LogChannel::ATOM_AUDIO_METADATA, "Read metadata: " + path + " (title='" + meta.title +
                                                     "', artist='" + meta.artist + "', duration=" +
                                                     std::to_string(meta.durationSeconds) + "s)");
        return meta;
    } catch (...) {
        LOG_WARNING(LogChannel::ATOM_AUDIO_METADATA, "Failed to read metadata: " + path);
        return std::nullopt;
    }
}

} // namespace atom
