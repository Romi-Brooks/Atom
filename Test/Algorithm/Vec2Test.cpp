#include <Algorithm/Math/Scalar.hpp>
#include <Algorithm/Vector/Vec2.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using namespace atom::algo;

    const Vec2 a{3.0f, 4.0f};
    ATOM_CHECK_NEAR(a.LengthSquared(), 25.0f, 0.0f);
    ATOM_CHECK_NEAR(a.Length(), 5.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(a.Normalized().GetX(), 0.6f, 1.0e-5f);
    ATOM_CHECK_NEAR(a.Normalized().GetY(), 0.8f, 1.0e-5f);
    ATOM_CHECK_NEAR(a.Normalized().Length(), 1.0f, 1.0e-5f);

    Vec2 zero = Vec2::Zero();
    ATOM_CHECK(!zero.Normalize());
    ATOM_CHECK(zero.Normalized() == Vec2::Zero());

    ATOM_CHECK_NEAR(Vec2{1.f, 0.f}.Dot(Vec2{0.f, 1.f}), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(Vec2{1.f, 0.f}.Cross(Vec2{0.f, 1.f}), 1.0f, 0.0f);
    ATOM_CHECK((Vec2{1.f, 2.f} + Vec2{3.f, 4.f}) == Vec2{4.f, 6.f});
    ATOM_CHECK((Vec2{3.f, 4.f} - Vec2{1.f, 1.f}) == Vec2{2.f, 3.f});
    ATOM_CHECK((Vec2{2.f, 3.f} * 2.0f) == Vec2{4.f, 6.f});
    ATOM_CHECK((2.0f * Vec2{2.f, 3.f}) == Vec2{4.f, 6.f});
    ATOM_CHECK((Vec2{4.f, 6.f} / 2.0f) == Vec2{2.f, 3.f});
    ATOM_CHECK((-Vec2{1.f, -2.f}) == Vec2{-1.f, 2.f});

    ATOM_CHECK(Vec2::UnitX().Rotated(Pi * 0.5f).GetY() > 0.99f);
    ATOM_CHECK_NEAR(Vec2::Distance(Vec2{0.f, 0.f}, Vec2{3.f, 4.f}), 5.0f, 1.0e-5f);
    ATOM_CHECK(Vec2::Lerp(Vec2{0.f, 0.f}, Vec2{10.f, 0.f}, 0.5f) == Vec2{5.f, 0.f});
    ATOM_CHECK(Vec2::One() == Vec2{1.f, 1.f});
    return 0;
}
