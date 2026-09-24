/**
  * @file           : IAudioDecoder.hpp
  * @author         : Romi Brooks
  * @brief          : Abstract interface for audio decoders (Strategy pattern)
  * @attention      : Implementations: Atom WavProf (own RIFF reader), SDL3Wav
  *                   (SDL_LoadWAV), Minimp3 (mp3dec_ex). Each decoder opens one
  *                   file/buffer and produces raw PCM chunks.
  *
  *                   Responsibility split (keep it that way):
  *                   - decoder : decides whether it can handle a file and, if not,
  *                               says *why* (DecoderOpenStatus). It never picks or
  *                               falls back to another decoder.
  *                   - registry: owns the ordered candidate list per extension
  *                               (Register / RegisterFallback / Replace).
  *                   - loader  : owns the attempt loop, engine-format validation
  *                               and diagnostics (AudioClipLoader).
  *                   - caller  : owns the policy, i.e. which order to register.
  * @date           : 2026/7/5
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_IAUDIO_DECODER_HPP
#define ATOM_IAUDIO_DECODER_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include <Filesystem/FileSystem.hpp>

namespace atom::audio {

struct DecoderInfo {
        uint32_t sample_rate = 0;
        uint16_t channels = 0;
        // Bits per sample of the PCM the decoder writes to DecodeChunk().
        //
        // This must be one of the widths the playback layer understands: 8, 16,
        // 32 (integer) or 32 with is_float. A decoder that reads packed 24-bit
        // PCM has to widen it to signed 32-bit while decoding and report 32 here
        // (see WavProfDecoder); reporting 24 would make every consumer
        // misinterpret the byte stride. SDL_AudioFormat has no packed 24-bit
        // format either, so widening somewhere is unavoidable.
        uint16_t bits_per_sample = 0;
        uint64_t total_pcm_frames = 0; // 0 = unknown (streaming)
        bool is_float = false;         // true when data is IEEE float (e.g. F32LE)
};

// Result of Open()/OpenFromMemory(). The reason matters: it is what tells the
// loader whether another decoder in the chain is worth trying.
enum class DecoderOpenStatus {
        // The decoder is open and GetInfo() is valid.
        Opened,
        // The decoder does not implement this file's container/encoding. This is
        // the normal outcome for a chained decoder (WavProf on an ADPCM WAV) and
        // means "ask the next candidate".
        UnsupportedFormat,
        // The decoder implements the format but the data is not usable: broken
        // container, truncated stream, no audio frames.
        InvalidData,
        // The file/buffer could not be read at all (missing, unreadable, empty).
        IoError,
};

// Human-readable status for logs and diagnostics.
[[nodiscard]] constexpr auto DescribeDecoderOpenStatus(const DecoderOpenStatus status) -> const char* {
    switch (status) {
    case DecoderOpenStatus::Opened:
        return "opened";
    case DecoderOpenStatus::UnsupportedFormat:
        return "unsupported format";
    case DecoderOpenStatus::InvalidData:
        return "invalid data";
    case DecoderOpenStatus::IoError:
        return "i/o error";
    }
    return "unknown";
}

class IAudioDecoder {
    public:
        virtual ~IAudioDecoder() = default;

        // Open a decoder over an in-memory buffer (e.g. an entry extracted from
        // a resource pack). The buffer is borrowed: the caller must keep it alive
        // until Close() is called. Same status contract as Open().
        [[nodiscard]] virtual auto OpenFromMemory(const void* data, std::size_t size) -> DecoderOpenStatus = 0;

        // Open a decoder over an already-resolved VFS file. This is the streaming
        // entry point that lets a decoder consume an asset without ever touching a
        // native path: the IFileSystem handed out the IFile, and the decoder only
        // sees the random/sequential read contract. The file is borrowed for the
        // lifetime of the decoder and must outlive it until Close().
        //
        // Decoders that cannot stream (e.g. an all-in-memory loader) may read the
        // whole file up front here and fall back to their buffer path. Same status
        // contract as Open().
        [[nodiscard]] virtual auto OpenStream(atom::fs::IFile& file) -> DecoderOpenStatus = 0;

        // Close and release all resources.
        virtual auto Close() -> void = 0;

        // Decode the next chunk of PCM data.
        // Returns number of bytes written to output. 0 = EOF or error.
        virtual auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t = 0;

        // Seek to the beginning of PCM data (rewind).
        virtual auto Rewind() -> bool = 0;

        // True when SeekToFrame() can jump to an arbitrary frame. Decoders that
        // only decode forward report false; Rewind() stays usable either way.
        [[nodiscard]] virtual auto IsSeekable() const -> bool {
            return false;
        }

        // Seek to an absolute PCM frame position (0 == the first frame).
        // The next DecodeChunk() must return audio starting at that frame.
        // frame may point past the end of the stream: implementations clamp to
        // the last frame and report success, so that "seek to the end" works.
        // Default implementation supports "seek to the start" only, through
        // Rewind().
        virtual auto SeekToFrame(std::uint64_t frame) -> bool {
            return frame == 0 && Rewind();
        }

        // Get decoder info (valid after Open succeeds).
        [[nodiscard]] virtual auto GetInfo() const -> const DecoderInfo& = 0;

        // Check if a file is open.
        [[nodiscard]] virtual auto IsOpen() const -> bool = 0;
};

} // namespace atom::audio

#endif // ATOM_IAUDIO_DECODER_HPP
