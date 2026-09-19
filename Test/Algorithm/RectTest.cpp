#include <Algorithm/Geometry/Rect.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using namespace atom::algo;

    const Rect rect{10.0f, 20.0f, 30.0f, 40.0f};
    ATOM_CHECK(rect.Contains(10.0f, 20.0f));
    ATOM_CHECK(rect.Contains(40.0f, 60.0f));
    ATOM_CHECK(rect.Contains(Vec2{25.0f, 40.0f}));
    ATOM_CHECK(!rect.Contains(9.9f, 20.0f));
    ATOM_CHECK(!rect.Contains(40.1f, 20.0f));

    const auto moved = rect.Translated(5.0f, -5.0f);
    ATOM_CHECK_NEAR(moved.x, 15.0f, 0.0f);
    ATOM_CHECK_NEAR(moved.y, 15.0f, 0.0f);
    ATOM_CHECK_NEAR(moved.width, 30.0f, 0.0f);
    ATOM_CHECK_NEAR(moved.height, 40.0f, 0.0f);

    const auto scaled = rect.ScaledAboutCenter(2.0f);
    ATOM_CHECK_NEAR(scaled.width, 60.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(scaled.height, 80.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(scaled.x + scaled.width * 0.5f, rect.x + rect.width * 0.5f, 1.0e-4f);
    ATOM_CHECK_NEAR(scaled.y + scaled.height * 0.5f, rect.y + rect.height * 0.5f, 1.0e-4f);
    return 0;
}
