/**
  * @file           : Minimp3Decoder.cpp
  * @author         : Romi Brooks
  * @brief          : Streaming MP3 decoder backed by minimp3 (lieff/minimp3)
  * @attention      : MINIMP3_IMPLEMENTATION must be defined in exactly this
  *                   translation unit; the C implementation is compiled with C
  *                   linkage so it matches minimp3's extern "C" declarations.
  * @date           : 2026/8/15
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "Minimp3Decoder.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

#define MINIMP3_IMPLEMENTATION
extern "C" {
#include <minimp3_ex.h>
}

#include <Log/LogSystem.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace atom {
namespace {

// minimp3 callback-I/O adapters over a FILE* kept open in binary mode. The
// file stays open and is read in bounded chunks, matching the engine's
// streaming decoder contract.
auto Mp3ReadCallback(void* buffer, const size_t size, void* user_data) -> size_t {
    return std::fread(buffer, 1, size, static_cast<FILE*>(user_data));
}

auto Mp3SeekCallback(const uint64_t position, void* user_data) -> int {
    auto* stream = static_cast<FILE*>(user_data);
#ifdef _WIN32
    return _fseeki64(stream, static_cast<__int64>(position), SEEK_SET) == 0 ? 0 : -1;
#else
    return fseeko(stream, static_cast<off_t>(position), SEEK_SET) == 0 ? 0 : -1;
#endif
}

} // namespace

struct Minimp3Decoder::Impl {
    mp3dec_ex_t dec{};
    mp3dec_io_t io{};
    FILE* stream = nullptr;
};

Minimp3Decoder::Minimp3Decoder() : impl_(std::make_unique<Impl>()) {}

Minimp3Decoder::~Minimp3Decoder() {
    Close();
}

auto Minimp3Decoder::Open(const std::string& path) -> bool {
    Close();

#ifdef _WIN32
    // Wide open so UTF-8 paths with non-ASCII characters (e.g. Chinese) work.
    // std::ifstream::open(const wchar_t*) is unavailable in this toolchain
    // (no _GLIBCXX_HAVE__WFOPEN), so use _wfopen like RiffWaveReader does.
    const auto wide_path = atom::Utf8ToWide(path);
    if (wide_path.empty()) {
        LOG_ERROR(LogChannel::ATOM_AUDIO_MINIMP3, "Minimp3: failed to convert path to UTF-16: " + path);
        return false;
    }
    impl_->stream = _wfopen(wide_path.c_str(), L"rb");
#else
    impl_->stream = std::fopen(path.c_str(), "rb");
#endif
    if (!impl_->stream) {
        LOG_ERROR(LogChannel::ATOM_AUDIO_MINIMP3, "Minimp3: failed to open file: " + path);
        return false;
    }

    impl_->io.read = Mp3ReadCallback;
    impl_->io.read_data = impl_->stream;
    impl_->io.seek = Mp3SeekCallback;
    impl_->io.seek_data = impl_->stream;

    // MP3D_DO_NOT_SCAN: open fast without scanning the whole file for a frame
    // index; MP3D_SEEK_TO_SAMPLE: mp3dec_ex_seek(0) lands on the first frame.
    const int open_result = mp3dec_ex_open_cb(&impl_->dec, &impl_->io, MP3D_SEEK_TO_SAMPLE | MP3D_DO_NOT_SCAN);
    if (open_result != 0) {
        LOG_ERROR(LogChannel::ATOM_AUDIO_MINIMP3,
                  "Minimp3: failed to open stream (error " + std::to_string(open_result) + "): " + path);
        Close();
        return false;
    }

    const auto& frame_info = impl_->dec.info;
    if (frame_info.hz == 0 || frame_info.channels == 0 || frame_info.channels > 2) {
        LOG_ERROR(LogChannel::ATOM_AUDIO_MINIMP3, "Minimp3: no valid MPEG audio frame found: " + path);
        Close();
        return false;
    }

    current_path_ = path;
    info_.sample_rate = static_cast<uint32_t>(frame_info.hz);
    info_.channels = static_cast<uint16_t>(frame_info.channels);
    info_.bits_per_sample = 16; // minimp3 always outputs interleaved s16
    info_.is_float = false;
    info_.total_pcm_frames = 0; // unknown unless the VBR tag was detected
    const auto detected_samples = impl_->dec.detected_samples > 0 ? impl_->dec.detected_samples : impl_->dec.samples;
    if (detected_samples > 0 && info_.channels > 0)
        info_.total_pcm_frames = detected_samples / info_.channels;

    LOG_DEBUG(LogChannel::ATOM_AUDIO_MINIMP3, "Minimp3: MP3 stream opened: " + path);
    return true;
}

auto Minimp3Decoder::Close() -> void {
    mp3dec_ex_close(&impl_->dec);
    if (impl_->stream) {
        std::fclose(impl_->stream);
        impl_->stream = nullptr;
    }
    scratch_.clear();
    current_path_.clear();
    info_ = {};
}

auto Minimp3Decoder::DecodeChunk(uint8_t* output, const uint32_t max_bytes) -> uint32_t {
    if (!IsOpen() || !output || max_bytes == 0)
        return 0;

    const auto channels = static_cast<std::size_t>(info_.channels);
    const auto bytes_per_sample = sizeof(mp3d_sample_t); // int16
    const auto bytes_per_frame = channels * bytes_per_sample;
    if (channels == 0 || bytes_per_frame == 0 || max_bytes < bytes_per_frame)
        return 0;

    const auto max_frames = static_cast<std::size_t>(max_bytes) / bytes_per_frame;
    const auto requested_samples = max_frames * channels;

    // mp3dec_ex_read writes int16 samples; decode into an aligned scratch and
    // copy out so the possibly-unaligned output buffer is never written to
    // through an int16 pointer.
    const auto scratch_bytes = requested_samples * bytes_per_sample;
    if (scratch_.size() < scratch_bytes)
        scratch_.resize(scratch_bytes);

    const auto decoded_samples =
        mp3dec_ex_read(&impl_->dec, reinterpret_cast<mp3d_sample_t*>(scratch_.data()), requested_samples);
    if (decoded_samples == 0) {
        if (impl_->dec.last_error != 0) {
            LOG_ERROR(LogChannel::ATOM_AUDIO_MINIMP3, "Minimp3: decode error (error " +
                                                        std::to_string(impl_->dec.last_error) + "): " + current_path_);
            Close(); // lets the source distinguish this from a normal EOF
        }
        return 0;
    }

    const auto decoded_bytes = decoded_samples * bytes_per_sample;
    std::memcpy(output, scratch_.data(), decoded_bytes);
    return static_cast<uint32_t>(decoded_bytes);
}

auto Minimp3Decoder::Rewind() -> bool {
    if (!IsOpen())
        return false;
    return mp3dec_ex_seek(&impl_->dec, 0) == 0;
}

auto Minimp3Decoder::GetInfo() const -> const DecoderInfo& {
    return info_;
}

auto Minimp3Decoder::IsOpen() const -> bool {
    return impl_->stream != nullptr;
}

} // namespace atom
