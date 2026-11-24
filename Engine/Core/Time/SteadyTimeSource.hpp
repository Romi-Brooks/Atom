/**
  * @file           : SteadyTimeSource.hpp
  * @author         : Romi Brooks
  * @brief          : chrono::steady_clock host source for tests and headless runs.
  * @attention      : Not a substitute for SDL3TimeSource in production; SDL
  *                   keeps host time aligned with audio/event timestamps.
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_TIME_STEADYTIMESOURCE_HPP
#define ATOM_TIME_STEADYTIMESOURCE_HPP

#include <chrono>
#include <cstdint>

#include <Backend/Contracts/Time/ITimeSource.hpp>

namespace atom::time {

class SteadyTimeSource final : public backend::ITimeSource {
    public:
        [[nodiscard]] auto NowNs() const -> std::uint64_t override {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
        }
};

// Deterministic source for unit tests. Advance manually.
class ManualTimeSource final : public backend::ITimeSource {
    public:
        [[nodiscard]] auto NowNs() const -> std::uint64_t override {
            return now_ns_;
        }

        auto AdvanceNs(const std::uint64_t delta_ns) -> void {
            now_ns_ += delta_ns;
        }

        auto SetNs(const std::uint64_t value) -> void {
            now_ns_ = value;
        }

    private:
        std::uint64_t now_ns_ = 0;
};

} // namespace atom::time

#endif // ATOM_TIME_STEADYTIMESOURCE_HPP
