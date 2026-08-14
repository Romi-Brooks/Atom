#ifndef ATOM_BACKEND_REGISTER_BUILTIN_AUDIO_HPP
#define ATOM_BACKEND_REGISTER_BUILTIN_AUDIO_HPP

namespace atom {
class AudioDecoderRegistry;
auto RegisterBuiltinAudioDecoders(AudioDecoderRegistry& registry) -> bool;
} // namespace atom

#endif // ATOM_BACKEND_REGISTER_BUILTIN_AUDIO_HPP
