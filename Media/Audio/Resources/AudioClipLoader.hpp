#ifndef ATOM_AUDIO_CLIP_LOADER_HPP
#define ATOM_AUDIO_CLIP_LOADER_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <Backend/Contracts/Audio/AudioTypes.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Filesystem/FileSystem.hpp>

namespace atom {
class AudioClipLoader final {
    public:
        explicit AudioClipLoader(atom::audio::AudioDecoderRegistry& decoders) : decoders_(decoders) {}

        // Open a streaming decoder and return it along with the audio spec.
        // The decoder is left open — the caller takes ownership and must close it.
        // The returned atom::audio::AudioSpec is suitable for passing to atom::audio::IAudioBackend::CreateStreamingMusicSource.
        struct StreamingResult {
                std::unique_ptr<atom::audio::IAudioDecoder> decoder;
                atom::audio::AudioSpec spec;
                // Owns the underlying VFS file when the decoder was opened from a
                // stream (OpenStreaming(IFileSystem, AssetPath)). Must outlive
                // `decoder`; keep it alive alongside the decoder/source. Null for
                // the memory entry point.
                std::unique_ptr<atom::fs::IFile> file;
        };

        // Same as OpenStreaming, but over an in-memory buffer (e.g. an entry
        // extracted from a resource pack). filename is used only to select a
        // decoder by extension. The buffer is borrowed: the caller must keep it
        // alive for as long as the returned decoder is used.
        [[nodiscard]] auto OpenStreamingFromMemory(const std::string& filename, const void* data,
                                                   std::size_t size) const -> std::optional<StreamingResult>;

        // Load a resource through the VFS. The path's extension selects the
        // decoder; the IFile is opened via the filesystem and handed to the
        // decoder's OpenStream, so no native path is involved.
        [[nodiscard]] auto Load(const atom::fs::IFileSystem& filesystem,
                                const atom::fs::AssetPath& path) const -> std::optional<atom::audio::DecodedAudio>;

        // Convenience overload: parses `path` as an AssetPath and resolves it
        // through the process-wide default Vfs (atom::fs::Vfs::GetInstance()).
        [[nodiscard]] auto Load(const std::string& path) const -> std::optional<atom::audio::DecodedAudio>;

        // Same as OpenStreaming, but resolving the file through the VFS first.
        [[nodiscard]] auto OpenStreaming(const atom::fs::IFileSystem& filesystem,
                                         const atom::fs::AssetPath& path) const -> std::optional<StreamingResult>;

        // Convenience overload: parses `path` as an AssetPath and resolves it
        // through the process-wide default Vfs (atom::fs::Vfs::GetInstance()).
        [[nodiscard]] auto OpenStreaming(const std::string& path) const -> std::optional<StreamingResult>;

    private:
        // Shared front half of both entry points: resolve the decoder through the
        // registry, open it and validate the format it reports. On success the
        // resolved sample format is written to `format`. Every failure is logged
        // at WARNING -- "the file is listed but silent" must be visible at the
        // default log level.
        [[nodiscard]] auto OpenDecoder(std::string_view operation, const std::string& label,
                                       const void* memory_data, std::size_t memory_size,
                                       atom::audio::AudioSampleFormat& format) const
            -> std::unique_ptr<atom::audio::IAudioDecoder>;

        [[nodiscard]] auto OpenDecoder(std::string_view operation, const std::string& label,
                                       atom::fs::IFile& file, atom::audio::AudioSampleFormat& format) const
            -> std::unique_ptr<atom::audio::IAudioDecoder>;

        atom::audio::AudioDecoderRegistry& decoders_;
};
} // namespace atom

#endif // ATOM_AUDIO_CLIP_LOADER_HPP
