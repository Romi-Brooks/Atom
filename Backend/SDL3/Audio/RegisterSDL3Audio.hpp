#ifndef ATOM_BACKEND_REGISTER_SDL3_AUDIO_HPP
#define ATOM_BACKEND_REGISTER_SDL3_AUDIO_HPP

namespace atom {
class AudioDecoderRegistry;
auto RegisterSDL3AudioDecoders(AudioDecoderRegistry& registry) -> bool;
} // namespace atom

#endif // ATOM_BACKEND_REGISTER_SDL3_AUDIO_HPP
