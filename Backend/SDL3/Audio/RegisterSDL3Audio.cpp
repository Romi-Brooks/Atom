#include "RegisterSDL3Audio.hpp"

#include <memory>

#include <Backend/Registry/AudioDecoderRegistry.hpp>
#include <Backend/SDL3/Audio/Decoder/SDL3WavDecoder.hpp>

namespace atom {

auto RegisterSDL3AudioDecoders(AudioDecoderRegistry& registry) -> bool {
    return registry.Register(".wav", [] { return std::make_unique<SDL3WavDecoder>(); });
}

} // namespace atom
