// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#ifndef ATOM_RNG_HPP
#define ATOM_RNG_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <Atom/Build/CoreExport.hpp>

namespace atom::random {

// Value-type pseudorandom stream for simulations and procedural content.
// Give each concurrent consumer its own stream. This is not a security RNG.
class ATOM_CORE_API Rng final {
    public:
        explicit constexpr Rng(std::uint64_t seed) noexcept : seed_(seed), state_(seed) {}

        [[nodiscard]] static auto FromEntropy() -> Rng;
        [[nodiscard]] auto Fork(std::uint64_t stream) const noexcept -> Rng;
        [[nodiscard]] auto NextU64() noexcept -> std::uint64_t;
        // Uniform over [0, bound). Bounds 0 and 1 return zero without advancing.
        [[nodiscard]] auto Below(std::uint64_t bound) noexcept -> std::uint64_t;
        [[nodiscard]] auto UnitFloat() noexcept -> float;
        // Finite bounds with min < max produce [min, max). Otherwise returns min.
        [[nodiscard]] auto UniformFloat(float min, float max) noexcept -> float;

        template <class T, std::size_t Extent> auto Shuffle(std::span<T, Extent> values) -> void {
            for (auto remaining = values.size(); remaining > 1; --remaining) {
                const auto selected = static_cast<std::size_t>(Below(remaining));
                using std::swap;
                swap(values[remaining - 1], values[selected]);
            }
        }

    private:
        std::uint64_t seed_;
        std::uint64_t state_;
};

} // namespace atom::random

#endif // ATOM_RNG_HPP
