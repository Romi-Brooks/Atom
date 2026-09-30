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
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::game) != nullptr);
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::physics) != nullptr);
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::render) != nullptr);
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::ui) != nullptr);
    ATOM_CHECK(time_system.FindDomain(atom::time::domain::audio) != nullptr);

    // Game is fixed 60Hz (~16.67ms).
    source.AdvanceNs(16'666'666ull);
    const auto frame1 = time_system.Tick();
    ATOM_CHECK(frame1.host_delta_ns == 16'666'666);

    const auto* game = time_system.FindDomain(atom::time::domain::game);
    ATOM_CHECK(game != nullptr);
    ATOM_CHECK(game->IsFixedStep());
    ATOM_CHECK(game->LastTick().steps >= 1);

    // Physics is independent (120Hz) — not the same domain as game.
    const auto* physics = time_system.FindDomain(atom::time::domain::physics);
    ATOM_CHECK(physics != nullptr);
    ATOM_CHECK(physics->FixedStep() == atom::time::second / 120);
    ATOM_CHECK(game->FixedStep() == atom::time::second / 60);

    time_system.Shutdown();
    ATOM_CHECK(!time_system.IsInitialized());
    return 0;
}

auto IndependentDomainsDoNotShareState() -> int {
    atom::time::ManualTimeSource source;
    auto& time_system = atom::time::TimeSystem::GetInstance();
    time_system.Shutdown();
    time_system.Initialize(source);

    auto* physics = time_system.FindDomain(atom::time::domain::physics);
    auto* game = time_system.FindDomain(atom::time::domain::game);
    ATOM_CHECK(physics != nullptr && game != nullptr);

    physics->SetPaused(true);
    source.AdvanceNs(50'000'000ull);
    time_system.Tick();

    ATOM_CHECK(physics->LastTick().steps == 0);
    ATOM_CHECK(game->LastTick().steps > 0);

    time_system.Shutdown();
    return 0;
}

auto MasterIsAlwaysDereferenceable() -> int {
    auto& time_system = atom::time::TimeSystem::GetInstance();
    time_system.Shutdown();

    // Shutdown state: TryMaster() reports "not initialized", but Master() must
    // still be dereferenceable and inert (never-null, never-throw contract).
    ATOM_CHECK(time_system.TryMaster() == nullptr);
    ATOM_CHECK(time_system.Master().Now() == 0);
    ATOM_CHECK(time_system.Master().Delta() == 0);

    atom::time::ManualTimeSource source;
    source.SetNs(500'000'000ull);
    time_system.Initialize(source);

    ATOM_CHECK(time_system.TryMaster() != nullptr);
    // Fresh epoch: host time is 500ms, master time starts at zero.
    ATOM_CHECK(time_system.Master().Now() == 0);

    source.AdvanceNs(2'000'000ull);
    ATOM_CHECK(time_system.Master().ObserveHost() == 2'000'000);
    ATOM_CHECK(time_system.Master().Now() == 2'000'000ull);

    // Shutdown must return the accessor to a safe, non-null state again.
    time_system.Shutdown();
    ATOM_CHECK(time_system.TryMaster() == nullptr);
    ATOM_CHECK(time_system.Master().Now() == 0);
    return 0;
}

} // namespace

auto main() -> int {
    if (const auto code = DefaultDomainsAndTick())
        return code;
    if (const auto code = IndependentDomainsDoNotShareState())
        return code;
    if (const auto code = MasterIsAlwaysDereferenceable())
        return code;
    return 0;
}
