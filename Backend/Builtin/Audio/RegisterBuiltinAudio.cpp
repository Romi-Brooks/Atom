#include "RegisterBuiltinAudio.hpp"

#include <memory>

#include <Backend/Builtin/Audio/Decoder/WavRiff/BuiltinWavDecoder.hpp>
#include <Backend/Registry/AudioDecoderRegistry.hpp>

namespace atom {

auto RegisterBuiltinAudioDecoders(AudioDecoderRegistry& registry) -> bool {
    return registry.Register(".wav", [] {
        return std::make_unique<BuiltinWavDecoder>();
    });
}

} // namespace atom
