#include "AudioClipLoader.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Filesystem/Vfs.hpp>
#include <Log/LogSystem.hpp>

namespace atom {
namespace {

// Maps the decoder's reported sample layout onto the playback layer's formats.
//
// The playback layer only understands 8-bit unsigned, 16-bit signed, 32-bit
// signed and 32-bit float. A decoder that reads packed 24-bit PCM must widen it
// to signed 32-bit while decoding and report 32 (WavProfDecoder does exactly
// that); reporting 24 would make the byte stride -- and therefore every
// following sample -- wrong, so it is rejected instead of guessed at (see
// IAudioDecoder.hpp).
auto ToSampleFormat(const atom::audio::DecoderInfo& info) -> std::optional<atom::audio::AudioSampleFormat> {
    if (info.bits_per_sample == 24)
        return std::nullopt;
    if (info.is_float && info.bits_per_sample == 32)
        return atom::audio::AudioSampleFormat::Float32;
    switch (info.bits_per_sample) {
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

auto DescribeFormat(const atom::audio::DecoderInfo& info) -> std::string {
    return "bits_per_sample=" + std::to_string(info.bits_per_sample) +
           ", is_float=" + (info.is_float ? "true" : "false") +
           ", sample_rate=" + std::to_string(info.sample_rate) + ", channels=" + std::to_string(info.channels);
}

} // namespace

auto AudioClipLoader::OpenDecoder(const std::string_view operation, const std::string& label,
                                  const void* memory_data, const std::size_t memory_size,
                                  atom::audio::AudioSampleFormat& format) const
    -> std::unique_ptr<atom::audio::IAudioDecoder> {
    const auto dot = label.find_last_of('.');
    if (dot == std::string::npos) {
        LOG_WARNING(atom::log::audio::Music,
                    std::string{operation} + ": no file extension, cannot select a decoder: " + label);
        return nullptr;
    }

    // The registry is the single resolution authority: extension lookup,
    // normalization and candidate order all come from it. A format may hold a
    // chain (preferred decoder first, then fallbacks), so walk it until one
    // candidate actually opens the buffer -- that is what lets a fast streaming
    // decoder handle the common case while a heavier one covers the encodings the
    // first cannot read.
    const auto candidates = decoders_.CandidatesForFile(label);
    if (candidates.empty()) {
        LOG_WARNING(atom::log::audio::Music, std::string{operation} + ": no decoder registered for extension '" +
                                                                 label.substr(dot) + "': " + label);
        return nullptr;
    }

    // Every declined candidate is collected so the final diagnostic can name each
    // decoder and the boundary it hit, instead of only the last one.
    std::string declined;
    const auto note_decline = [&declined](const std::string& name, const std::string_view reason) {
        declined += (declined.empty() ? "" : ", ") + name + " (" + std::string{reason} + ")";
    };

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto& candidate = candidates[index];
        const auto name = candidate.name.empty() ? "candidate " + std::to_string(index) : candidate.name;
        auto decoder = candidate.factory ? candidate.factory() : nullptr;
        if (!decoder)
            continue;

        const auto status = decoder->OpenFromMemory(memory_data, memory_size);
        if (status != atom::audio::DecoderOpenStatus::Opened) {
            // A candidate declining a buffer is the normal path of a chain (for
            // example WavProf meeting an ADPCM WAV); the caller sees one warning
            // only after every candidate is exhausted.
            note_decline(name, atom::audio::DescribeDecoderOpenStatus(status));
            LOG_DEBUG(atom::log::audio::Music,
                      std::string{operation} + ": decoder '" + name + "' declined (" +
                          atom::audio::DescribeDecoderOpenStatus(status) + "), trying the next candidate: " + label);
            decoder->Close();
            continue;
        }

        const auto& info = decoder->GetInfo();
        const auto resolved = ToSampleFormat(info);
        if (!resolved || info.sample_rate == 0 || info.channels == 0) {
            note_decline(name, "unusable format");
            LOG_DEBUG(atom::log::audio::Music, std::string{operation} + ": decoder '" + name +
                                                   "' opened the buffer but reported an unusable format (" +
                                                   DescribeFormat(info) + "), trying the next candidate");
            if (info.bits_per_sample == 24) {
                LOG_DEBUG(atom::log::audio::Music,
                          std::string{operation} +
                              ": packed 24-bit PCM must be widened to 32-bit by the decoder itself "
                              "(see WavProfDecoder)");
            }
            decoder->Close();
            continue;
        }

        format = *resolved;
        return decoder;
    }

    LOG_WARNING(atom::log::audio::Music,
                std::string{operation} + ": every registered decoder declined the memory buffer [" + declined +
                    "]: " + label);
    return nullptr;
}

auto AudioClipLoader::OpenDecoder(const std::string_view operation, const std::string& label, atom::fs::IFile& file,
                                  atom::audio::AudioSampleFormat& format) const
    -> std::unique_ptr<atom::audio::IAudioDecoder> {
    const auto dot = label.find_last_of('.');
    if (dot == std::string::npos) {
        LOG_WARNING(atom::log::audio::Music,
                    std::string{operation} + ": no file extension, cannot select a decoder: " + label);
        return nullptr;
    }

    const auto candidates = decoders_.CandidatesForFile(label);
    if (candidates.empty()) {
        LOG_WARNING(atom::log::audio::Music, std::string{operation} + ": no decoder registered for extension '" +
                                                                 label.substr(dot) + "': " + label);
        return nullptr;
    }

    std::string declined;
    const auto note_decline = [&declined](const std::string& name, const std::string_view reason) {
        declined += (declined.empty() ? "" : ", ") + name + " (" + std::string{reason} + ")";
    };

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto& candidate = candidates[index];
        const auto name = candidate.name.empty() ? "candidate " + std::to_string(index) : candidate.name;
        auto decoder = candidate.factory ? candidate.factory() : nullptr;
        if (!decoder)
            continue;

        // A stream must be re-readable for each candidate: a declined decoder may
        // have consumed part of the file, so rewind it before every attempt.
        if (file.Seek(0) != atom::fs::Result::Success) {
            note_decline(name, "unrewindable stream");
            decoder->Close();
            continue;
        }
        const auto status = decoder->OpenStream(file);
        if (status != atom::audio::DecoderOpenStatus::Opened) {
            note_decline(name, atom::audio::DescribeDecoderOpenStatus(status));
            LOG_DEBUG(atom::log::audio::Music,
                      std::string{operation} + ": decoder '" + name + "' declined (" +
                          atom::audio::DescribeDecoderOpenStatus(status) + "), trying the next candidate: " + label);
            decoder->Close();
            continue;
        }

        const auto& info = decoder->GetInfo();
        const auto resolved = ToSampleFormat(info);
        if (!resolved || info.sample_rate == 0 || info.channels == 0) {
            note_decline(name, "unusable format");
            LOG_DEBUG(atom::log::audio::Music, std::string{operation} + ": decoder '" + name +
                                                   "' opened the stream but reported an unusable format (" +
                                                   DescribeFormat(info) + "), trying the next candidate");
            decoder->Close();
            continue;
        }

        format = *resolved;
        return decoder;
    }

