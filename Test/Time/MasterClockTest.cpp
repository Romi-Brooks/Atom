#include <Test/Support/TestHelpers.hpp>

#include <Time/MasterClock.hpp>
#include <Time/SteadyTimeSource.hpp>

namespace {

auto MasterClockStartsAtZeroAndTracksDelta() -> int {
    atom::time::ManualTimeSource source;
    source.SetNs(1'000'000'000ull);
    atom::time::MasterClock master{source};

    ATOM_CHECK(master.Now() == 0);
    ATOM_CHECK(master.Delta() == 0);

    source.AdvanceNs(16'000'000ull);
    ATOM_CHECK(master.Now() == 16'000'000ull);

    // First ObserveHost is relative to Reset()/construction baseline.
    const auto first = master.ObserveHost();
    ATOM_CHECK(first == 16'000'000);
    ATOM_CHECK(master.Delta() == 16'000'000);

    source.AdvanceNs(8'000'000ull);
    const auto second = master.ObserveHost();
    ATOM_CHECK(second == 8'000'000);
    ATOM_CHECK(master.Delta() == 8'000'000);
    ATOM_CHECK(master.Now() == 24'000'000ull);
    return 0;
}

auto MasterClockResetReanchorsEpoch() -> int {
    atom::time::ManualTimeSource source;
    atom::time::MasterClock master{source};
    source.AdvanceNs(5'000'000'000ull);
    ATOM_CHECK(master.Now() == 5'000'000'000ull);

    master.Reset();
    ATOM_CHECK(master.Now() == 0);
    source.AdvanceNs(1'000);
    ATOM_CHECK(master.Now() == 1'000);
    return 0;
}

} // namespace

auto main() -> int {
    if (const auto code = MasterClockStartsAtZeroAndTracksDelta())
        return code;
    if (const auto code = MasterClockResetReanchorsEpoch())
        return code;
    return 0;
}
