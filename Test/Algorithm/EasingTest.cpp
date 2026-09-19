#include <Algorithm/Interpolation/Easing.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using namespace atom::algo;
    using namespace atom::algo::easing;

    ATOM_CHECK_NEAR(InCubic(0.0f), 0.0f, 1.0e-6f);
    ATOM_CHECK_NEAR(InCubic(1.0f), 1.0f, 1.0e-6f);
    ATOM_CHECK_NEAR(InCubic(0.5f), 0.125f, 1.0e-6f);
    ATOM_CHECK_NEAR(InCubic(-1.0f), 0.0f, 1.0e-6f);

    ATOM_CHECK_NEAR(OutCubic(0.0f), 0.0f, 1.0e-6f);
    ATOM_CHECK_NEAR(OutCubic(1.0f), 1.0f, 1.0e-6f);
    ATOM_CHECK_NEAR(OutCubic(0.5f), 1.0f - 0.125f, 1.0e-6f);

    ATOM_CHECK_NEAR(OutBack(0.0f), 0.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(OutBack(1.0f), 1.0f, 1.0e-5f);

    ATOM_CHECK_NEAR(InSine(0.0f), 0.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(OutSine(1.0f), 1.0f, 1.0e-5f);
    for (const float t : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        const float in = 1.0f - InSine(t);
        const float out = OutSine(t);
        ATOM_CHECK_NEAR(in * in + out * out, 1.0f, 1.0e-4f);
    }

    ATOM_CHECK_NEAR(IntervalProgress(0.5f, 0.0f, 0.0f), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(IntervalProgress(2.0f, 1.0f, 1.0f), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(IntervalProgress(0.5f, 0.0f, 1.0f), 0.5f, 1.0e-6f);
    ATOM_CHECK_NEAR(IntervalProgress(5.0f, 1.0f, 3.0f), 1.0f, 1.0e-6f);
    ATOM_CHECK_NEAR(IntervalProgress(-1.0f, 0.0f, 2.0f), 0.0f, 1.0e-6f);
    return 0;
}
