#include <Backend/Contracts/Audio/NullAudioBackend.hpp>
#include <Backend/Extension/BackendRegistry.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <memory>

auto main() -> int {
    using namespace atom::backend;

    BackendRegistry registry{};
    ATOM_CHECK(!registry.ContainsAudioBackend("null"));
    ATOM_CHECK(!registry.CreateAudioBackend("null"));

    ATOM_CHECK(registry.RegisterAudioBackend("null", [] { return std::make_unique<atom::audio::NullAudioBackend>(); }));
    ATOM_CHECK(registry.ContainsAudioBackend("null"));
    ATOM_CHECK(registry.ContainsAudioBackend("NULL"));
    ATOM_CHECK(!registry.RegisterAudioBackend("null", [] { return std::make_unique<atom::audio::NullAudioBackend>(); }));

    auto created = registry.CreateAudioBackend("null");
    ATOM_CHECK(created != nullptr);
    ATOM_CHECK(!registry.CreateAudioBackend("missing"));
    return 0;
}
