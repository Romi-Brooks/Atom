#include <Test/Support/TestHelpers.hpp>

#include <Time/DomainClock.hpp>
#include <Time/TimeTypes.hpp>

namespace {

auto VariableClockAdvancesWithRateAndPause() -> int {
    atom::time::ClockDesc desc{};
    desc.rate = 0.5;
    atom::time::DomainClock clock{desc};

    const auto tick = clock.Advance(10'000'000); // 10ms host -> 5ms domain
    ATOM_CHECK(tick.steps == 1);
    ATOM_CHECK(tick.delta_ns == 5'000'000);
    ATOM_CHECK(clock.Now() == 5'000'000ull);
    ATOM_CHECK_NEAR(clock.Alpha(), 0.0f);

    clock.SetPaused(true);
    const auto paused = clock.Advance(10'000'000);
    ATOM_CHECK(paused.steps == 0);
    ATOM_CHECK(clock.Now() == 5'000'000ull);
    return 0;
}

auto FixedClockAccumulatesStepsAndAlpha() -> int {
    atom::time::ClockDesc desc{};
    desc.fixed_step_ns = 10'000'000; // 10ms step
    desc.max_steps_per_advance = 5;
    atom::time::DomainClock clock{desc};

    const auto first = clock.Advance(25'000'000); // 2.5 steps -> 2 steps, alpha 0.5
    ATOM_CHECK(first.steps == 2);
    ATOM_CHECK(first.fixed_step_ns == 10'000'000);
    ATOM_CHECK(clock.Now() == 20'000'000ull);
    ATOM_CHECK_NEAR(clock.Alpha(), 0.5f);

    const auto second = clock.Advance(5'000'000); // leftover 5 + 5 = 10 -> 1 step
    ATOM_CHECK(second.steps == 1);
    ATOM_CHECK(clock.Now() == 30'000'000ull);
    ATOM_CHECK_NEAR(clock.Alpha(), 0.0f);
    return 0;
}

auto FixedClockCapsCatchUpSteps() -> int {
    atom::time::ClockDesc desc{};
    desc.fixed_step_ns = 1'000'000;
    desc.max_steps_per_advance = 3;
    atom::time::DomainClock clock{desc};

    const auto tick = clock.Advance(100'000'000); // huge hitch
    ATOM_CHECK(tick.steps == 3);
    ATOM_CHECK(clock.Now() == 3'000'000ull);
    return 0;
}

auto NegativeOffsetReportsEarlierTime() -> int {
    atom::time::ClockDesc desc{};
    desc.offset_ns = -4'000'000; // 4ms audio latency compensation
    atom::time::DomainClock clock{desc};

    clock.Advance(10'000'000);
    ATOM_CHECK(clock.Now() == 6'000'000ull);
    return 0;
}

auto SyncToPullsTowardDeviceTime() -> int {
    atom::time::DomainClock clock{};
    clock.Advance(10'000'000);
    ATOM_CHECK(clock.Now() == 10'000'000ull);

    clock.SyncTo(20'000'000ull, 0.5);
    ATOM_CHECK(clock.Now() == 15'000'000ull);

    clock.SyncTo(20'000'000ull, 1.0);
    ATOM_CHECK(clock.Now() == 20'000'000ull);
    return 0;
}

} // namespace

auto main() -> int {
    if (const auto code = VariableClockAdvancesWithRateAndPause())
        return code;
    if (const auto code = FixedClockAccumulatesStepsAndAlpha())
        return code;
    if (const auto code = FixedClockCapsCatchUpSteps())
        return code;
    if (const auto code = NegativeOffsetReportsEarlierTime())
        return code;
    if (const auto code = SyncToPullsTowardDeviceTime())
        return code;
    return 0;
}