    LOG_WARNING(atom::log::audio::Music,
                std::string{operation} + ": every registered decoder declined the stream [" + declined + "]: " + label);
    return nullptr;
}

auto AudioClipLoader::OpenStreamingFromMemory(const std::string& filename, const void* data,
                                              const std::size_t size) const -> std::optional<StreamingResult> {
    atom::audio::AudioSampleFormat format{};
    auto decoder = OpenDecoder("OpenStreamingFromMemory", filename, data, size, format);
    if (!decoder)
        return std::nullopt;

    const auto& info = decoder->GetInfo();

    LOG_INFO(atom::log::audio::Music,
             "OpenStreamingFromMemory: opened streaming decoder over " + std::to_string(size) + " bytes: " + filename +
                 " (sample_rate=" + std::to_string(info.sample_rate) + ", channels=" +
                 std::to_string(info.channels) + ", bits_per_sample=" + std::to_string(info.bits_per_sample) + ")");
    return StreamingResult{
        .decoder = std::move(decoder),
        .spec = atom::audio::AudioSpec{format, info.sample_rate, info.channels},
    };
}

auto AudioClipLoader::Load(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path) const
    -> std::optional<atom::audio::DecodedAudio> {
    std::unique_ptr<atom::fs::IFile> file{};
    if (filesystem.OpenRead(path, file) != atom::fs::Result::Success || !file) {
        LOG_WARNING(atom::log::audio::Music, "Load: could not open through VFS: " + std::string{path.String()});
        return std::nullopt;
    }

    const std::string label{path.String()};
    atom::audio::AudioSampleFormat format{};
    auto decoder = OpenDecoder("Load", label, *file, format);
    if (!decoder)
        return std::nullopt;

    const auto info = decoder->GetInfo();

    std::vector<uint8_t> pcm;
    std::array<uint8_t, 64 * 1024> chunk{};
    while (const auto decoded = decoder->DecodeChunk(chunk.data(), static_cast<uint32_t>(chunk.size()))) {
        pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + decoded);
    }
    decoder->Close();
    if (pcm.empty()) {
        LOG_WARNING(atom::log::audio::Music, "Load: decoder produced no PCM data: " + label);
        return std::nullopt;
    }

