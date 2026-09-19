#ifndef ATOM_BACKEND_AUDIO_DECODER_REGISTRY_HPP
#define ATOM_BACKEND_AUDIO_DECODER_REGISTRY_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <Backend/Contracts/Audio/IAudioDecoder.hpp>

namespace atom::audio {

// Format -> decoder factory registry.
//
// One extension can hold a *chain* of decoders: the preferred implementation
// first, then fallbacks that are only tried when an earlier candidate cannot
// open the file. That is how a fast-but-narrow decoder and a complete-but-heavy
// one coexist (for example WavProf streaming PCM while SDL3's loader covers the
// compressed WAV encodings WavProf cannot read) without making every caller pick
// one implementation up front.
class AudioDecoderRegistry final {
    public:
        using Factory = std::function<std::unique_ptr<IAudioDecoder>()>;

        // One entry of an extension's decoder chain. The name is diagnostics-only
        // (logs name the decoder that declined a file) and may be left empty.
        struct Candidate {
                std::string name;
                Factory factory;
        };

        // Highest-priority decoder for an extension. Keeps the first registration
        // (existing behaviour); use RegisterFallback/Replace for the rest of the
        // chain.
        auto Register(std::string extension, Factory factory, std::string name = {}) -> bool;

        // Append a decoder that is tried only when every earlier candidate failed
        // to open the file.
        auto RegisterFallback(std::string extension, Factory factory, std::string name = {}) -> bool;

        // Replace the entire chain for an extension with a single decoder.
        auto Replace(std::string extension, Factory factory, std::string name = {}) -> bool;
        auto Unregister(std::string_view extension) -> bool;

        // First candidate only. Kept for callers that want exactly one decoder;
        // AudioClipLoader uses CandidatesForFile() so the chain can fall through.
        [[nodiscard]] auto CreateForFile(std::string_view filepath) const -> std::unique_ptr<IAudioDecoder>;

        // Every candidate for the file's extension, in priority order.
        [[nodiscard]] auto CandidatesForFile(std::string_view filepath) const -> std::vector<Candidate>;

        [[nodiscard]] auto Contains(std::string_view extension) const -> bool;

    private:
        static auto NormalizeExtension(std::string_view extension) -> std::string;
        std::unordered_map<std::string, std::vector<Candidate>> chains_;
};

} // namespace atom::audio

#endif
