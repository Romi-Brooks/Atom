// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file AudioBackendId.hpp
 * @brief Public audio playback-backend selection tokens (engine-curated).
 */

#ifndef ATOM_BACKEND_CONTRACTS_AUDIO_AUDIO_BACKEND_ID_HPP
#define ATOM_BACKEND_CONTRACTS_AUDIO_AUDIO_BACKEND_ID_HPP

#include <string_view>

namespace atom::backend {

// Closed set of audio playback backends shipped with Atom. Format decoders are
// selected separately by file extension and are not part of this enum.
enum class AudioBackendId {
    Sdl3,
    Sdl3Mixer,
};

[[nodiscard]] constexpr auto ToString(const AudioBackendId id) -> std::string_view {
    switch (id) {
    case AudioBackendId::Sdl3:
        return "sdl3";
    case AudioBackendId::Sdl3Mixer:
        return "sdl3_mixer";
    }
    return "sdl3";
}

} // namespace atom::backend

#endif // ATOM_BACKEND_CONTRACTS_AUDIO_AUDIO_BACKEND_ID_HPP