    LOG_INFO(atom::log::audio::Music,
             "Load: decoded audio successfully: " + label + " (pcm_bytes=" + std::to_string(pcm.size()) +
                 ", sample_rate=" + std::to_string(info.sample_rate) + ", channels=" + std::to_string(info.channels) +
                 ", bits_per_sample=" + std::to_string(info.bits_per_sample) + ")");
    return atom::audio::DecodedAudio{
        .pcm = std::move(pcm),
        .spec = atom::audio::AudioSpec{format, info.sample_rate, info.channels},
    };
}

auto AudioClipLoader::Load(const std::string& path) const -> std::optional<atom::audio::DecodedAudio> {
    atom::fs::AssetPath asset_path{};
    if (!atom::fs::AssetPath::TryParse(path, asset_path)) {
        LOG_WARNING(atom::log::audio::Music, "Load: invalid asset path: " + path);
        return std::nullopt;
    }
    return Load(atom::fs::Vfs::GetInstance(), asset_path);
}

auto AudioClipLoader::OpenStreaming(const atom::fs::IFileSystem& filesystem, const atom::fs::AssetPath& path) const
    -> std::optional<StreamingResult> {
    std::unique_ptr<atom::fs::IFile> file{};
    if (filesystem.OpenRead(path, file) != atom::fs::Result::Success || !file) {
        LOG_WARNING(atom::log::audio::Music,
                    "OpenStreaming: could not open through VFS: " + std::string{path.String()});
        return std::nullopt;
    }

    const std::string label{path.String()};
    atom::audio::AudioSampleFormat format{};
    auto decoder = OpenDecoder("OpenStreaming", label, *file, format);
    if (!decoder)
        return std::nullopt;

    const auto& info = decoder->GetInfo();

    LOG_INFO(atom::log::audio::Music, "OpenStreaming: opened streaming decoder: " + label +
                                                  " (sample_rate=" + std::to_string(info.sample_rate) +
                                                  ", channels=" + std::to_string(info.channels) +
                                                  ", bits_per_sample=" + std::to_string(info.bits_per_sample) + ")");
    // Keep the IFile alive for as long as the decoder streams from it.
    return StreamingResult{
        .decoder = std::move(decoder),
        .spec = atom::audio::AudioSpec{format, info.sample_rate, info.channels},
        .file = std::move(file),
    };
}

auto AudioClipLoader::OpenStreaming(const std::string& path) const -> std::optional<StreamingResult> {
    atom::fs::AssetPath asset_path{};
    if (!atom::fs::AssetPath::TryParse(path, asset_path)) {
        LOG_WARNING(atom::log::audio::Music, "OpenStreaming: invalid asset path: " + path);
        return std::nullopt;
    }
    return OpenStreaming(atom::fs::Vfs::GetInstance(), asset_path);
}

} // namespace atom
