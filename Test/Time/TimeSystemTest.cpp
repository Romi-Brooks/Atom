#include <Test/Support/TestHelpers.hpp>

#include <Time/SteadyTimeSource.hpp>
#include <Time/TimeSystem.hpp>

namespace {

auto DefaultDomainsAndTick() -> int {
    atom::time::ManualTimeSource source;
    source.SetNs(100'000'000ull);

    auto& time_system = atom::time::TimeSystem::GetInstance();
    time_system.Shutdown();
    time_system.Initialize(source);

    ATOM_CHECK(time_system.IsInitialized());
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::kGame) != nullptr);
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::kPhysics) != nullptr);
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::kRender) != nullptr);
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::kUi) != nullptr);
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::kAudio) != nullptr);

    // Game is fixed 60Hz (~16.67ms).
    source.AdvanceNs(16'666'666ull);
    const auto frame1 = time_system.Tick();
    ATOM_CHECK(frame1.host_delta_ns == 16'666'666);

    const auto* game = time_system.FindDomain(atom::time::domain::kGame);
    ATOM_CHECK(game != nullptr);
    ATOM_CHECK(game->IsFixedStep());
    ATOM_CHECK(game->LastTick().steps >= 1);

    // Physics is independent (120Hz) — not the same domain as game.
    const auto* physics = time_system.FindDomain(atom::time::domain::kPhysics);
    ATOM_CHECK(physics != nullptr);
    ATOM_CHECK(physics->FixedStep() == atom::time::kSecond / 120);
    ATOM_CHECK(game->FixedStep() == atom::time::kSecond / 60);

    time_system.Shutdown();
    ATOM_CHECK(!time_system.IsInitialized());
    return 0;
}

auto IndependentDomainsDoNotShareState() -> int {
    atom::time::ManualTimeSource source;
    auto& time_system = atom::time::TimeSystem::GetInstance();
    time_system.Shutdown();
    time_system.Initialize(source);

    auto* physics = time_system.FindDomain(atom::time::domain::kPhysics);
    auto* game = time_system.FindDomain(atom::time::domain::kGame);
    ATOM_CHECK(physics != nullptr && game != nullptr);

    physics->SetPaused(true);
    source.AdvanceNs(50'000'000ull);
    time_system.Tick();

    ATOM_CHECK(physics->LastTick().steps == 0);
    ATOM_CHECK(game->LastTick().steps > 0);

    time_system.Shutdown();
    return 0;
}

} // namespace

auto main() -> int {
    if (const auto code = DefaultDomainsAndTick())
        return code;
    if (const auto code = IndependentDomainsDoNotShareState())
        return code;
    return 0;
}
