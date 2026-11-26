/**
  * @file           : NumericContract.hpp
  * @author         : Romi Brooks
  * @brief          : Unified numeric boundary contract for public engine APIs.
  * @attention      : Header-only and dependency-free so every layer (contracts,
  *                   backends, media, render, Lua) applies the same rules.
  * @date           : 2026/9/30
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_NUMERIC_NUMERICCONTRACT_HPP
#define ATOM_BACKEND_CONTRACTS_NUMERIC_NUMERICCONTRACT_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// ARCH-006: public entry points that take a magnitude from game code route it
// through one of these helpers. The goal is that a NaN, an infinity, a negative
// or an absurd magnitude is either rejected (Try*/IsValid*) or clamped
// deterministically (Sanitize*) instead of reaching a renderer, a decoder or an
// audio backend, where it used to become UB, a division by zero, or an
// unbounded allocation.
namespace atom::numeric {

// ---------------------------------------------------------------------------
// Shared ranges
// ---------------------------------------------------------------------------

// Volume is a 0..100 percentage engine-wide (IAudioSource, AudioMixer, the Lua
// bindings and every example use this scale).
inline constexpr float min_volume = 0.0f;
inline constexpr float max_volume = 100.0f;

// Playback positions and fade durations are seconds. The bound keeps a
// NaN/infinite value from turning into an overflowing integer conversion in the
// decoders and the audio backends.
inline constexpr float max_media_seconds = 86400.0f; // 24 h

// Geometric radii and rounded corners. The bound keeps tessellation and vertex
// math inside the range where float still has sub-unit precision.
inline constexpr float max_radius = 1.0e6f;

// Largest CPU-side RGBA surface the engine accepts for one texture.
inline constexpr uint32_t max_texture_dimension = 16384u;

// Largest CPU-side surface a decoder may allocate for a single image. This is a
// *total* budget, not a per-axis limit: a thin strip such as 1x20000 is cheap and
// must stay decodable, while a 16384x16384 header (~1 GiB of RGBA) must not be.
inline constexpr std::size_t max_decoded_image_bytes = 256u * 1024u * 1024u; // 256 MiB

// Voice pool bounds: at least one voice, and a hard cap so a public "voice
// limit" can never ask a backend for an unbounded number of streams.
inline constexpr std::size_t min_voices = 1;
inline constexpr std::size_t max_voices = 256;
inline constexpr std::size_t default_voices = 8;

// ---------------------------------------------------------------------------
// Predicates and sanitizers
// ---------------------------------------------------------------------------

[[nodiscard]] inline auto IsFinite(const float value) -> bool {
    return std::isfinite(value);
}

// Volume: non-finite collapses to silence; finite values clamp into range.
[[nodiscard]] inline auto SanitizeVolume(const float volume) -> float {
    if (!std::isfinite(volume))
        return min_volume;
    return std::clamp(volume, min_volume, max_volume);
}

[[nodiscard]] inline auto IsValidVolume(const float volume) -> bool {
    return std::isfinite(volume) && volume >= min_volume && volume <= max_volume;
}

// Radius: non-finite or non-positive collapses to 0, which every drawing call
// already treats as "nothing to draw".
[[nodiscard]] inline auto SanitizeRadius(const float radius) -> float {
    if (!std::isfinite(radius) || radius <= 0.0f)
        return 0.0f;
    return std::min(radius, max_radius);
}

[[nodiscard]] inline auto IsValidRadius(const float radius) -> bool {
    return std::isfinite(radius) && radius > 0.0f && radius <= max_radius;
}

// Duration / timeout in seconds: finite and non-negative, clamped to the media
// ceiling.
[[nodiscard]] inline auto SanitizeSeconds(const float seconds) -> float {
    if (!std::isfinite(seconds) || seconds <= 0.0f)
        return 0.0f;
    return std::min(seconds, max_media_seconds);
}

// Playback position in seconds. Returns false for a non-finite request (the
// caller reports a failed seek); a finite value is clamped into
// [0, max_media_seconds].
[[nodiscard]] inline auto TrySanitizeOffsetSeconds(const float seconds, float& out_seconds) -> bool {
    if (!std::isfinite(seconds))
        return false;
    out_seconds = std::clamp(seconds, 0.0f, max_media_seconds);
    return true;
}

// Voice cap. 0 means "unspecified": fall back to the engine default. Out-of-range
// values are clamped, so a pool can always be constructed.
[[nodiscard]] inline auto SanitizeVoiceCount(const std::size_t voices,
                                             const std::size_t fallback = default_voices) -> std::size_t {
    const auto requested = voices == 0 ? fallback : voices;
    return std::clamp(requested, min_voices, max_voices);
}

// Strict form used by setters that can report failure.
[[nodiscard]] inline auto IsValidVoiceCount(const std::size_t voices) -> bool {
    return voices >= min_voices && voices <= max_voices;
}

// Byte counts for CPU-side buffers. Rejects a size that cannot be multiplied out
// in std::size_t without wrapping, so `width * height * bytes_per_pixel` is
// always safe. (On a 64-bit size_t the product of two uint32_t dimensions cannot
// wrap, so there the per-pixel term is the check that actually fires.)
[[nodiscard]] inline auto CheckedPixelBytes(const uint32_t width, const uint32_t height,
                                            const uint32_t bytes_per_pixel, std::size_t& out_bytes) -> bool {
    if (width == 0 || height == 0 || bytes_per_pixel == 0)
        return false;
    const auto max = std::numeric_limits<std::size_t>::max();
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    const auto bpp = static_cast<std::size_t>(bytes_per_pixel);
    if (w > max / h)
        return false;
    const auto pixels = w * h;
    if (pixels > max / bpp)
        return false;
    out_bytes = pixels * bpp;
    return true;
}

// Texture size: dimensions inside [1, max_texture_dimension] whose RGBA upload
// size is representable.
[[nodiscard]] inline auto IsValidTextureSize(const uint32_t width, const uint32_t height) -> bool {
    if (width == 0 || height == 0 || width > max_texture_dimension || height > max_texture_dimension)
        return false;
    std::size_t bytes = 0;
    return CheckedPixelBytes(width, height, 4u, bytes);
}

// A borrowed buffer is only usable when a non-zero length comes with a non-null
// pointer (an empty or null buffer has nothing to decode).
[[nodiscard]] inline auto IsValidBuffer(const void* data, const std::size_t size) -> bool {
    return data != nullptr && size > 0;
}

// Total RGBA budget for one decoded image, used by decoders that must reject a
// hostile header *before* allocating. Returns false when the size is not
// representable or exceeds max_decoded_image_bytes.
[[nodiscard]] inline auto IsWithinDecodedImageBudget(const uint32_t width, const uint32_t height,
                                                     const uint32_t bytes_per_pixel = 4u) -> bool {
    std::size_t bytes = 0;
    if (!CheckedPixelBytes(width, height, bytes_per_pixel, bytes))
        return false;
    return bytes <= max_decoded_image_bytes;
}

} // namespace atom::numeric

#endif // ATOM_BACKEND_CONTRACTS_NUMERIC_NUMERICCONTRACT_HPP
