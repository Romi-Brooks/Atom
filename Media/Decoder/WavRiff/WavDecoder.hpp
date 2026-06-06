/**
	* @file           : WavDecoder.hpp
	* @author         : Romi Brooks
	* @brief          : WAV audio file decoder (RIFF PCM format)
	* @attention      :
	* @date           : 2026/6/6
	Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_WAV_DECODER_HPP
#define ATOM_WAV_DECODER_HPP

// Standard Library
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace atom::media {
// Supports standard RIFF/WAV PCM format.
// WAV 音频文件解码器，支持标准 RIFF/WAV PCM 格式。
class WavDecoder final {
    public:
        WavDecoder() = default;
        ~WavDecoder();

        WavDecoder(const WavDecoder&) = delete;
        auto operator=(const WavDecoder&) -> WavDecoder& = delete;

        // Open a WAV file and parse headers.
        // 打开 WAV 文件并解析头部信息。
        auto Open(const std::string& filePath) -> bool;

        // Close the currently opened file.
        // 关闭当前已打开的文件。
        auto Close() -> void;

        // Read a chunk of PCM data into the provided buffer.
        // 读取一段 PCM 数据到提供的缓冲区。
        auto ReadPcmChunk(std::uint8_t* buffer, std::size_t maxBytes) -> std::size_t;

        [[nodiscard]] auto GetChannels() const -> std::uint16_t { return channels_; }
        [[nodiscard]] auto GetSampleRate() const -> std::uint32_t { return sample_rate_; }
        [[nodiscard]] auto GetBitsPerSample() const -> std::uint16_t { return bits_per_sample_; }
        [[nodiscard]] auto IsOpen() const -> bool { return file_.is_open(); }

    private:
        // RIFF file header
        // RIFF 文件头
        struct RiffHeader {
            char chunk_id[4];       // "RIFF"
            std::uint32_t chunk_size;
            char format[4];         // "WAVE"
        };

        // Sub-chunk header
        // 子块头
        struct SubchunkHeader {
            char subchunk_id[4];
            std::uint32_t subchunk_size;
        };

        // Format sub-chunk data
        // 格式子块数据
        struct FmtData {
            std::uint16_t audio_format;
            std::uint16_t num_channels;
            std::uint32_t sample_rate;
            std::uint32_t byte_rate;
            std::uint16_t block_align;
            std::uint16_t bits_per_sample;
        };

        std::ifstream file_;
        std::size_t data_start_ = 0;    // Offset where PCM data begins PCM 数据起始偏移
        std::size_t data_bytes_ = 0;    // Total PCM data size PCM 数据总大小
        std::uint16_t channels_ = 0;
        std::uint32_t sample_rate_ = 0;
        std::uint16_t bits_per_sample_ = 0;

        // Magic numbers for header validation
        // 用于头部验证的魔数
        static constexpr char kMagicRiff[4] = {'R', 'I', 'F', 'F'};
        static constexpr char kMagicWave[4] = {'W', 'A', 'V', 'E'};
        static constexpr char kSubchunkFmt[4] = {'f', 'm', 't', ' '};
        static constexpr char kSubchunkData[4] = {'d', 'a', 't', 'a'};

        static constexpr std::uint16_t kPcmFormat = 1;
};

} // namespace atom::media

#endif // ATOM_WAV_DECODER_HPP
