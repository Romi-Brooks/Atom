/**
  * @file           : ITimeSource.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral high-resolution host clock contract.
  * @attention      : Monotonic only. Implementations must never jump backwards.
  *                   Values are nanoseconds from an unspecified host epoch;
  *                   only differences are meaningful unless a domain maps them.
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_CONTRACTS_TIME_ITIMESOURCE_HPP
#define ATOM_BACKEND_CONTRACTS_TIME_ITIMESOURCE_HPP

#include <cstdint>

namespace atom::backend {

// Host time source used by atom::time::MasterClock. Backends bind this to
// their native high-resolution counter (SDL_GetPerformanceCounter on SDL3).
// Domain modules must depend on this contract only, never on a concrete
// backend implementation.
class ITimeSource {
    public:
        virtual ~ITimeSource() = default;

        // Monotonic nanoseconds. Must be non-decreasing across calls.
        [[nodiscard]] virtual auto NowNs() const -> std::uint64_t = 0;
};

} // namespace atom::backend

#endif // ATOM_BACKEND_CONTRACTS_TIME_ITIMESOURCE_HPP
