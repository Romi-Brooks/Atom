/**
  * @file           : DomainClock.cpp
  * @author         : Romi Brooks
  * @brief          : Rate/offset/pause derived clock with optional fixed step.
  * @attention      :
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "DomainClock.hpp"

#include <algorithm>
#include <cmath>

namespace atom::time {

namespace {
[[nodiscard]] auto ClampRate(const double rate) -> double {
    return rate < 0.0 ? 0.0 : rate;
}
} // namespace

DomainClock::DomainClock(const ClockDesc desc) : desc_(desc) {
    desc_.rate = ClampRate(desc_.rate);
    if (desc_.fixed_step_ns < 0)
        desc_.fixed_step_ns = 0;
    if (desc_.fixed_step_ns > 0 && desc_.max_steps_per_advance == 0)
        desc_.max_steps_per_advance = 1;
}

auto DomainClock::Now() const -> TimePoint {
    // Offset may be negative (e.g. audio output latency): report earlier time.
    const auto adjusted = static_cast<Duration>(now_ns_) + desc_.offset_ns;
    return adjusted < 0 ? TimePoint{0} : static_cast<TimePoint>(adjusted);
}

auto DomainClock::Delta() const -> Duration {
    return last_delta_ns_;
}

auto DomainClock::Alpha() const -> float {
    return alpha_;
}

auto DomainClock::IsPaused() const -> bool {
    return desc_.paused;
}

auto DomainClock::IsFixedStep() const -> bool {
    return desc_.fixed_step_ns > 0;
}

auto DomainClock::FixedStep() const -> Duration {
    return desc_.fixed_step_ns;
}

auto DomainClock::Rate() const -> double {
    return desc_.rate;
}

auto DomainClock::Offset() const -> Duration {
    return desc_.offset_ns;
}

auto DomainClock::LastTick() const -> const TickResult& {
    return last_tick_;
}

auto DomainClock::SetRate(const double rate) -> void {
    desc_.rate = ClampRate(rate);
}

auto DomainClock::SetPaused(const bool paused) -> void {
    desc_.paused = paused;
}

auto DomainClock::SetOffset(const Duration offset_ns) -> void {
    desc_.offset_ns = offset_ns;
}

auto DomainClock::SetFixedStep(const Duration fixed_step_ns, const std::uint32_t max_steps_per_advance) -> void {
    desc_.fixed_step_ns = fixed_step_ns < 0 ? 0 : fixed_step_ns;
    desc_.max_steps_per_advance = max_steps_per_advance == 0 ? 1 : max_steps_per_advance;
    accumulator_ns_ = 0;
    alpha_ = 0.0f;
}

auto DomainClock::Advance(const Duration host_delta_ns) -> TickResult {
    if (desc_.paused || host_delta_ns <= 0 || desc_.rate <= 0.0) {
        last_delta_ns_ = 0;
        return Publish(0, 0);
    }

    const auto scaled = static_cast<Duration>(static_cast<double>(host_delta_ns) * desc_.rate);
    if (scaled <= 0) {
        last_delta_ns_ = 0;
        return Publish(0, 0);
    }

    if (!IsFixedStep()) {
        now_ns_ += static_cast<TimePoint>(scaled);
        last_delta_ns_ = scaled;
        return Publish(scaled, 1);
    }

    const auto step = desc_.fixed_step_ns;
    accumulator_ns_ += scaled;
    std::uint32_t steps = 0;
    while (accumulator_ns_ >= step && steps < desc_.max_steps_per_advance) {
        accumulator_ns_ -= step;
        now_ns_ += static_cast<TimePoint>(step);
        ++steps;
    }

    // Spiral-of-death guard: drop backlog beyond one step when capped.
    if (steps == desc_.max_steps_per_advance && accumulator_ns_ >= step)
        accumulator_ns_ %= step;

    alpha_ = step > 0 ? static_cast<float>(static_cast<double>(accumulator_ns_) / static_cast<double>(step)) : 0.0f;
    if (alpha_ < 0.0f)
        alpha_ = 0.0f;
    if (alpha_ >= 1.0f)
        alpha_ = 0.0f;

    const auto consumed = static_cast<Duration>(step) * static_cast<Duration>(steps);
    last_delta_ns_ = consumed;
    return Publish(consumed, steps);
}

auto DomainClock::SyncTo(const TimePoint device_now, const double drift_gain) -> void {
    const auto gain = std::clamp(drift_gain, 0.0, 1.0);
    const auto reported = Now();
    const auto error = static_cast<Duration>(device_now) - static_cast<Duration>(reported);
    desc_.offset_ns += static_cast<Duration>(static_cast<double>(error) * gain);
}

auto DomainClock::Publish(const Duration consumed_delta_ns, const std::uint32_t steps) -> TickResult {
    last_tick_ = TickResult{Now(), consumed_delta_ns, steps, alpha_, desc_.fixed_step_ns};
    return last_tick_;
}

} // namespace atom::time
