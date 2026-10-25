/**
  * @file           : RiffWaveReader.hpp
  * @author         : Romi Brooks
  * @brief          : C++ WAV PCM RIFF decoder (builtin)
  * @attention      : Self-contained RIFF/WAV parser. No external dependencies.
  *                   Based on the original WavProf C decoder, rewritten in C++.
  * @date           : 2026/7/8
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_AUDIO_RIFF_WAVE_READER_HPP
#define ATOM_BACKEND_AUDIO_RIFF_WAVE_READER_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Filesystem/FileSystem.hpp>

namespace atom::backend::audio_decoder {
struct WavHeader {
        char chunk_id[4]{};         // "RIFF"
        uint32_t chunk_size = 0;    // file size - 8
        char format[4]{};           // "WAVE"
        char subchunk_id[4]{};      // "fmt "
        uint32_t subchunk_size = 0; // fmt chunk size (16 for PCM)
        uint16_t audio_format = 0;  // 1 = PCM
        uint16_t num_channels = 0;
        uint32_t sample_rate = 0;
        uint32_t byte_rate = 0;
        uint16_t block_align = 0;
        uint16_t bits_per_sample = 0;
} __attribute__((packed));

class RiffWaveReader {
    public:
        RiffWaveReader() = default;
        ~RiffWaveReader();

        RiffWaveReader(const RiffWaveReader&) = delete;
        auto operator=(const RiffWaveReader&) -> RiffWaveReader& = delete;

        // Open a WAV stream from an in-memory buffer (e.g. an entry extracted
        // from a resource pack). The buffer is borrowed: the caller must keep it
        // alive until Close(). Returns false if the data is invalid.
        [[nodiscard]] auto OpenFromMemory(const void* data, std::size_t size) -> atom::audio::DecoderOpenStatus;

        // Open a WAV stream over an already-resolved VFS file. The file is
        // borrowed: the caller must keep it alive until Close(). Random reads go
        // through IFile::ReadAt, so no native path is involved.
        [[nodiscard]] auto OpenStream(atom::fs::IFile& file) -> atom::audio::DecoderOpenStatus;

        // Close the file.
        auto Close() -> void;

        // Read the next PCM chunk. Returns bytes written to buffer, 0 = EOF.
        auto ReadChunk(uint8_t* buffer, size_t max_bytes) -> size_t;

        // Seek to the beginning of PCM data (rewind).
        auto Rewind() -> bool;

        // Seek to a byte offset inside the PCM data. The offset is clamped to the
        // available data and aligned down to a whole frame, so a caller can pass
        // frame_index * bytes_per_frame safely.
        auto SeekToByte(std::size_t byte_offset) -> bool;

        // Queries
        [[nodiscard]] auto IsOpen() const -> bool {
            return mem_data_ != nullptr || file_ != nullptr;
        }
        [[nodiscard]] auto GetChannels() const -> uint16_t {
            return channels_;
        }
        [[nodiscard]] auto GetSampleRate() const -> uint32_t {
            return sample_rate_;
        }
        [[nodiscard]] auto GetBitsPerSample() const -> uint16_t {
            return bits_per_sample_;
        }
        // 1 = uncompressed PCM, 3 = IEEE float (32-bit only).
        [[nodiscard]] auto GetAudioFormat() const -> uint16_t {
            return audio_format_;
        }
        [[nodiscard]] auto GetTotalPCMBytes() const -> size_t {
            return data_bytes_;
        }

    private:
        size_t data_start_ = 0;
        size_t data_bytes_ = 0;
        uint16_t channels_ = 0;
        uint32_t sample_rate_ = 0;
        uint16_t bits_per_sample_ = 0;
        uint16_t audio_format_ = 0;
        // Borrowed in-memory buffer (OpenFromMemory). Never owned: the caller
        // must keep it alive until Close(). mem_pos_ is the current read cursor
        // within the whole buffer.
        const uint8_t* mem_data_ = nullptr;
        size_t mem_size_ = 0;
        size_t mem_pos_ = 0;

        // Borrowed VFS file (OpenStream). Never owned: the caller must keep it
        // alive until Close(). file_pos_ is the current read cursor, and
        // file_size_ caches IFile::Size() for bounds checks.
        atom::fs::IFile* file_ = nullptr;
        uint64_t file_pos_ = 0;
        uint64_t file_size_ = 0;
};
} // namespace atom::backend::audio_decoder

#endif // ATOM_BACKEND_AUDIO_RIFF_WAVE_READER_HPP
