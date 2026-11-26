// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "Rng.hpp"

#include <cmath>
#include <random>

namespace atom::random {
namespace {

// SplitMix64, using fixed unsigned arithmetic for a reproducible stream.
constexpr std::uint64_t increment = 0x9E3779B97F4A7C15ULL;

auto Mix(std::uint64_t value) noexcept -> std::uint64_t {
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

} // namespace

auto Rng::FromEntropy() -> Rng {
    std::random_device source;
    const auto high = static_cast<std::uint64_t>(source()) << 32;
    return Rng{high ^ static_cast<std::uint64_t>(source())};
}

auto Rng::Fork(const std::uint64_t stream) const noexcept -> Rng {
    // Derive from the seed so forking does not depend on the parent's draw order.
    return Rng{Mix(seed_ ^ Mix(stream + increment))};
}

auto Rng::NextU64() noexcept -> std::uint64_t {
    state_ += increment;
    return Mix(state_);
}

auto Rng::Below(const std::uint64_t bound) noexcept -> std::uint64_t {
    if (bound <= 1)
        return 0;
    // Discard the part of the input range that would bias the modulo.
    const auto threshold = (std::uint64_t{0} - bound) % bound;
    auto value = NextU64();
    while (value < threshold)
        value = NextU64();
    return value % bound;
}

auto Rng::UnitFloat() noexcept -> float {
    return static_cast<float>(NextU64() >> 40) * 0x1p-24f;
}

auto Rng::UniformFloat(const float min, const float max) noexcept -> float {
    if (!std::isfinite(min) || !std::isfinite(max) || min >= max)
        return min;
    const double span = static_cast<double>(max) - min;
    const float sampled = static_cast<float>(min + span * UnitFloat());
    return sampled < max ? sampled : std::nextafter(max, min);
}

} // namespace atom::random
