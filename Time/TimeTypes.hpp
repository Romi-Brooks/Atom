/**
  * @file           : TimeTypes.hpp
  * @author         : Romi Brooks
  * @brief          : Shared time value types for the Atom Time module.
  * @attention      : All domain time is nanoseconds. Prefer Duration/TimePoint
  *                   over float seconds at API boundaries; convert only at UI.
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_TIME_TIMETYPES_HPP
#define ATOM_TIME_TIMETYPES_HPP

#include <cstdint>

namespace atom::time {

// Nanoseconds since a domain epoch. Monotonic within a domain.
using TimePoint = std::uint64_t;
// Signed nanosecond span (deltas may be clamped to 0).
using Duration = std::int64_t;

inline constexpr Duration kNanosecond = 1;
inline constexpr Duration kMicrosecond = 1000;
inline constexpr Duration kMillisecond = 1000000;
inline constexpr Duration kSecond = 1000000000;

[[nodiscard]] inline auto ToSeconds(const TimePoint value) -> double {
    return static_cast<double>(value) / static_cast<double>(kSecond);
}

[[nodiscard]] inline auto ToSeconds(const Duration value) -> double {
    return static_cast<double>(value) / static_cast<double>(kSecond);
}

[[nodiscard]] inline auto ToFloatSeconds(const Duration value) -> float {
    return static_cast<float>(ToSeconds(value));
}

[[nodiscard]] inline auto FromSeconds(const double seconds) -> Duration {
    return static_cast<Duration>(seconds * static_cast<double>(kSecond));
}

// Description of a derived domain clock.
struct ClockDesc {
        // 1.0 = real time, 0.5 = slow-motion, 2.0 = fast-forward.
        double rate = 1.0;
        // Added to reported Now(). Negative values look "earlier" (e.g. audio latency).
        Duration offset_ns = 0;
        // 0 = variable step. Otherwise fixed simulation step (physics / determinism).
        Duration fixed_step_ns = 0;
        // Catch-up cap per Advance; leftover backlog is dropped (spiral-of-death guard).
        std::uint32_t max_steps_per_advance = 5;
        bool paused = false;
};

// Result of advancing one domain for a single host frame.
struct TickResult {
        TimePoint now = 0;
        // Domain time consumed this tick: one variable delta, or step * steps.
        Duration delta_ns = 0;
        // Fixed-step domains: how many steps the caller should simulate (0 if paused).
        // Variable-step domains: 1 when advancing, 0 when paused.
        std::uint32_t steps = 0;
        // Interpolation remainder in [0, 1). Only meaningful for fixed-step domains.
        float alpha = 0.0f;
        // Fixed step size for this domain (0 when variable).
        Duration fixed_step_ns = 0;
};

// Result of advancing the master clock for one frame.
struct FrameTick {
        TimePoint master_now = 0;
        Duration host_delta_ns = 0;
};

} // namespace atom::time

#endif // ATOM_TIME_TIMETYPES_HPP
