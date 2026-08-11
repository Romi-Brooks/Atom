#ifndef ATOM_BACKEND_REGISTER_BUILTIN_AUDIO_HPP
#define ATOM_BACKEND_REGISTER_BUILTIN_AUDIO_HPP

namespace atom {
class AudioDecoderRegistry;
auto RegisterBuiltinAudioDecoders(AudioDecoderRegistry& registry) -> bool;
}

#endif // ATOM_BACKEND_REGISTER_BUILTIN_AUDIO_HPP
