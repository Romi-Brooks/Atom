// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>

#include <Test/Support/TestHelpers.hpp>
#include <Utilities/Random/Rng.hpp>

auto main() -> int {
    using atom::random::Rng;
    Rng canonical{0};
    ATOM_CHECK(canonical.NextU64() == 0xE220A8397B1DCDAFULL);
    ATOM_CHECK(canonical.NextU64() == 0x6E789E6AA1B965F4ULL);
    ATOM_CHECK(canonical.NextU64() == 0x06C45D188009454FULL);

    Rng parent{2026};
    auto stream = parent.Fork(7);
    const auto first = stream.NextU64();
    (void)parent.NextU64();
    ATOM_CHECK(parent.Fork(7).NextU64() == first);
    ATOM_CHECK(parent.Fork(8).NextU64() != first);

    Rng fixed{0};
    ATOM_CHECK(fixed.Below(0) == 0);
    ATOM_CHECK(fixed.Below(1) == 0);
    ATOM_CHECK(fixed.UniformFloat(2.0f, 2.0f) == 2.0f);
    ATOM_CHECK(fixed.NextU64() == 0xE220A8397B1DCDAFULL);

    const auto adjacent = std::nextafter(1.0f, 2.0f);
    constexpr float largest = std::numeric_limits<float>::max();
    std::array<unsigned, 7> buckets{};
    for (int i = 0; i < 1000; ++i) {
        const auto index = stream.Below(buckets.size());
        ATOM_CHECK(index < buckets.size());
        ++buckets[index];
        const auto sample = stream.UnitFloat();
        ATOM_CHECK(sample >= 0.0f && sample < 1.0f);
        const auto wide = stream.UniformFloat(-largest, largest);
        ATOM_CHECK(std::isfinite(wide) && wide < largest);
        ATOM_CHECK(stream.UniformFloat(1.0f, adjacent) == 1.0f);
    }
    ATOM_CHECK(std::ranges::all_of(buckets, [](unsigned count) { return count != 0; }));

    std::array values{0, 1, 2, 3, 4, 5};
    auto copy = values;
    Rng{3}.Shuffle(std::span{values});
    Rng{3}.Shuffle(std::span{copy});
    ATOM_CHECK(values == copy);
    std::ranges::sort(values);
    ATOM_CHECK((values == std::array{0, 1, 2, 3, 4, 5}));
    return 0;
}
