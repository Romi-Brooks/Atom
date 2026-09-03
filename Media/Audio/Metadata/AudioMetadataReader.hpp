/**
  * @file           : AudioMetadataReader.hpp
  * @author         : Romi Brooks
  * @brief          : Engine-level audio metadata (tags + properties) reader
  *                   backed by TagLib.
  * @attention      : TagLib is an implementation detail: this module exposes
  *                   only Atom's own AudioMetadata type, so callers never
  *                   include TagLib headers directly.
  * @date           : 2026/8/20
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_AUDIO_METADATA_READER_HPP
#define ATOM_AUDIO_METADATA_READER_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace atom::audio {

// Audio file metadata: tag fields plus basic audio properties.
struct AudioMetadata {
        // Tag fields
        std::string title;
        std::string artist;
        std::string album;
        std::string comment;
        std::string genre;
        uint32_t year = 0;
        uint32_t track = 0;

        // Audio properties (0 when unavailable)
        uint32_t durationSeconds = 0;
        uint32_t bitrateKbps = 0;
        uint32_t sampleRate = 0;
        uint16_t channels = 0;

        // First embedded artwork image (usually the front cover). Empty when the
        // file carries no image. Encoded bytes are preserved so rendering backends
        // can decode PNG, JPEG, or another advertised MIME type as appropriate.
        std::string artworkMimeType;
        std::vector<uint8_t> artworkData;
};

// Reads tag and property metadata from an audio file.
// Returns nullopt when the file cannot be read or carries no tags.
class AudioMetadataReader {
    public:
        static auto Read(const std::string& path) -> std::optional<AudioMetadata>;
};

} // namespace atom::audio

#endif // ATOM_AUDIO_METADATA_READER_HPP
