#include <Algorithm/Math/Scalar.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using namespace atom::algo;
    using atom::test::NearlyEqual;

    ATOM_CHECK_NEAR(ToRadians(180.0f), Pi, 1.0e-5f);
    ATOM_CHECK_NEAR(ToDegrees(Pi), 180.0f, 1.0e-4f);
    ATOM_CHECK_NEAR(Clamp(5.0f, 0.0f, 1.0f), 1.0f, 0.0f);
    ATOM_CHECK_NEAR(Clamp(-2.0f, 0.0f, 1.0f), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(Saturate(1.5f), 1.0f, 0.0f);
    ATOM_CHECK_NEAR(Lerp(0.0f, 10.0f, 0.25f), 2.5f, 1.0e-5f);
    ATOM_CHECK_NEAR(InverseLerp(0.0f, 10.0f, 5.0f), 0.5f, 1.0e-5f);
    ATOM_CHECK_NEAR(InverseLerp(2.0f, 2.0f, 9.0f), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(SmoothStep(0.0f, 1.0f, 0.0f), 0.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(SmoothStep(0.0f, 1.0f, 1.0f), 1.0f, 1.0e-5f);
    ATOM_CHECK(NearlyEqual(1.0f, 1.0f + 1.0e-7f));
    ATOM_CHECK(!NearlyEqual(1.0f, 2.0f));
    return 0;
}
