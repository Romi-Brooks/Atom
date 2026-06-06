/**
	* @file           : WavDecoder.cpp
	* @author         : Romi Brooks
	* @brief          : WAV audio file decoder implementation
	* @attention      :
	* @date           : 2026/6/6
	Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <cstring>
#include <iostream>

// Self Dependencies
#include "WavDecoder.hpp"

namespace atom::media {
    WavDecoder::~WavDecoder() {
        Close();
    }

    auto WavDecoder::Open(const std::string& filePath) -> bool {
        // Close any previously opened file
        // 关闭之前打开的文件
        Close();

        file_.open(filePath, std::ios::binary);
        if (!file_.is_open()) {
            std::cout << "Error: Failed to open file: " << filePath << std::endl;
            return false;
        }

        // Read RIFF header
        // 读取 RIFF 文件头
        RiffHeader riff {};
        if (!file_.read(reinterpret_cast<char*>(&riff), sizeof(riff))) {
            std::cout << "Error: Failed to read RIFF header" << std::endl;
            file_.close();
            return false;
        }

        // Validate RIFF magic and WAVE format
        // 验证 RIFF 魔数和 WAVE 格式
        if (std::memcmp(riff.chunk_id, kMagicRiff, 4) != 0
            || std::memcmp(riff.format, kMagicWave, 4) != 0)
        {
            std::cout << "Error: Not a valid WAV file" << std::endl;
            file_.close();
            return false;
        }

        // Iterate through sub-chunks to find "fmt " and "data"
        // 遍历子块以找到 "fmt " 和 "data"
        SubchunkHeader sub {};

        // --- Find "fmt " sub-chunk ---
        // --- 查找 "fmt " 子块 ---
        while (file_.read(reinterpret_cast<char*>(&sub), sizeof(sub))) {
            if (std::memcmp(sub.subchunk_id, kSubchunkFmt, 4) == 0) {
                FmtData fmt {};
                if (!file_.read(reinterpret_cast<char*>(&fmt), sizeof(fmt))) {
                    std::cout << "Error: Failed to read fmt chunk data" << std::endl;
                    file_.close();
                    return false;
                }

                // Skip any extra format bytes
                // 跳过额外的格式字节
                const auto extra = static_cast<long>(sub.subchunk_size) - static_cast<long>(sizeof(fmt));
                if (extra > 0) {
                    file_.seekg(extra, std::ios::cur);
                }

                // Only PCM format is supported
                // 仅支持 PCM 格式
                if (fmt.audio_format != kPcmFormat) {
                    std::cout << "Error: Non-PCM format not supported (format="
                            << fmt.audio_format << ")" << std::endl;
                    file_.close();
                    return false;
                }

                channels_ = fmt.num_channels;
                sample_rate_ = fmt.sample_rate;
                bits_per_sample_ = fmt.bits_per_sample;
                break;
            }

            // Skip non-"fmt " chunks (JUNK, LIST, etc.)
            // 跳过非 "fmt " 块
            file_.seekg(sub.subchunk_size, std::ios::cur);
        }

        if (channels_ == 0) {
            std::cout << "Error: No fmt chunk found" << std::endl;
            file_.close();
            return false;
        }

        // --- Find "data" sub-chunk ---
        // --- 查找 "data" 子块 ---
        file_.clear();
        while (file_.read(reinterpret_cast<char*>(&sub), sizeof(sub))) {
            if (std::memcmp(sub.subchunk_id, kSubchunkData, 4) == 0) {
                data_start_ = static_cast<std::size_t>(file_.tellg());
                data_bytes_ = sub.subchunk_size;
                return true;
            }
            file_.seekg(sub.subchunk_size, std::ios::cur);
        }

        std::cout << "Error: No data chunk found" << std::endl;
        file_.close();
        return false;
    }

    auto WavDecoder::Close() -> void {
        if (file_.is_open()) {
            file_.close();
        }
        data_start_ = 0;
        data_bytes_ = 0;
        channels_ = 0;
        sample_rate_ = 0;
        bits_per_sample_ = 0;
    }

    auto WavDecoder::ReadPcmChunk(std::uint8_t* buffer, std::size_t maxBytes) -> std::size_t {
        if (!file_.is_open() || buffer == nullptr || maxBytes == 0) {
            return 0;
        }

        // Calculate remaining bytes in the PCM data section
        // 计算 PCM 数据段中剩余的字节数
        const auto currentPos = static_cast<std::size_t>(file_.tellg());
        if (currentPos <= data_start_) {
            return 0;
        }
        const std::size_t bytesRead = currentPos - data_start_;
        if (bytesRead >= data_bytes_) {
            return 0;
        }

        const std::size_t remain = data_bytes_ - bytesRead;
        const std::size_t toRead = (remain < maxBytes) ? remain : maxBytes;

        file_.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(toRead));
        return static_cast<std::size_t>(file_.gcount());
    }
} // namespace atom::media
