/**
  * @file           : DomainClock.hpp
  * @author         : Romi Brooks
  * @brief          : Rate/offset/pause derived clock with optional fixed step.
  * @attention      : Create one domain per consumer (game, physics, render,
  *                   audio, ui). Do not share a single Sim clock across
  *                   unrelated subsystems — share pause/rate via SetRate/
  *                   SetPaused on each, or drive them together from outside.
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_TIME_DOMAINCLOCK_HPP
#define ATOM_TIME_DOMAINCLOCK_HPP

#include <Time/TimeTypes.hpp>

namespace atom::time {

class DomainClock {
    public:
        explicit DomainClock(ClockDesc desc = {});

        [[nodiscard]] auto Now() const -> TimePoint;
        [[nodiscard]] auto Delta() const -> Duration;
        [[nodiscard]] auto Alpha() const -> float;
        [[nodiscard]] auto IsPaused() const -> bool;
        [[nodiscard]] auto IsFixedStep() const -> bool;
        [[nodiscard]] auto FixedStep() const -> Duration;
        [[nodiscard]] auto Rate() const -> double;
        [[nodiscard]] auto Offset() const -> Duration;
        [[nodiscard]] auto LastTick() const -> const TickResult&;

        auto SetRate(double rate) -> void;
        auto SetPaused(bool paused) -> void;
        auto SetOffset(Duration offset_ns) -> void;
        auto SetFixedStep(Duration fixed_step_ns, std::uint32_t max_steps_per_advance) -> void;

        // Advance by a host-side delta. Fixed-step domains accumulate and
        // report how many steps to simulate plus an interpolation alpha.
        auto Advance(Duration host_delta_ns) -> TickResult;

        // Soft-PLL toward a device timestamp (audio hardware clock). Adjusts
        // offset so Now() approaches device_now. drift_gain in (0, 1].
        auto SyncTo(TimePoint device_now, double drift_gain = 0.1) -> void;

    private:
        [[nodiscard]] auto Publish(Duration consumed_delta_ns, std::uint32_t steps) -> TickResult;

        ClockDesc desc_{};
        TimePoint now_ns_ = 0;
        Duration accumulator_ns_ = 0;
        Duration last_delta_ns_ = 0;
        float alpha_ = 0.0f;
        TickResult last_tick_{};
};

} // namespace atom::time

#endif // ATOM_TIME_DOMAINCLOCK_HPP
