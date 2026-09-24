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
#include <cstring>

#define MINIMP3_IMPLEMENTATION
extern "C" {
#include <minimp3_ex.h>
}

#include <Log/LogSystem.hpp>

namespace atom::backend::audio_decoder {
namespace {
// Borrowed in-memory buffer (OpenFromMemory). Never owned: the caller must
// keep it alive until Close(). pos is the current read cursor.
struct MemoryStream {
    const uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t pos = 0;
};

// Borrowed VFS file (OpenStream). Never owned: the caller must keep it alive
// until Close(). pos is the current read cursor within the whole file.
struct FileStream {
    atom::fs::IFile* file = nullptr;
    std::uint64_t pos = 0;
    std::uint64_t size = 0;
};

// minimp3 callback-I/O adapters over an IFile. ReadAt does not move IFile's own
// cursor, so the stream owns the position. Reads are clamped to the file size
// so a short final block reads its remainder instead of failing out of range
// (minimp3 treats a short read as EOF).
auto Mp3FileReadCallback(void* buffer, const size_t size, void* user_data) -> size_t {
    auto* stream = static_cast<FileStream*>(user_data);
    const auto remaining = stream->size > stream->pos ? stream->size - stream->pos : 0;
    const auto to_read = size < remaining ? size : static_cast<size_t>(remaining);
    if (to_read == 0)
        return 0;
    const auto destination = std::span<std::byte>{static_cast<std::byte*>(buffer), to_read};
    if (stream->file->ReadAt(stream->pos, destination) != atom::fs::Result::Success)
        return 0;
    stream->pos += to_read;
    return to_read;
}

auto Mp3FileSeekCallback(const uint64_t position, void* user_data) -> int {
    auto* stream = static_cast<FileStream*>(user_data);
    stream->pos = position;
    return 0;
}

// minimp3 callback-I/O adapters over a borrowed in-memory buffer.
auto Mp3MemReadCallback(void* buffer, const size_t size, void* user_data) -> size_t {
    auto* stream = static_cast<MemoryStream*>(user_data);
    const auto remaining = stream->size - stream->pos;
    const auto to_copy = size < remaining ? size : remaining;
    if (to_copy > 0) {
        std::memcpy(buffer, stream->data + stream->pos, to_copy);
        stream->pos += to_copy;
    }
    return to_copy;
}

auto Mp3MemSeekCallback(const uint64_t position, void* user_data) -> int {
    auto* stream = static_cast<MemoryStream*>(user_data);
    if (position > stream->size)
        return -1;
    stream->pos = static_cast<std::size_t>(position);
    return 0;
}

// Shared validation for the decoded MPEG frame info:
// false when the stream contains no valid MPEG audio frame.
auto ValidateMp3Frame(const mp3dec_frame_info_t& frame_info) -> bool {
    return frame_info.hz != 0 && frame_info.channels != 0 && frame_info.channels <= 2;
}

} // namespace

struct Minimp3Decoder::Impl {
    mp3dec_ex_t dec{};
    mp3dec_io_t io{};
    MemoryStream mem{};
    FileStream file{};
};

Minimp3Decoder::Minimp3Decoder() : impl_(std::make_unique<Impl>()) {}

Minimp3Decoder::~Minimp3Decoder() {
    Close();
}

auto Minimp3Decoder::OpenFromMemory(const void* data, const std::size_t size) -> atom::audio::DecoderOpenStatus {
    Close();

    if (!data || size == 0) {
        LOG_DEBUG(atom::log::audio::Minimp3, "Minimp3: invalid in-memory buffer");
        return atom::audio::DecoderOpenStatus::IoError;
    }

    impl_->mem.data = static_cast<const uint8_t*>(data);
    impl_->mem.size = size;
    impl_->mem.pos = 0;

    impl_->io.read = Mp3MemReadCallback;
    impl_->io.read_data = &impl_->mem;
    impl_->io.seek = Mp3MemSeekCallback;
    impl_->io.seek_data = &impl_->mem;

    // Same flags as Open(): the open-time scan is what makes the total length
    // (and therefore seek/duration) available. See the note in Open().
    const int open_result = mp3dec_ex_open_cb(&impl_->dec, &impl_->io, MP3D_SEEK_TO_SAMPLE);
    if (open_result != 0) {
        LOG_DEBUG(atom::log::audio::Minimp3,
                  "Minimp3: failed to open memory stream (error " + std::to_string(open_result) + ")");
        Close();
        return atom::audio::DecoderOpenStatus::InvalidData;
    }

    const auto& frame_info = impl_->dec.info;
    if (!ValidateMp3Frame(frame_info)) {
        LOG_DEBUG(atom::log::audio::Minimp3, "Minimp3: no valid MPEG audio frame found in memory buffer");
        Close();
        return atom::audio::DecoderOpenStatus::InvalidData;
    }

    current_path_ = "(memory)";
    info_.sample_rate = static_cast<uint32_t>(frame_info.hz);
    info_.channels = static_cast<uint16_t>(frame_info.channels);
    info_.bits_per_sample = 16; // minimp3 always outputs interleaved s16
    info_.is_float = false;
    info_.total_pcm_frames = 0; // unknown unless the VBR tag was detected
    const auto detected_samples = impl_->dec.detected_samples > 0 ? impl_->dec.detected_samples : impl_->dec.samples;
    if (detected_samples > 0 && info_.channels > 0)
        info_.total_pcm_frames = detected_samples / info_.channels;

    LOG_DEBUG(atom::log::audio::Minimp3,
              "Minimp3: MP3 memory stream opened (" + std::to_string(size) + " bytes)");
    return atom::audio::DecoderOpenStatus::Opened;
}

auto Minimp3Decoder::OpenStream(atom::fs::IFile& file) -> atom::audio::DecoderOpenStatus {
    Close();

    impl_->file.file = &file;
    impl_->file.pos = 0;
    impl_->file.size = file.Size();

    impl_->io.read = Mp3FileReadCallback;
    impl_->io.read_data = &impl_->file;
    impl_->io.seek = Mp3FileSeekCallback;
    impl_->io.seek_data = &impl_->file;

    // Same flags as Open(): the open-time scan is what makes the total length
    // (and therefore seek/duration) available. See the note in Open().
    const int open_result = mp3dec_ex_open_cb(&impl_->dec, &impl_->io, MP3D_SEEK_TO_SAMPLE);
    if (open_result != 0) {
        LOG_DEBUG(atom::log::audio::Minimp3,
                  "Minimp3: failed to open stream (error " + std::to_string(open_result) + ")");
        Close();
        return atom::audio::DecoderOpenStatus::InvalidData;
    }

    const auto& frame_info = impl_->dec.info;
    if (!ValidateMp3Frame(frame_info)) {
        LOG_DEBUG(atom::log::audio::Minimp3, "Minimp3: no valid MPEG audio frame found in stream");
        Close();
        return atom::audio::DecoderOpenStatus::InvalidData;
    }

    current_path_ = "(vfs)";
    info_.sample_rate = static_cast<uint32_t>(frame_info.hz);
    info_.channels = static_cast<uint16_t>(frame_info.channels);
    info_.bits_per_sample = 16; // minimp3 always outputs interleaved s16
    info_.is_float = false;
    info_.total_pcm_frames = 0; // unknown unless the VBR tag was detected
    const auto detected_samples = impl_->dec.detected_samples > 0 ? impl_->dec.detected_samples : impl_->dec.samples;
    if (detected_samples > 0 && info_.channels > 0)
        info_.total_pcm_frames = detected_samples / info_.channels;

    LOG_DEBUG(atom::log::audio::Minimp3, "Minimp3: MP3 stream opened from VFS");
    return atom::audio::DecoderOpenStatus::Opened;
}

auto Minimp3Decoder::Close() -> void {
    mp3dec_ex_close(&impl_->dec);
    impl_->mem = {};
    impl_->file = {};
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
            LOG_ERROR(atom::log::audio::Minimp3,
                      "Minimp3: decode error (error " + std::to_string(impl_->dec.last_error) + "): " + current_path_);
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

auto Minimp3Decoder::IsSeekable() const -> bool {
    // mp3dec_ex_t is opened with MP3D_SEEK_TO_SAMPLE: mp3dec_ex_seek() seeks
    // precisely to a sample, building the frame index on demand when the open
    // flags skipped the scan.
    return IsOpen();
}

auto Minimp3Decoder::SeekToFrame(const std::uint64_t frame) -> bool {
    if (!IsOpen())
        return false;

    // mp3dec_ex_seek() takes an interleaved *sample* position, while DecoderInfo
    // and the engine work in frames (that is why total_pcm_frames divides
    // detected_samples by the channel count).
    const auto channels = static_cast<std::uint64_t>(info_.channels);
    if (channels == 0)
        return false;
    if (frame == 0)
        return Rewind();

    const auto target = frame * channels;
    if (mp3dec_ex_seek(&impl_->dec, target) != 0) {
        LOG_WARNING(atom::log::audio::Minimp3,
                    "Minimp3: seek to frame " + std::to_string(frame) + " failed: " + current_path_);
        return false;
    }
    return true;
}

auto Minimp3Decoder::GetInfo() const -> const atom::audio::DecoderInfo& {
    return info_;
}

auto Minimp3Decoder::IsOpen() const -> bool {
    return impl_->mem.data != nullptr || impl_->file.file != nullptr;
}

} // namespace atom::backend::audio_decoder
