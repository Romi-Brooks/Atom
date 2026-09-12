/**
  * @file           : SDL3WavDecoder.cpp
  * @author         : Romi Brooks
  * @brief          : WAV decoder backed by SDL3's own loader (SDL_LoadWAV)
  * @attention      : See the header for the memory/streaming trade-off.
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDL3WavDecoder.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include <SDL3/SDL.h>

#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Log/LogSystem.hpp>

namespace atom::backend::audio_decoder {

namespace {

// Maps the SDL spec SDL_LoadWAV() produced onto the playback layer's formats.
// SDL widens packed 24-bit PCM and every compressed encoding (ADPCM, A-Law,
// mu-Law) into one of these, so 64-bit float is the only thing left to reject.
auto ToSampleFormat(const SDL_AudioSpec& spec) -> std::optional<atom::audio::AudioSampleFormat> {
    const auto bits = SDL_AUDIO_BITSIZE(spec.format);
    if (SDL_AUDIO_ISFLOAT(spec.format))
        return bits == 32 ? std::optional{atom::audio::AudioSampleFormat::Float32} : std::nullopt;
    switch (bits) {
    case 8:
        return atom::audio::AudioSampleFormat::Unsigned8;
    case 16:
        return atom::audio::AudioSampleFormat::Signed16;
    case 32:
        return atom::audio::AudioSampleFormat::Signed32;
    default:
        return std::nullopt;
    }
}

} // namespace

struct SDL3WavDecoder::Impl {
    Uint8* pcm = nullptr;   // owned; released with SDL_free
    Uint32 length = 0;      // total decoded bytes
    Uint32 cursor = 0;      // read offset into pcm
    SDL_AudioSpec spec{};
};

SDL3WavDecoder::SDL3WavDecoder() : impl_(std::make_unique<Impl>()) {}

SDL3WavDecoder::~SDL3WavDecoder() {
    Close();
}

auto SDL3WavDecoder::AdoptLoadedBuffer(const std::string& source_label) -> bool {
    const auto format = ToSampleFormat(impl_->spec);
    if (!format || impl_->spec.freq <= 0 || impl_->spec.channels == 0 || impl_->pcm == nullptr ||
        impl_->length == 0) {
        LOG_DEBUG(atom::log::audio::SDL3Wav,
                    "SDL3Wav: unsupported or empty audio data (" + std::to_string(impl_->spec.freq) + " Hz, " +
                        std::to_string(impl_->spec.channels) + " ch, format=" +
                        std::to_string(static_cast<int>(impl_->spec.format)) + "): " + source_label);
        SDL_free(impl_->pcm);
        impl_->pcm = nullptr;
        impl_->length = 0;
        return false;
    }

    const auto bytes_per_frame = (SDL_AUDIO_BITSIZE(impl_->spec.format) / 8u) * impl_->spec.channels;
    info_.sample_rate = static_cast<uint32_t>(impl_->spec.freq);
    info_.channels = impl_->spec.channels;
    info_.bits_per_sample = SDL_AUDIO_BITSIZE(impl_->spec.format);
    info_.is_float = SDL_AUDIO_ISFLOAT(impl_->spec.format);
    // SDL already decoded everything, so the exact frame count is known.
    info_.total_pcm_frames = bytes_per_frame == 0 ? 0 : impl_->length / bytes_per_frame;

    LOG_INFO(atom::log::audio::SDL3Wav,
             "SDL3Wav: decoded whole file into memory: " + source_label + " (pcm_bytes=" +
                 std::to_string(impl_->length) + ", sample_rate=" + std::to_string(info_.sample_rate) +
                 ", channels=" + std::to_string(info_.channels) + ", bits_per_sample=" +
                 std::to_string(info_.bits_per_sample) + ")");
    return true;
}

auto SDL3WavDecoder::Open(const std::string& path) -> atom::audio::DecoderOpenStatus {
    Close();

    // SDL takes a UTF-8 path and handles the platform encoding itself; the file
    // is fully decoded into impl_->pcm.
    if (!SDL_LoadWAV(path.c_str(), &impl_->spec, &impl_->pcm, &impl_->length)) {
        // SDL reports "Couldn't open ..." for unreadable files and format errors
        // for everything else; keep both visible at debug level, the loader owns
        // the user-facing diagnostics.
        LOG_DEBUG(atom::log::audio::SDL3Wav,
                  "SDL3Wav: SDL_LoadWAV failed: " + std::string(SDL_GetError()) + ": " + path);
        impl_->pcm = nullptr;
        impl_->length = 0;
        return atom::audio::DecoderOpenStatus::InvalidData;
    }
    impl_->cursor = 0;
    if (!AdoptLoadedBuffer(path)) {
        impl_->spec = {};
        return atom::audio::DecoderOpenStatus::InvalidData;
    }
    return atom::audio::DecoderOpenStatus::Opened;
}

auto SDL3WavDecoder::OpenFromMemory(const void* data, const std::size_t size) -> atom::audio::DecoderOpenStatus {
    Close();
    if (data == nullptr || size == 0) {
        LOG_DEBUG(atom::log::audio::SDL3Wav, "SDL3Wav: invalid in-memory buffer");
        return atom::audio::DecoderOpenStatus::IoError;
    }

    // SDL_LoadWAV_IO borrows the buffer: SDL_IOFromConstMem does not copy it, and
    // the decoded PCM it produces is a fresh allocation we own.
    SDL_IOStream* stream = SDL_IOFromConstMem(data, size);
    if (stream == nullptr) {
        LOG_DEBUG(atom::log::audio::SDL3Wav,
                  "SDL3Wav: SDL_IOFromConstMem failed: " + std::string(SDL_GetError()));
        return atom::audio::DecoderOpenStatus::IoError;
    }
    const bool loaded = SDL_LoadWAV_IO(stream, true, &impl_->spec, &impl_->pcm, &impl_->length);
    if (!loaded) {
        LOG_DEBUG(atom::log::audio::SDL3Wav,
                  "SDL3Wav: SDL_LoadWAV_IO failed: " + std::string(SDL_GetError()));
        impl_->pcm = nullptr;
        impl_->length = 0;
        return atom::audio::DecoderOpenStatus::InvalidData;
    }
    impl_->cursor = 0;
    if (!AdoptLoadedBuffer("(memory)")) {
        impl_->spec = {};
        return atom::audio::DecoderOpenStatus::InvalidData;
    }
    return atom::audio::DecoderOpenStatus::Opened;
}

auto SDL3WavDecoder::Close() -> void {
    if (impl_->pcm != nullptr) {
        SDL_free(impl_->pcm);
        impl_->pcm = nullptr;
    }
    impl_->length = 0;
    impl_->cursor = 0;
    impl_->spec = {};
    info_ = {};
}

auto SDL3WavDecoder::DecodeChunk(uint8_t* output, const uint32_t max_bytes) -> uint32_t {
    if (!IsOpen() || output == nullptr || max_bytes == 0)
        return 0;
    const auto remaining = impl_->length - impl_->cursor;
    if (remaining == 0)
        return 0;

    const auto to_copy = std::min<std::uint32_t>(max_bytes, remaining);
    std::copy_n(impl_->pcm + impl_->cursor, to_copy, output);
    impl_->cursor += to_copy;
    return to_copy;
}

auto SDL3WavDecoder::Rewind() -> bool {
    if (!IsOpen())
        return false;
    impl_->cursor = 0;
    return true;
}

auto SDL3WavDecoder::IsSeekable() const -> bool {
    return IsOpen();
}

auto SDL3WavDecoder::SeekToFrame(const std::uint64_t frame) -> bool {
    if (!IsOpen() || info_.channels == 0 || info_.bits_per_sample == 0)
        return false;
    const auto bytes_per_frame = (info_.bits_per_sample / 8u) * info_.channels;
    if (bytes_per_frame == 0)
        return false;

    auto offset = frame * bytes_per_frame;
    if (offset > impl_->length)
        offset = impl_->length;
    // Always land on a frame boundary.
    offset -= offset % bytes_per_frame;
    impl_->cursor = static_cast<Uint32>(offset);
    return true;
}

auto SDL3WavDecoder::GetInfo() const -> const atom::audio::DecoderInfo& {
    return info_;
}

auto SDL3WavDecoder::IsOpen() const -> bool {
    return impl_->pcm != nullptr && impl_->length > 0;
}

auto CreateSDL3WavDecoder() -> std::unique_ptr<atom::audio::IAudioDecoder> {
    return std::make_unique<SDL3WavDecoder>();
}

} // namespace atom::backend::audio_decoder
